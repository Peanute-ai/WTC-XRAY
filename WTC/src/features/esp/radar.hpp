#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include "radar_icons.hpp"

// Este encabezado se incluye desde esp.hpp después de los tipos del SDK y de ImGui.
namespace esp::radar
{
    struct map_view_t
    {
        ImVec2 screen_min{};
        ImVec2 screen_max{};
        vec2_t world_center{ 0.0f, 0.0f }; // ejes X/Z del mundo
        vec2_t world_size{ 0.0f, 0.0f };
        bool valid = false;
    };

    inline bool valid_size(const vec2_t& size)
    {
        return std::isfinite(size.x) && std::isfinite(size.y) &&
            size.x > 0.001f && size.y > 0.001f;
    }

    inline bool valid_center(const vec2_t& center)
    {
        return std::isfinite(center.x) && std::isfinite(center.y);
    }

    inline map_view_t read_map_view()
    {
        map_view_t view{};
        const uintptr_t image = runtime::base_address();
        if (!image || !TargetProcess ||
            TargetProcess->Read<uint8_t>(image + offsets::map_drawn_this_frame_offset) != 1)
            return view; // Puerta de 1 byte: nunca reutilizar la geometría del frame anterior.

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport || viewport->Size.x <= 0.0f || viewport->Size.y <= 0.0f)
            return view;

        const vec2_t pos = TargetProcess->Read<vec2_t>(image + offsets::map_minimap_pos_offset);
        const vec2_t size = TargetProcess->Read<vec2_t>(image + offsets::map_minimap_size_offset);
        if (!valid_center(pos) || !valid_size(size))
            return view;

        // Rectángulo del juego en píxeles relativos al viewport; no escalar ni
        // recortarlo, ya que eso alteraría la transformación mundo→mapa.
        view.screen_min = ImVec2(viewport->Pos.x + pos.x, viewport->Pos.y + pos.y);
        view.screen_max = ImVec2(view.screen_min.x + size.x, view.screen_min.y + size.y);
        const ImVec2 viewport_max(viewport->Pos.x + viewport->Size.x,
            viewport->Pos.y + viewport->Size.y);
        if (size.x < 8.0f || size.y < 8.0f ||
            view.screen_min.x < viewport->Pos.x || view.screen_min.y < viewport->Pos.y ||
            view.screen_max.x > viewport_max.x || view.screen_max.y > viewport_max.y)
            return view;

        const vec2_t full_center = TargetProcess->Read<vec2_t>(image + offsets::map_full_world_center_offset);
        const vec2_t full_size = TargetProcess->Read<vec2_t>(image + offsets::map_full_world_size_offset);
        const vec2_t live_center = TargetProcess->Read<vec2_t>(image + offsets::map_world_center_offset);
        const vec2_t live_size = TargetProcess->Read<vec2_t>(image + offsets::map_view_size_offset);
        if (valid_center(live_center) && valid_size(live_size))
        {
            view.world_center = live_center;
            view.world_size = live_size;
        }
        else if (valid_center(full_center) && valid_size(full_size))
        {
            view.world_center = full_center;
            view.world_size = full_size;
        }
        else
            return view;

        // Bounds tácticos según modo 0..2. aspect != 1 omite el límite del
        // zoom, pero el centro siempre se ciñe a los bounds, incluso degenerados.
        bool tactical_valid = false;
        const uintptr_t tactical = TargetProcess->Read<uintptr_t>(image + offsets::tactical_map_obj_offset);
        if (tactical)
        {
            const uint32_t mode = TargetProcess->Read<uint32_t>(tactical + offsets::tactical_map_offsets::mode);
            if (mode <= 2)
            {
                const uintptr_t shift = static_cast<uintptr_t>(mode) * offsets::tactical_map_offsets::mode_stride;
                const vec2_t bounds_min = TargetProcess->Read<vec2_t>(tactical + offsets::tactical_map_offsets::bounds_min + shift);
                const vec2_t bounds_max = TargetProcess->Read<vec2_t>(tactical + offsets::tactical_map_offsets::bounds_max + shift);
                const float aspect = TargetProcess->Read<float>(tactical + offsets::tactical_map_offsets::aspect);
                if (valid_center(bounds_min) && valid_center(bounds_max) &&
                    bounds_max.x > bounds_min.x && bounds_max.y > bounds_min.y &&
                    std::isfinite(aspect) && aspect > 0.0f)
                {
                    tactical_valid = true;
                    if (std::fabs(aspect - 1.0f) <= 0.0001f)
                    {
                        view.world_size.x = std::fmin(view.world_size.x, bounds_max.x - bounds_min.x);
                        view.world_size.y = std::fmin(view.world_size.y, bounds_max.y - bounds_min.y);
                    }
                    const float half_x = view.world_size.x * 0.5f;
                    const float half_y = view.world_size.y * 0.5f;
                    view.world_center.x = std::fmin(std::fmax(bounds_min.x + half_x, view.world_center.x), bounds_max.x - half_x);
                    view.world_center.y = std::fmin(std::fmax(bounds_min.y + half_y, view.world_center.y), bounds_max.y - half_y);
                }
            }
        }
        if (!tactical_valid)
        {
            if (!valid_center(full_center) || !valid_size(full_size))
                return view;
            view.world_center = full_center;
            view.world_size = full_size;
        }

        view.valid = valid_center(view.world_center) && valid_size(view.world_size);
        return view;
    }

    inline void draw(const std::unordered_map<uintptr_t, vec3_t>& enemy_positions)
    {
        // El latch del juego se pone a 1 y vuelve a 0 dentro del frame del
        // juego. El render del FUSER/DMA NO está sincronizado con ese frame:
        // leerlo una sola vez por frame del overlay provoca parpadeo.
        // Retener la ÚLTIMA vista confirmada durante una ventana breve evita
        // perder los frames intermedios, sin usar indefinidamente globals viejos.
        static map_view_t last_view{};
        static std::chrono::steady_clock::time_point last_seen{};
        static uintptr_t last_image = 0;
        const uintptr_t image = runtime::base_address();
        if (!sdk::radar_enabled || !image || image != last_image)
        {
            last_view = {};
            last_seen = {};
            last_image = image;
            if (!sdk::radar_enabled || !image)
                return;
        }

        const auto now = std::chrono::steady_clock::now();
        map_view_t map = read_map_view(); // El latch se lee cada frame; nunca se cachea su valor.
        if (map.valid)
        {
            last_view = map;
            last_seen = now;
        }
        else
        {
            constexpr auto kMapGrace = std::chrono::milliseconds(250);
            if (!last_view.valid || now - last_seen > kMapGrace)
            {
                last_view = {};
                return;
            }
            map = last_view;
        }
        if (enemy_positions.empty())
            return;

        ImDrawList* list = ImGui::GetForegroundDrawList();
        if (!list)
            return;

        const ImU32 enemy_color = ImGui::ColorConvertFloat4ToU32(sdk::radar_enemy_color);
        const float width = map.screen_max.x - map.screen_min.x;
        const float height = map.screen_max.y - map.screen_min.y;
        list->PushClipRect(map.screen_min, map.screen_max, true);
        for (const auto& [unit_base, pos] : enemy_positions)
        {
            if (!std::isfinite(pos.x) || !std::isfinite(pos.z))
                continue;
            const float x = 0.5f + (pos.x - map.world_center.x) / map.world_size.x;
            const float y = 0.5f - (pos.z - map.world_center.y) / map.world_size.y;
            if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f)
                continue;

            const ImVec2 point(map.screen_min.x + width * x,
                map.screen_min.y + height * y);
            const icons::kind symbol = icons::classify_unit(unit_base, image);
            icons::draw(list, point, symbol, enemy_color);
        }
        list->PopClipRect();
    }
}
