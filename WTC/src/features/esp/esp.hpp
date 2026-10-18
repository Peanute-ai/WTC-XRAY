#pragma once

#define NOMINMAX
#include <Windows.h>

#include <array>
#include <algorithm>
#include <limits>
#include <cmath>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <cstdio>
#include <vector>
#include <cstring>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#define ROW_MAJOR   1

#include "game/classes/movement.hpp"
#include "xray_bvh.hpp"
#include "radar.hpp"
#include "air_lead_prediction.hpp"

struct vec4_t {
    float x, y, z, w;
};

#ifndef VEC3D_T_DEFINED
#define VEC3D_T_DEFINED
struct vec3d_t {
    double x, y, z;
};
#endif

namespace esp
{
    // ========================================================================
    // [SECCIÓN 1] HELPERS DE DIBUJO
    // ========================================================================
    inline void draw_impact_x(const vec2_t& screen_pos, ImU32 color, float size = 8.0f, float thickness = 1.5f)
    {
        g_render->line(screen_pos.x - size, screen_pos.y - size, screen_pos.x + size, screen_pos.y + size, color, thickness);
        g_render->line(screen_pos.x + size, screen_pos.y - size, screen_pos.x - size, screen_pos.y + size, color, thickness);
    }

    // ========================================================================
    // [SECCIÓN 2] CONTEXTO DE PROYECCIÓN
    // ========================================================================
    struct projection_context_t
    {
        vec3_t camera_pos{};
        double bias_clip_x = 0.0;
        double bias_clip_y = 0.0;
        double bias_clip_w = 0.0;
        bool valid = false;
    };

    // ========================================================================
    // [SECCIÓN 3] CACHÉS DE UNIDAD
    // ========================================================================
    struct unit_cached_info_t
    {
        std::string unit_type;
        std::string vehicle_name;
        uint16_t raw_type = 0;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    struct unit_type_info_t
    {
        ImU32       color;
        bool        worth_to_draw;
        bool        is_dummy;
        bool        is_plane;
        bool        show_reload;
        bool        draw_box;
    };

    struct unit_box_cache_t
    {
        vec3_t bbmin{};
        vec3_t bbmax{};
        rotation_matrix_t rot{};
        std::chrono::steady_clock::time_point next_bounds_refresh{};
        std::chrono::steady_clock::time_point next_rot_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    struct unit_reload_cache_t
    {
        uint8_t value = 0;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    // ========================================================================
    // [SECCIÓN 4] CLASIFICACIÓN DE UNIDADES
    // ========================================================================
    enum class UnitClass {
        Unknown,
        Air,
        Ground,
        Naval
    };

    inline UnitClass classify_unit_type(uint16_t raw_type) {
        switch (raw_type) {
        case 0: return UnitClass::Air;
        case 3: return UnitClass::Ground;
        case 5: return UnitClass::Naval;
        default: return UnitClass::Unknown;
        }
    }

    inline unit_type_info_t get_unit_type_info(std::string_view unit_type, uint16_t raw_type)
    {
        UnitClass cls = classify_unit_type(raw_type);

        if (cls == UnitClass::Air) {
            if (unit_type == "exp_helicopter")       return { ImGui::ColorConvertFloat4ToU32(sdk::color_helicopter), sdk::show_helicopter, false, true, true, false };
            if (unit_type == "exp_bomber")           return { ImGui::ColorConvertFloat4ToU32(sdk::color_bomber), sdk::show_bomber, false, true, false, false };
            if (unit_type == "exp_fighter")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_fighter), sdk::show_fighter, false, true, false, false };
            if (unit_type == "exp_assault")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_attacker), sdk::show_attacker, false, true, false, false };
            if (unit_type == "exp_recon_drone")      return { ImGui::ColorConvertFloat4ToU32(sdk::color_recon_drone), sdk::show_recon_drone, false, true, false, false };
            return { ImGui::ColorConvertFloat4ToU32(sdk::color_fighter), true, false, true, false, false };
        }

        if (cls == UnitClass::Ground) {
            if (unit_type == "exp_tank")             return { ImGui::ColorConvertFloat4ToU32(sdk::color_light_tank), sdk::show_light_tank, false, false, true, true };
            if (unit_type == "exp_heavy_tank")       return { ImGui::ColorConvertFloat4ToU32(sdk::color_heavy_tank), sdk::show_heavy_tank, false, false, true, true };
            if (unit_type == "exp_tank_destroyer")   return { ImGui::ColorConvertFloat4ToU32(sdk::color_medium_tank), sdk::show_medium_tank, false, false, true, true };
            if (unit_type == "exp_SPAA")             return { ImGui::ColorConvertFloat4ToU32(sdk::color_spaa), sdk::show_spaa, false, false, true, true };
            return { ImGui::ColorConvertFloat4ToU32(sdk::color_light_tank), true, false, false, true, true };
        }

        if (cls == UnitClass::Naval) {
            if (unit_type == "exp_gun_boat")         return { ImGui::ColorConvertFloat4ToU32(sdk::color_gun_boat), sdk::show_gun_boat, false, false, false, false };
            if (unit_type == "exp_destroyer")        return { ImGui::ColorConvertFloat4ToU32(sdk::color_destroyer), sdk::show_destroyer, false, false, false, false };
            if (unit_type == "exp_cruiser")          return { ImGui::ColorConvertFloat4ToU32(sdk::color_cruiser), sdk::show_cruiser, false, false, false, false };
            if (unit_type == "exp_torpedo_gun_boat") return { ImGui::ColorConvertFloat4ToU32(sdk::color_torpedo_boat), sdk::show_torpedo_boat, false, false, false, false };
            return { IM_COL32(255, 255, 255, 255), false, false, false, false, false };
        }

        if (unit_type == "exp_fortification" ||
            unit_type == "exp_structure" ||
            unit_type == "exp_aaa" ||
            unit_type == "dummy_plane" ||
            unit_type == "exp_bridge" ||
            unit_type == "dummy_tank" ||
            unit_type == "dummy_immortal" ||
            unit_type == "structures/dummy_immortal" ||
            unit_type == "ai_bot")
        {
            return { IM_COL32(255, 255, 255, 255), false, true, false, false, false };
        }

        return { IM_COL32(255, 255, 255, 255), false, false, false, true, false };
    }

    inline bool is_air_lead_target(std::string_view unit_type, const unit_type_info_t& info)
    {
        if (!info.is_plane) return false;
        if (unit_type == "exp_helicopter")      return sdk::air_lead_target_helicopter;
        if (unit_type == "exp_bomber")          return sdk::air_lead_target_bomber;
        if (unit_type == "exp_fighter")         return sdk::air_lead_target_fighter;
        if (unit_type == "exp_assault")         return sdk::air_lead_target_attacker;
        if (unit_type == "exp_recon_drone")     return sdk::air_lead_target_recon_drone;
        return false;
    }

    // ========================================================================
    // [SECCIÓN 5] CACHÉS
    // ========================================================================
    inline unit_cached_info_t& get_cached_unit_info(const uintptr_t unit_base, c_unit& unit)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_cached_info_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];

        if (now >= entry.next_refresh)
        {
            auto info = unit.getInfo();
            entry.unit_type = info.getUnitType();
            entry.vehicle_name = info.getVehicleName();
            entry.raw_type = TargetProcess->Read<uint16_t>(
                unit_base + offsets::unit_offsets::unitType_offset);
            entry.next_refresh = now + std::chrono::milliseconds(1500);
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    inline unit_box_cache_t& get_cached_box_info(const uintptr_t unit_base, c_unit& unit, const float distance_m)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_box_cache_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];
        if (now >= entry.next_bounds_refresh)
        {
            entry.bbmin = unit.getBBMin();
            entry.bbmax = unit.getBBMax();
            entry.next_bounds_refresh = now + std::chrono::milliseconds(1000);
        }

        if (now >= entry.next_rot_refresh)
        {
            entry.rot = unit.getRotationMatrix();
            const auto rot_ttl = (distance_m <= 120.0f)
                ? std::chrono::milliseconds(16)
                : std::chrono::milliseconds(33);
            entry.next_rot_refresh = now + rot_ttl;
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    inline unit_reload_cache_t& get_cached_reload_info(const uintptr_t unit_base, c_unit& unit)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, unit_reload_cache_t> cache;
        static auto next_gc = clock::now() + std::chrono::seconds(5);

        const auto now = clock::now();
        auto& entry = cache[unit_base];
        if (now >= entry.next_refresh)
        {
            entry.value = unit.getReloadTime();
            entry.next_refresh = now + std::chrono::milliseconds(80);
        }
        entry.last_seen = now;

        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end(); )
            {
                if (now - it->second.last_seen > std::chrono::seconds(10))
                    it = cache.erase(it);
                else
                    ++it;
            }
            next_gc = now + std::chrono::seconds(5);
        }

        return entry;
    }

    // ========================================================================
    // [SECCIÓN 6] GEOMETRÍA Y PROYECCIÓN
    // ========================================================================
    inline std::array<vec3_t, 8> calculate_bbox_corners_rotated(
        const vec3_t& position,
        const vec3_t& bbmin,
        const vec3_t& bbmax,
        const rotation_matrix_t& rot)
    {
        std::array<vec3_t, 8> local = { {
            { bbmin.x, bbmin.y, bbmin.z },
            { bbmax.x, bbmin.y, bbmin.z },
            { bbmin.x, bbmax.y, bbmin.z },
            { bbmax.x, bbmax.y, bbmin.z },
            { bbmin.x, bbmin.y, bbmax.z },
            { bbmax.x, bbmin.y, bbmax.z },
            { bbmin.x, bbmax.y, bbmax.z },
            { bbmax.x, bbmax.y, bbmax.z }
        } };

        const vec3_t center{
            (bbmin.x + bbmax.x) * 0.5f,
            (bbmin.y + bbmax.y) * 0.5f,
            (bbmin.z + bbmax.z) * 0.5f
        };

        std::array<vec3_t, 8> world{};

        for (size_t i = 0; i < 8; ++i)
        {
            const vec3_t offset{
                local[i].x - center.x,
                local[i].y - center.y,
                local[i].z - center.z
            };

            const vec3_t rotated = rot.transform(offset);

            world[i] = {
                position.x + center.x + rotated.x,
                position.y + center.y + rotated.y,
                position.z + center.z + rotated.z
            };
        }

        return world;
    }

    inline void draw_wireframe_box(
        const std::array<vec2_t, 8>& c,
        ImU32 color,
        float thickness)
    {
        g_render->line(c[0].x, c[0].y, c[1].x, c[1].y, color, thickness);
        g_render->line(c[1].x, c[1].y, c[3].x, c[3].y, color, thickness);
        g_render->line(c[3].x, c[3].y, c[2].x, c[2].y, color, thickness);
        g_render->line(c[2].x, c[2].y, c[0].x, c[0].y, color, thickness);

        g_render->line(c[4].x, c[4].y, c[5].x, c[5].y, color, thickness);
        g_render->line(c[5].x, c[5].y, c[7].x, c[7].y, color, thickness);
        g_render->line(c[7].x, c[7].y, c[6].x, c[6].y, color, thickness);
        g_render->line(c[6].x, c[6].y, c[4].x, c[4].y, color, thickness);

        g_render->line(c[0].x, c[0].y, c[4].x, c[4].y, color, thickness);
        g_render->line(c[1].x, c[1].y, c[5].x, c[5].y, color, thickness);
        g_render->line(c[2].x, c[2].y, c[6].x, c[6].y, color, thickness);
        g_render->line(c[3].x, c[3].y, c[7].x, c[7].y, color, thickness);
    }

    inline bool world_to_screen_counted(
        const vec3_t& point,
        vec2_t& out,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t&,
        uint32_t& w2s_counter)
    {
        ++w2s_counter;

        const ViewMatrix_t& mvp = camera_matrix;

        vec4_t clip;

#if defined(ROW_MAJOR)
        clip.x = point.x * mvp[0][0] + point.y * mvp[1][0] + point.z * mvp[2][0] + mvp[3][0];
        clip.y = point.x * mvp[0][1] + point.y * mvp[1][1] + point.z * mvp[2][1] + mvp[3][1];
        clip.z = point.x * mvp[0][2] + point.y * mvp[1][2] + point.z * mvp[2][2] + mvp[3][2];
        clip.w = point.x * mvp[0][3] + point.y * mvp[1][3] + point.z * mvp[2][3] + mvp[3][3];
#elif defined(COL_MAJOR)
        clip.x = mvp[0][0] * point.x + mvp[0][1] * point.y + mvp[0][2] * point.z + mvp[0][3];
        clip.y = mvp[1][0] * point.x + mvp[1][1] * point.y + mvp[1][2] * point.z + mvp[1][3];
        clip.z = mvp[2][0] * point.x + mvp[2][1] * point.y + mvp[2][2] * point.z + mvp[2][3];
        clip.w = mvp[3][0] * point.x + mvp[3][1] * point.y + mvp[3][2] * point.z + mvp[3][3];
#endif

        if (clip.w < 0.01f)
            return false;

        static int cached_frame = -1;
        static ImVec2 viewport_pos{};
        static ImVec2 viewport_half_size{};

        const int frame = ImGui::GetFrameCount();
        if (frame != cached_frame)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            if (!viewport)
                return false;

            viewport_pos = viewport->Pos;
            viewport_half_size = { viewport->Size.x * 0.5f, viewport->Size.y * 0.5f };
            cached_frame = frame;
        }

        if (viewport_half_size.x <= 0.0f || viewport_half_size.y <= 0.0f)
            return false;

        const float inv_w = 1.0f / clip.w;
        const float ndc_x = clip.x * inv_w;
        const float ndc_y = clip.y * inv_w;

        out.x = viewport_pos.x + (ndc_x + 1.0f) * viewport_half_size.x;
        out.y = viewport_pos.y + (1.0f - ndc_y) * viewport_half_size.y;

        return true;
    }

    inline bool is_point_in_clip_frustum(
        const vec3_t& point,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        const float margin = 0.2f)
    {
        const float legacy_clip_x =
            (point.x * camera_matrix[0][0]) +
            (point.y * camera_matrix[1][0]) +
            (point.z * camera_matrix[2][0]) +
            camera_matrix[3][0];
        const float legacy_clip_y =
            (point.x * camera_matrix[0][1]) +
            (point.y * camera_matrix[1][1]) +
            (point.z * camera_matrix[2][1]) +
            camera_matrix[3][1];
        const float legacy_clip_w =
            (point.x * camera_matrix[0][3]) +
            (point.y * camera_matrix[1][3]) +
            (point.z * camera_matrix[2][3]) +
            camera_matrix[3][3];

        const float legacy_limit = 1.0f + margin;
        const bool legacy_visible = legacy_clip_w > 0.001f &&
            std::fabs(legacy_clip_x / legacy_clip_w) <= legacy_limit &&
            std::fabs(legacy_clip_y / legacy_clip_w) <= legacy_limit;

        if (!projection_ctx.valid)
            return legacy_visible;

        const double local_x = static_cast<double>(point.x) - static_cast<double>(projection_ctx.camera_pos.x);
        const double local_y = static_cast<double>(point.y) - static_cast<double>(projection_ctx.camera_pos.y);
        const double local_z = static_cast<double>(point.z) - static_cast<double>(projection_ctx.camera_pos.z);

        const double clip_x =
            local_x * static_cast<double>(camera_matrix[0][0]) +
            local_y * static_cast<double>(camera_matrix[1][0]) +
            local_z * static_cast<double>(camera_matrix[2][0]) +
            projection_ctx.bias_clip_x;

        const double clip_y =
            local_x * static_cast<double>(camera_matrix[0][1]) +
            local_y * static_cast<double>(camera_matrix[1][1]) +
            local_z * static_cast<double>(camera_matrix[2][1]) +
            projection_ctx.bias_clip_y;

        const double clip_w =
            local_x * static_cast<double>(camera_matrix[0][3]) +
            local_y * static_cast<double>(camera_matrix[1][3]) +
            local_z * static_cast<double>(camera_matrix[2][3]) +
            projection_ctx.bias_clip_w;

        if (clip_w <= 0.001f)
            return legacy_visible;

        const double inv_w = 1.0 / clip_w;
        const double ndc_x = clip_x * inv_w;
        const double ndc_y = clip_y * inv_w;
        const double limit = 1.0 + static_cast<double>(margin);
        const bool precise_visible = std::fabs(ndc_x) <= limit && std::fabs(ndc_y) <= limit;
        return precise_visible || legacy_visible;
    }

    inline bool draw_cheap_2d_box(
        const vec3_t& unit_pos,
        const vec3_t& bbmin,
        const vec3_t& bbmax,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        ImU32 color,
        float* out_max_y = nullptr)
    {
        vec2_t top_sp{};
        vec2_t bottom_sp{};
        vec2_t side_sp{};
        vec2_t center_sp{};

        const float center_y = (bbmin.y + bbmax.y) * 0.5f;
        const float half_width_world = std::max(std::fabs(bbmin.x), std::fabs(bbmax.x));

        const vec3_t top_world{ unit_pos.x, unit_pos.y + bbmax.y, unit_pos.z };
        const vec3_t bottom_world{ unit_pos.x, unit_pos.y + bbmin.y, unit_pos.z };
        const vec3_t side_world{ unit_pos.x + half_width_world, unit_pos.y + center_y, unit_pos.z };
        const vec3_t center_world{ unit_pos.x, unit_pos.y + center_y, unit_pos.z };

        if (!world_to_screen_counted(top_world, top_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(bottom_world, bottom_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(side_world, side_sp, camera_matrix, projection_ctx, w2s_counter) ||
            !world_to_screen_counted(center_world, center_sp, camera_matrix, projection_ctx, w2s_counter))
        {
            return false;
        }

        const float height = std::fabs(bottom_sp.y - top_sp.y);
        if (height < 2.0f)
            return false;

        float half_width = std::fabs(side_sp.x - center_sp.x);
        if (half_width < 1.0f)
            half_width = std::max(2.0f, height * 0.28f);

        const float min_x = center_sp.x - half_width;
        const float max_x = center_sp.x + half_width;
        const float min_y = std::min(top_sp.y, bottom_sp.y);
        const float max_y = std::max(top_sp.y, bottom_sp.y);

        g_render->rect(min_x, min_y, max_x - min_x, max_y - min_y, color, 0.0f);
        if (out_max_y)
            *out_max_y = max_y;
        return true;
    }

    struct projected_box_bounds_t
    {
        float min_x = 0.0f;
        float min_y = 0.0f;
        float max_x = 0.0f;
        float max_y = 0.0f;
        bool valid = false;
    };

    inline auto get_projected_box_bounds(
        c_unit& unit,
        const vec3_t& unit_pos,
        const float distance_f,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        uint32_t& obb_counter) -> projected_box_bounds_t
    {
        projected_box_bounds_t bounds{};
        auto& box_info = get_cached_box_info(unit.get_base(), unit, distance_f);
        ++obb_counter;
        const auto world_corners = calculate_bbox_corners_rotated(unit_pos, box_info.bbmin, box_info.bbmax, box_info.rot);

        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();

        bool any_visible = false;
        for (const auto& corner : world_corners)
        {
            vec2_t sp;
            if (!world_to_screen_counted(corner, sp, camera_matrix, projection_ctx, w2s_counter))
                continue;

            any_visible = true;

            min_x = std::min(min_x, sp.x);
            min_y = std::min(min_y, sp.y);
            max_x = std::max(max_x, sp.x);
            max_y = std::max(max_y, sp.y);
        }

        if (!any_visible || max_x <= min_x || max_y <= min_y)
            return bounds;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (viewport)
        {
            const float view_min_x = viewport->Pos.x;
            const float view_min_y = viewport->Pos.y;
            const float view_max_x = viewport->Pos.x + viewport->Size.x;
            const float view_max_y = viewport->Pos.y + viewport->Size.y;

            min_x = std::clamp(min_x, view_min_x, view_max_x);
            max_x = std::clamp(max_x, view_min_x, view_max_x);
            min_y = std::clamp(min_y, view_min_y, view_max_y);
            max_y = std::clamp(max_y, view_min_y, view_max_y);
        }

        bounds.min_x = min_x;
        bounds.min_y = min_y;
        bounds.max_x = max_x;
        bounds.max_y = max_y;
        bounds.valid = true;
        return bounds;
    }

    inline bool draw_projected_2d_box_from_obb(
        c_unit& unit,
        const vec3_t& unit_pos,
        const float distance_f,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& projection_ctx,
        uint32_t& w2s_counter,
        uint32_t& obb_counter,
        ImU32 color,
        float* out_max_y = nullptr)
    {
        const auto bounds = get_projected_box_bounds(unit, unit_pos, distance_f, camera_matrix, projection_ctx, w2s_counter, obb_counter);
        if (!bounds.valid)
            return false;

        g_render->rect(bounds.min_x, bounds.min_y, bounds.max_x - bounds.min_x, bounds.max_y - bounds.min_y, color, 0.0f);
        if (out_max_y)
            *out_max_y = bounds.max_y;
        return true;
    }

    // ========================================================================
    // [SECCIÓN 7] X-RAY — PACKED BVH: TRANSFORM + AXIS CONVERSION
    // ========================================================================
    enum xform_layout_t
    {
        XF_RM = 0,
        XF_CM,
        XF_CM3,
        XF_RM3,
        XF_COUNT
    };

    struct part_xform_t
    {
        float          f[16] = {};
        xform_layout_t layout = XF_RM3;
        bool           valid = false;
    };

    struct xform_scheme_t
    {
        uintptr_t      stride = 0x30;
        xform_layout_t layout = XF_RM3;
        int            clean = -1;
        int            tested = 0;
    };

    inline const char* xform_layout_name(xform_layout_t l)
    {
        switch (l)
        {
        case XF_RM:  return "4x4-row";
        case XF_CM:  return "4x4-col";
        case XF_CM3: return "3x4-col(TMatrix)";
        case XF_RM3: return "3x4-row(TMatrix)";
        default:     return "?";
        }
    }

    inline float orthonorm_score(const vec3_t& b0, const vec3_t& b1, const vec3_t& b2)
    {
        return std::fabs(b0.length() - 1.0f) + std::fabs(b1.length() - 1.0f) + std::fabs(b2.length() - 1.0f)
            + std::fabs(b0.dot(b1)) + std::fabs(b1.dot(b2)) + std::fabs(b2.dot(b0));
    }

    inline void basis_from(const float* f, xform_layout_t layout, vec3_t& b0, vec3_t& b1, vec3_t& b2)
    {
        switch (layout)
        {
        case XF_RM:
            b0 = vec3_t(f[0], f[1], f[2]);
            b1 = vec3_t(f[4], f[5], f[6]);
            b2 = vec3_t(f[8], f[9], f[10]);
            break;
        case XF_CM:
            b0 = vec3_t(f[0], f[4], f[8]);
            b1 = vec3_t(f[1], f[5], f[9]);
            b2 = vec3_t(f[2], f[6], f[10]);
            break;
        case XF_CM3:
        case XF_RM3:
        default:
            b0 = vec3_t(f[0], f[1], f[2]);
            b1 = vec3_t(f[3], f[4], f[5]);
            b2 = vec3_t(f[6], f[7], f[8]);
            break;
        }
    }

    inline vec3_t apply_xform(const float* f, xform_layout_t layout, const vec3_t& v)
    {
        switch (layout)
        {
        case XF_RM:
            return vec3_t{
                v.x * f[0] + v.y * f[1] + v.z * f[2] + f[3],
                v.x * f[4] + v.y * f[5] + v.z * f[6] + f[7],
                v.x * f[8] + v.y * f[9] + v.z * f[10] + f[11] };
        case XF_CM:
            return vec3_t{
                v.x * f[0] + v.y * f[4] + v.z * f[8] + f[12],
                v.x * f[1] + v.y * f[5] + v.z * f[9] + f[13],
                v.x * f[2] + v.y * f[6] + v.z * f[10] + f[14] };
        case XF_CM3:
            return vec3_t{
                v.x * f[0] + v.y * f[3] + v.z * f[6] + f[9],
                v.x * f[1] + v.y * f[4] + v.z * f[7] + f[10],
                v.x * f[2] + v.y * f[5] + v.z * f[8] + f[11] };
        case XF_RM3:
        default:
            return vec3_t{
                v.x * f[0] + v.y * f[1] + v.z * f[2] + f[9],
                v.x * f[3] + v.y * f[4] + v.z * f[5] + f[10],
                v.x * f[6] + v.y * f[7] + v.z * f[8] + f[11] };
        }
    }

    inline vec3_t bvh_vertex_to_local_game(const vec3_t& v_bvh, const part_xform_t* xf)
    {
        float vx = v_bvh.x, vy = v_bvh.y, vz = v_bvh.z;

        switch (sdk::xray_axis_fix_mode)
        {
        case 0:  vx = -v_bvh.y; vy = v_bvh.x; vz = v_bvh.z; break;
        case 1:  vx = v_bvh.x; vy = v_bvh.z; vz = -v_bvh.y; break;
        case 2:  vx = v_bvh.y; vy = -v_bvh.x; vz = v_bvh.z; break;
        default: break;
        }

        if (xf && xf->valid)
            return apply_xform(xf->f, xf->layout, vec3_t{ vx, vy, vz });

        return vec3_t{ vx, vy, vz };
    }

    inline bool read_part_transform(uintptr_t matrix_array_base, uint16_t transform_index, part_xform_t& out)
    {
        out.valid = false;
        if (!matrix_array_base) return false;
        if (transform_index == 0xFFFFu || transform_index >= 4096u) return false;

        const uintptr_t addr = matrix_array_base + static_cast<uintptr_t>(transform_index) * 0x30;
        if (!TargetProcess->Read(addr, out.f, 12 * sizeof(float))) return false;

        out.layout = XF_CM3;
        out.valid = true;
        return true;
    }

    // ========================================================================
    // [SECCIÓN 7c] X-RAY — SKELETON TREE (animado)
    // ========================================================================
    struct unit_skeleton_ctx_t
    {
        uintptr_t dm = 0;
        uint8_t   valid_flag = 0;
        uintptr_t tree = 0;
        uintptr_t packed = 0;
        uint16_t  tree_count = 0;
        bool      animated_ok = false;
    };

    inline unit_skeleton_ctx_t read_unit_skeleton_ctx(uintptr_t unit_base)
    {
        unit_skeleton_ctx_t ctx{};
        if (!unit_base) return ctx;

        ctx.dm = TargetProcess->Read<uintptr_t>(
            unit_base + offsets::unit_offsets::damageModelCont_offset);
        if (!ctx.dm) return ctx;

        ctx.valid_flag = TargetProcess->Read<uint8_t>(
            ctx.dm + offsets::damage_model::m_ValidFlag);
        if (ctx.valid_flag != 1) return ctx;

        ctx.tree = TargetProcess->Read<uintptr_t>(
            unit_base + offsets::unit_offsets::modelTransform_offset);
        if (!ctx.tree) return ctx;

        ctx.packed = TargetProcess->Read<uintptr_t>(
            ctx.tree + offsets::damage_model::geom_node_tree_base);
        ctx.tree_count = TargetProcess->Read<uint16_t>(
            ctx.tree + offsets::damage_model::geom_node_tree_count);

        ctx.animated_ok = (ctx.packed != 0 && ctx.tree_count > 0);
        return ctx;
    }

    inline bool read_animated_matrix(const unit_skeleton_ctx_t& ctx,
        uint16_t tree_index,
        part_xform_t& out)
    {
        out.valid = false;
        if (!ctx.animated_ok) return false;
        if (tree_index == 0xFFFF) return false;
        if (tree_index >= ctx.tree_count) return false;

        const uintptr_t addr = ctx.packed + static_cast<uintptr_t>(tree_index) * 0x40;
        if (!TargetProcess->Read(addr, out.f, 16 * sizeof(float))) return false;

        out.layout = XF_CM;
        out.valid = true;

        return true;
    }

    // ========================================================================
    // [SECCIÓN 7d] CLASIFICACIÓN CACHEADA DE PARTS
    // ========================================================================
    using xray_part_kind_t = xray::xray_part_kind_t;

    // ========================================================================
    // [SECCIÓN 8] X-RAY — BATCHING Y DIBUJO
    // ========================================================================

    struct FillBatch
    {
        std::vector<ImDrawVert> verts;
        std::vector<ImDrawIdx>  indices;
    };
    using FillBatchMap = std::unordered_map<ImU32, FillBatch>;

    inline void flush_fill_batches(ImDrawList* draw_list, FillBatchMap& batches)
    {
        if (!draw_list) return;

        for (auto& [kind, batch] : batches)
        {
            const int vtx_count = static_cast<int>(batch.verts.size());
            const int idx_count = static_cast<int>(batch.indices.size());
            if (vtx_count == 0 || idx_count == 0)
                continue;

            const ImDrawIdx base = static_cast<ImDrawIdx>(draw_list->VtxBuffer.Size);
            draw_list->PrimReserve(idx_count, vtx_count);

            std::memcpy(draw_list->_VtxWritePtr, batch.verts.data(),
                vtx_count * sizeof(ImDrawVert));

            for (int i = 0; i < idx_count; ++i)
                draw_list->_IdxWritePtr[i] = base + batch.indices[i];

            draw_list->_VtxWritePtr = draw_list->VtxBuffer.Data + draw_list->VtxBuffer.Size;
            draw_list->_IdxWritePtr = draw_list->IdxBuffer.Data + draw_list->IdxBuffer.Size;
        }
    }

    // ------------------------------------------------------------------------
    // [SECCIÓN 8a] Proyección de un part a buffers reutilizables.
    // NO proyecta para dibujar: solo llena out_projected / out_visible.
    // ------------------------------------------------------------------------
    inline size_t project_part_vertices_to(
        const xray::XrayPart& part,
        const part_xform_t* part_transform,
        const rotation_matrix_t& unit_rot,
        const vec3_t& unit_pos,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,
        uint32_t& w2s_counter,
        std::vector<vec2_t>& out_projected,
        std::vector<uint8_t>& out_visible)
    {
        const size_t n = part.vertices.size();
        out_projected.resize(n);
        out_visible.assign(n, 0);

        size_t visible_count = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const vec3_t v_local = bvh_vertex_to_local_game(part.vertices[i], part_transform);
            const vec3_t p_world = unit_rot.transform(v_local) + unit_pos;
            if (world_to_screen_counted(p_world, out_projected[i], camera_matrix, proj_ctx, w2s_counter))
            {
                out_visible[i] = 1;
                ++visible_count;
            }
        }
        return visible_count;
    }

    // ------------------------------------------------------------------------
    // [SECCIÓN 8b] Dibujo de wireframe desde buffers ya proyectados.
    // Reutiliza el resultado de project_part_vertices_to sin re-proyectar.
    // ------------------------------------------------------------------------
    inline void draw_lines_from_projected(
        const std::vector<uint32_t>& indices,
        const std::vector<vec2_t>& projected,
        const std::vector<uint8_t>& visible,
        ImDrawList* draw_list,
        ImU32 color,
        float thickness)
    {
        if (!draw_list) return;
        const size_t n = projected.size();
        if (n == 0) return;

        const size_t tri_count = indices.size() / 3;
        for (size_t t = 0; t < tri_count; ++t)
        {
            const uint32_t ia = indices[t * 3 + 0];
            const uint32_t ib = indices[t * 3 + 1];
            const uint32_t ic = indices[t * 3 + 2];
            if (ia >= n || ib >= n || ic >= n) continue;
            if (!visible[ia] || !visible[ib] || !visible[ic]) continue;

            const ImVec2 a(projected[ia].x, projected[ia].y);
            const ImVec2 b(projected[ib].x, projected[ib].y);
            const ImVec2 c(projected[ic].x, projected[ic].y);

            draw_list->AddLine(a, b, color, thickness);
            draw_list->AddLine(b, c, color, thickness);
            draw_list->AddLine(c, a, color, thickness);
        }
    }

    // ------------------------------------------------------------------------
    // [SECCIÓN 8c] Wrapper wireframe puro (solo para xray_render_mode == 0).
    // Usa buffers thread_local internos.
    // ------------------------------------------------------------------------
    inline void draw_module_lines_world(
        const xray::XrayPart& part,
        const part_xform_t* part_transform,
        const rotation_matrix_t& unit_rot,
        const vec3_t& unit_pos,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,
        uint32_t& w2s_counter,
        ImU32 color,
        float thickness)
    {
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        if (!draw_list) draw_list = ImGui::GetForegroundDrawList();
        if (!draw_list) return;

        thread_local std::vector<vec2_t>  s_wire_projected;
        thread_local std::vector<uint8_t> s_wire_visible;

        project_part_vertices_to(part, part_transform, unit_rot, unit_pos,
            camera_matrix, proj_ctx, w2s_counter, s_wire_projected, s_wire_visible);

        draw_lines_from_projected(part.indices, s_wire_projected, s_wire_visible,
            draw_list, color, thickness);
    }

    // ------------------------------------------------------------------------
    // [SECCIÓN 8d] Draw X-Ray modules (con todas las optimizaciones).
    // ------------------------------------------------------------------------
    inline void draw_xray_modules(
        c_unit& unit,
        const vec3_t& unit_pos,
        const ViewMatrix_t& camera_matrix,
        const projection_context_t& proj_ctx,
        uint32_t& w2s_counter,
        float distance_f)
    {
        const uintptr_t unit_base = unit.get_base();
        rotation_matrix_t unit_rot = unit.getRotationMatrix();

        auto& cache = xray::get_xray_cache(unit_base);

        if (cache.parts.empty())
            return;

        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        if (!draw_list) draw_list = ImGui::GetForegroundDrawList();
        if (!draw_list) return;

        // ====================================================================
        // CONTEXTO LEÍDO UNA SOLA VEZ POR UNIT/FRAME
        // ====================================================================
        const unit_skeleton_ctx_t skel = read_unit_skeleton_ctx(unit_base);

        // ====================================================================
        // VIEWPORT + MARGEN PARA CULLING 2D
        // ====================================================================
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float vp_w = vp ? vp->Size.x : 1920.0f;
        const float vp_h = vp ? vp->Size.y : 1080.0f;
        const float vp_margin = 200.0f;

        // Key by module kind (at most seven), NOT animated color/alpha.
        // unordered_map::clear destroys nested vectors; clear their sizes instead.
        // Retain geometry capacity without accumulating keys as distance/colors change.
        thread_local FillBatchMap fill_batches;
        for (auto& entry : fill_batches) {
            entry.second.verts.clear();
            entry.second.indices.clear();
        }

        // Buffers reusables por thread para el pase de proyección.
        thread_local std::vector<vec2_t>  s_projected;
        thread_local std::vector<uint8_t> s_visible;

        // Cache de transforms por índice dentro de la unit/frame.
        // - animated_cache por tree_index (skeleton tree)
        // - bind_cache por transform_index (m_TransformArray)
        // Referencias estables: unordered_map garantiza no invalidar
        // punteros a valores en inserts posteriores.
        thread_local std::unordered_map<uint16_t, part_xform_t> s_animated_cache;
        thread_local std::unordered_map<uint16_t, part_xform_t> s_bind_cache;
        s_animated_cache.clear();
        s_bind_cache.clear();

        for (const auto& part : cache.parts)
        {
            const xray_part_kind_t kind = part.kind;

            bool  is_small = false;
            ImU32 color = IM_COL32(255, 255, 255, 30);

            switch (kind)
            {
            case xray_part_kind_t::GunBarrel:
                if (!sdk::bShowModuleGunBarrel) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_gun_barrel);
                is_small = true;
                break;
            case xray_part_kind_t::CannonBreech:
                if (!sdk::bShowModuleCannonBreech) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_cannon_breech);
                is_small = true;
                break;
            case xray_part_kind_t::Crew:
                if (!sdk::bShowModuleCrew) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_crew);
                is_small = true;
                break;
            case xray_part_kind_t::Fuel:
                if (!sdk::bShowModuleFuelTank) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_fuel_tank);
                break;
            case xray_part_kind_t::Ammo:
                if (!sdk::bShowModuleAmmoRack) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_ammo_rack);
                break;
            case xray_part_kind_t::Engine:
                if (!sdk::bShowModuleEngine) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_engine);
                break;
            case xray_part_kind_t::Transmission:
                if (!sdk::bShowModuleTransmission) continue;
                color = ImGui::ColorConvertFloat4ToU32(sdk::color_mod_transmission);
                break;
            case xray_part_kind_t::Body:
            default:
                continue; // Body/partes no clasificadas: nunca proyectar ni dibujar.
            }

            if (is_small && distance_f > sdk::xray_small_module_distance)
                continue;

            if (part.vertices.empty() || part.indices.size() < 3)
                continue;

            // ---- Transform: skeleton tree primero, bind pose de fallback ----
            // Cacheado por índice dentro de este draw. Múltiples parts con el
            // mismo transform_index NO repiten lectura DMA.
            const part_xform_t* p_transform = nullptr;

            // animated
            if (skel.animated_ok && part.tree_index != 0xFFFF)
            {
                auto it = s_animated_cache.find(part.tree_index);
                if (it == s_animated_cache.end())
                {
                    part_xform_t tmp{};
                    if (read_animated_matrix(skel, part.tree_index, tmp))
                        it = s_animated_cache.emplace(part.tree_index, tmp).first;
                }
                if (it != s_animated_cache.end())
                    p_transform = &it->second;
            }

            // bind pose fallback
            if (!p_transform)
            {
                auto it = s_bind_cache.find(part.transform_index);
                if (it == s_bind_cache.end())
                {
                    part_xform_t tmp{};
                    if (read_part_transform(cache.matrix_array_base, part.transform_index, tmp))
                        it = s_bind_cache.emplace(part.transform_index, tmp).first;
                }
                if (it != s_bind_cache.end())
                    p_transform = &it->second;
            }

            // ---- Alpha ----
            if (sdk::xray_module_alpha < 1.0f)
            {
                int alpha = static_cast<int>((color >> 24) & 0xFF);
                alpha = static_cast<int>(static_cast<float>(alpha) * sdk::xray_module_alpha);
                alpha = std::clamp(alpha, 0, 255);
                color = (color & 0x00FFFFFF) | (alpha << 24);
            }

            if (sdk::xray_alpha_falloff)
            {
                const float distance_ratio = std::clamp(distance_f / sdk::xray_render_distance, 0.0f, 1.0f);
                int alpha = static_cast<int>((color >> 24) & 0xFF);
                alpha = static_cast<int>(static_cast<float>(alpha) * (1.0f - distance_ratio));
                alpha = std::clamp(alpha, 10, 255);
                color = (color & 0x00FFFFFF) | (alpha << 24);
            }

            // ================================================================
            // MODO WIREFRAME PURO (0)
            //   Proyecta una sola vez con buffers thread_local y dibuja líneas.
            //   No pasa por fill batching.
            // ================================================================
            if (sdk::xray_render_mode == 0)
            {
                draw_module_lines_world(part, p_transform, unit_rot, unit_pos,
                    camera_matrix, proj_ctx, w2s_counter, color, sdk::xray_line_thickness);
                continue;
            }

            // ================================================================
            // MODO FILL (1/2) — un único pase de proyección
            // ================================================================
            const size_t n = part.vertices.size();
            const size_t visible_count = project_part_vertices_to(
                part, p_transform, unit_rot, unit_pos,
                camera_matrix, proj_ctx, w2s_counter,
                s_projected, s_visible);

            if (visible_count == 0)
                continue;

            // ---- Bounding 2D para cull rápido ----
            float bmin_x = std::numeric_limits<float>::max();
            float bmin_y = std::numeric_limits<float>::max();
            float bmax_x = std::numeric_limits<float>::lowest();
            float bmax_y = std::numeric_limits<float>::lowest();

            for (size_t i = 0; i < n; ++i)
            {
                if (!s_visible[i]) continue;
                if (s_projected[i].x < bmin_x) bmin_x = s_projected[i].x;
                if (s_projected[i].y < bmin_y) bmin_y = s_projected[i].y;
                if (s_projected[i].x > bmax_x) bmax_x = s_projected[i].x;
                if (s_projected[i].y > bmax_y) bmax_y = s_projected[i].y;
            }

            if (bmax_x < -vp_margin || bmax_y < -vp_margin ||
                bmin_x > vp_w + vp_margin || bmin_y > vp_h + vp_margin)
            {
                continue;
            }

            // ---- Fill batch ----
            auto& batch = fill_batches[static_cast<ImU32>(kind)];
            batch.verts.reserve(batch.verts.size() + n);
            batch.indices.reserve(batch.indices.size() + part.indices.size());

            const size_t tri_count = part.indices.size() / 3;
            for (size_t t = 0; t < tri_count; ++t)
            {
                const uint32_t ia = part.indices[t * 3 + 0];
                const uint32_t ib = part.indices[t * 3 + 1];
                const uint32_t ic = part.indices[t * 3 + 2];
                if (ia >= n || ib >= n || ic >= n) continue;
                if (!s_visible[ia] || !s_visible[ib] || !s_visible[ic]) continue;

                const ImDrawIdx base = static_cast<ImDrawIdx>(batch.verts.size());

                ImDrawVert v{};
                v.uv = ImVec2(0.0f, 0.0f);
                v.col = color;

                v.pos = ImVec2(s_projected[ia].x, s_projected[ia].y); batch.verts.push_back(v);
                v.pos = ImVec2(s_projected[ib].x, s_projected[ib].y); batch.verts.push_back(v);
                v.pos = ImVec2(s_projected[ic].x, s_projected[ic].y); batch.verts.push_back(v);

                batch.indices.push_back(base);
                batch.indices.push_back(base + 1);
                batch.indices.push_back(base + 2);
            }

            // ================================================================
            // WIREFRAME OVERLAY — reusa s_projected / s_visible del pase fill.
            // Antes se volvía a proyectar el part entero; ahora solo se
            // dibujan las líneas con datos ya calculados.
            // ================================================================
            if (sdk::xray_wireframe_overlay)
            {
                const float overlay_thickness = std::max(0.5f, sdk::xray_line_thickness * 0.7f);
                draw_lines_from_projected(part.indices, s_projected, s_visible,
                    draw_list, color, overlay_thickness);
            }

            if (sdk::bShowModuleNames && distance_f <= sdk::xray_names_distance && !part.vertices.empty())
            {
                const vec3_t v_local = bvh_vertex_to_local_game(part.vertices[0], p_transform);
                const vec3_t center_world = unit_rot.transform(v_local) + unit_pos;
                vec2_t sp;
                if (world_to_screen_counted(center_world, sp, camera_matrix, proj_ctx, w2s_counter))
                {
                    g_render->text(
                        { sp.x, sp.y },
                        IM_COL32(255, 255, 255, 255),
                        0,
                        part.name,
                        g_render->fonts().m_esp,
                        sdk::esp_font_size_module);
                }
            }
        }

        flush_fill_batches(draw_list, fill_batches);
    }

    // ========================================================================
    // [SECCIÓN 9] RUN PRINCIPAL
    // ========================================================================
    inline void run()
    {
        const auto esp_start = std::chrono::steady_clock::now();
        uint64_t aimbot_us_total = 0;
        uint64_t aimbot_calls = 0;
        uint32_t w2s_calls = 0;
        uint32_t obb_calls = 0;

        if (!sdk::esp_enabled)
            return;

        const auto snapshot = misc::snapshot_frame();
        if (!snapshot)
            return;

        const auto& curr_snapshot = *snapshot;
        const auto& frame = curr_snapshot.frame;
        if (!frame.valid)
            return;

        const auto snapshot_age_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - curr_snapshot.captured_at
        ).count();
        if (snapshot_age_us >= 0)
            misc::prof_snapshot_age_us.store(static_cast<uint64_t>(snapshot_age_us), std::memory_order_relaxed);

        const ViewMatrix_t camera_matrix = frame.camera_matrix;
        const vec3_t camera_pos = frame.camera_pos;

        projection_context_t projection_ctx{};
        projection_ctx.camera_pos = camera_pos;
        projection_ctx.bias_clip_x =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][0]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][0]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][0]) +
            static_cast<double>(camera_matrix[3][0]);
        projection_ctx.bias_clip_y =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][1]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][1]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][1]) +
            static_cast<double>(camera_matrix[3][1]);
        projection_ctx.bias_clip_w =
            static_cast<double>(camera_pos.x) * static_cast<double>(camera_matrix[0][3]) +
            static_cast<double>(camera_pos.y) * static_cast<double>(camera_matrix[1][3]) +
            static_cast<double>(camera_pos.z) * static_cast<double>(camera_matrix[2][3]) +
            static_cast<double>(camera_matrix[3][3]);
        projection_ctx.valid = false;

        if (frame.gui_state != GuiState::ALIVE && frame.gui_state != GuiState::SPEC)
            return;

        const float esp_render_distance = std::clamp(sdk::esp_render_distance, 1000.0f, 55000.0f);
        const float esp_render_distance_sq = esp_render_distance * esp_render_distance;
        const float esp_box_distance = std::clamp(sdk::esp_box_distance, 500.0f, 3500.0f);
        const float esp_box_distance_sq = esp_box_distance * esp_box_distance;
        constexpr float far_simplified_distance = 10000.0f;
        constexpr float far_simplified_distance_sq = far_simplified_distance * far_simplified_distance;
        ImFont* const esp_font = g_render->fonts().m_esp;

        if (frame.local_is_plane && sdk::esp_show_bomb_impact && !frame.bomb_impact.empty())
        {
            vec2_t sp;
            if (g_render->world_to_screen(frame.bomb_impact, sp, camera_matrix))
            {
                g_render->circle(sp.x, sp.y, 6.0f, IM_COL32(255, 0, 200, 255), 16);
                draw_impact_x(sp, IM_COL32(255, 255, 255, 255));
            }
        }

        radar::draw(curr_snapshot.positions);

        thread_local std::vector<c_unit> units;
        misc::snapshot_units_into(units);
        const vec3_t& local_pos = frame.local_pos;

        int32_t local_team = -1;
        uintptr_t local_unit_base = 0;
        rotation_matrix_t local_rot{};
        bool has_local_rot = false;
        {
            uintptr_t local_player = sdk::cLocalPlayer ? sdk::cLocalPlayer->get_base() : 0;
            uintptr_t local_unit_ptr = 0;
            if (local_player) {
                local_unit_ptr = TargetProcess->Read<uintptr_t>(local_player + offsets::localplayer::localunit_offset);
                if (!local_unit_ptr)
                    local_unit_ptr = TargetProcess->Read<uintptr_t>(local_player + offsets::localplayer::ownedUnitRef_offset);
                if (!local_unit_ptr)
                    local_unit_ptr = TargetProcess->Read<uintptr_t>(local_player + offsets::localplayer::controlledUnitRef_offset);

                if (local_unit_ptr) {
                    local_unit_base = local_unit_ptr;
                    local_rot = TargetProcess->Read<rotation_matrix_t>(local_unit_ptr + offsets::unit_offsets::rotation_matrix_offset);
                    has_local_rot = true;

                    uint8_t team_byte = TargetProcess->Read<uint8_t>(local_unit_ptr + offsets::unit_offsets::unitArmyNo_offset);
                    local_team = static_cast<int32_t>(team_byte);
                }
            }
        }

        bool is_scoping = false;
        {
            const uintptr_t module_base = runtime::base_address();
            uint8_t scoping_byte = 0;
            if (module_base >= 0x10000 && module_base < 0x7FFFFFFFFFFF &&
                offsets::is_scoping_offset < 0x7FFFFFFFFFFF - module_base)
            {
                TargetProcess->Read(module_base + offsets::is_scoping_offset, &scoping_byte, sizeof(scoping_byte));
            }
            is_scoping = (scoping_byte != 0);
        }

        constexpr float ballistic_render_distance = 3500.0f;
        constexpr float ballistic_render_distance_sq = ballistic_render_distance * ballistic_render_distance;
        if (sdk::aimbot_enabled || sdk::air_lead_enabled)
            aimbot::BeginFrame();

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        const ImVec2 viewport_center = main_viewport
            ? ImVec2(main_viewport->Pos.x + main_viewport->Size.x * 0.5f, main_viewport->Pos.y + main_viewport->Size.y * 0.5f)
            : ImVec2(0.0f, 0.0f);
        const float aim_fov_radius = main_viewport
            ? (std::min(main_viewport->Size.x, main_viewport->Size.y) * 0.50f)
            : 0.0f;
        const float aim_fov_radius_sq = aim_fov_radius * aim_fov_radius;

        c_unit best_aimbot_unit{};
        vec3_t best_aimbot_pos{};
        vec2_t best_aimbot_screen_pos{};
        float best_aimbot_distance_f = 0.0f;
        ImU32 best_aimbot_color = IM_COL32(255, 255, 255, 255);
        float best_aimbot_score = std::numeric_limits<float>::max();
        bool has_best_aimbot_target = false;

        c_unit best_xray_unit{};
        vec3_t best_xray_pos{};
        float best_xray_distance_f = 0.0f;
        float best_xray_score = std::numeric_limits<float>::max();
        bool has_xray_target = false;

        struct TeamCacheEntry {
            uint8_t team = 0xFF;
            std::chrono::steady_clock::time_point next_update{};
        };
        struct AirContainerCacheEntry {
            uintptr_t container = 0;
            std::chrono::steady_clock::time_point next_update{};
        };
        static std::unordered_map<uintptr_t, TeamCacheEntry> team_cache;
        static std::unordered_map<uintptr_t, AirContainerCacheEntry> air_container_cache;

        auto get_unit_team = [&](uintptr_t unit_base) -> uint8_t {
            const auto now = std::chrono::steady_clock::now();
            auto& entry = team_cache[unit_base];
            if (now >= entry.next_update) {
                entry.team = TargetProcess->Read<uint8_t>(unit_base + offsets::unit_offsets::unitArmyNo_offset);
                entry.next_update = now + std::chrono::milliseconds(500);
            }
            return entry.team;
            };

        auto get_air_container = [&](uintptr_t unit_base, const std::chrono::steady_clock::time_point& now) -> uintptr_t {
            auto& entry = air_container_cache[unit_base];
            if (now >= entry.next_update) {
                entry.container = TargetProcess->Read<uintptr_t>(unit_base + offsets::unit_offsets::airmovement_offset);
                entry.next_update = now + std::chrono::milliseconds(200);
            }
            return entry.container;
            };

        c_unit best_air_lead_unit{};
        vec3_t best_air_lead_pos{};
        float best_air_lead_score = std::numeric_limits<float>::max();
        bool has_best_air_lead_target = false;

        struct AirLeadCacheEntry {
            vec3_t raw_lead_point{};
            vec3_t smoothed_lead_point{};
            vec3_t last_position{};
            vec3_t measured_velocity{};
            vec3_t filtered_velocity{};
            vec3_t filtered_acceleration{};
            vec3_t prediction_velocity{};
            vec3_t prediction_acceleration{};
            uintptr_t last_container = 0;
            bool initialized = false;
            bool have_position = false;
            bool have_motion = false;
            std::chrono::steady_clock::time_point last_sample{};
            std::chrono::steady_clock::time_point last_update{};
            std::chrono::steady_clock::time_point next_sample{};
            std::chrono::steady_clock::time_point last_smooth{};
            vec3_t solve_origin{};
            air_lead_model::TurnState turn_state{};
            float turn_omega = 0.0f;
        };
        static std::unordered_map<uintptr_t, AirLeadCacheEntry> air_lead_cache;

        auto run_air_lead = [&](c_unit& unit, const vec3_t& unit_pos) {
            const uintptr_t unit_base = unit.get_base();
            const auto now = std::chrono::steady_clock::now();
            auto& cache = air_lead_cache[unit_base];
            // DMA + filtering + solve share a cadence; rendering stays per-frame.
            // Failed solutions are throttled too, rather than retried at render FPS.
            if (now >= cache.next_sample) {
                cache.next_sample = now + std::chrono::milliseconds(30);
                vec3_t air_velocity{ 0.0f, 0.0f, 0.0f };
                vec3_t air_acceleration{ 0.0f, 0.0f, 0.0f };
                bool velocity_from_native = false;
                bool acceleration_from_native = false;

                vec3_t origin = camera_pos;
                if (sdk::air_lead_use_weapon_position && local_unit_base) {
                    // Weapon-local geometry is shared across all air targets.
                    static uintptr_t weapon_owner = 0;
                    static vec3_t weapon_local{};
                    static bool weapon_valid = false;
                    static std::chrono::steady_clock::time_point weapon_next{};
                    if (weapon_owner != local_unit_base || now >= weapon_next) {
                        weapon_owner = local_unit_base;
                        weapon_valid = aimbot::GetLocalWeaponLocalPosition(weapon_local);
                        weapon_next = now + std::chrono::milliseconds(100);
                    }
                    if (weapon_valid) {
                        if (has_local_rot) {
                            origin = frame.local_pos + local_rot.transform(weapon_local);
                        }
                        else {
                            origin = frame.local_pos + weapon_local;
                        }
                    }
                }

                uintptr_t air_container = get_air_container(unit_base, now);
                if (air_container) {
                    vec3d_t vel_d{};
                    const bool velocity_read = TargetProcess->Read(
                        air_container + offsets::unit_offsets::airmovement_offsets::velocity_offset,
                        &vel_d, sizeof(vel_d));
                    vec3_t raw_vel = { static_cast<float>(vel_d.x), static_cast<float>(vel_d.y), static_cast<float>(vel_d.z) };
                    if (velocity_read && std::isfinite(raw_vel.x) && std::isfinite(raw_vel.y) && std::isfinite(raw_vel.z) &&
                        raw_vel.length() < 950.0f) {
                        air_velocity = raw_vel;
                        velocity_from_native = true;
                    }

                    vec3d_t acc_d{};
                    const bool acceleration_read = TargetProcess->Read(
                        air_container + offsets::unit_offsets::airmovement_offsets::acceleration_offset,
                        &acc_d, sizeof(acc_d));
                    vec3_t raw_acc = { static_cast<float>(acc_d.x), static_cast<float>(acc_d.y), static_cast<float>(acc_d.z) };
                    if (acceleration_read && std::isfinite(raw_acc.x) && std::isfinite(raw_acc.y) && std::isfinite(raw_acc.z) &&
                        raw_acc.length() < 160.0f) {
                        air_acceleration = raw_acc;
                        acceleration_from_native = true;
                    }
                }

                if ((cache.have_position && now - cache.last_sample > std::chrono::milliseconds(700)) ||
                    (cache.last_container && air_container && cache.last_container != air_container)) {
                    cache = AirLeadCacheEntry{};
                    cache.next_sample = now + std::chrono::milliseconds(30);
                }
                cache.last_container = air_container;
                cache.solve_origin = origin;

                const auto sample_time = curr_snapshot.captured_at;
                if (air_lead_model::finite(unit_pos) &&
                    (!cache.have_position || (unit_pos - cache.last_position).length() > 0.04f))
                {
                    if (cache.have_position)
                    {
                        const float dt = std::chrono::duration<float>(sample_time - cache.last_sample).count();
                        if (dt >= 0.025f && dt <= 0.55f)
                        {
                            const vec3_t measured = (unit_pos - cache.last_position) / dt;
                            if (air_lead_model::finite(measured) && measured.length() < 950.0f)
                            {
                                const vec3_t previous = cache.measured_velocity;
                                const bool had_motion = cache.have_motion;
                                const float alpha_v = 1.0f - std::exp(-dt / 0.18f);
                                cache.filtered_velocity = had_motion
                                    ? cache.filtered_velocity + (measured - cache.filtered_velocity) * alpha_v : measured;
                                if (had_motion)
                                {
                                    const vec3_t derived = (measured - previous) / dt;
                                    if (air_lead_model::finite(derived) && derived.length() < 160.0f)
                                    {
                                        const float alpha_a = 1.0f - std::exp(-dt / 0.32f);
                                        cache.filtered_acceleration += (derived - cache.filtered_acceleration) * alpha_a;
                                    }
                                }
                                cache.measured_velocity = measured;
                                cache.have_motion = true;
                                cache.turn_omega = air_lead_model::update_turn_state(
                                    cache.filtered_velocity, cache.filtered_acceleration, dt,
                                    static_cast<std::uint32_t>(unit_base), &cache.turn_state);
                            }
                        }
                        else if (dt > 0.55f)
                            cache.have_motion = false;
                    }
                    cache.last_position = unit_pos;
                    cache.last_sample = sample_time;
                    cache.have_position = true;
                }

                if (velocity_from_native && air_velocity.length() < 0.5f &&
                    cache.have_motion && cache.filtered_velocity.length() > 10.0f)
                    velocity_from_native = false;
                if (!velocity_from_native && cache.have_motion)
                    air_velocity = cache.filtered_velocity;
                if ((!acceleration_from_native ||
                    (air_acceleration.length() < 0.1f && cache.filtered_acceleration.length() > 5.0f)) &&
                    cache.have_motion)
                    air_acceleration = cache.filtered_acceleration;
                if (!air_lead_model::finite(air_acceleration) || air_acceleration.length() > 160.0f)
                    air_acceleration = {};
                cache.prediction_velocity = air_velocity;
                cache.prediction_acceleration = air_acceleration;
                { // already paced by next_sample above
                    const aimbot::BallisticsData local_bal = aimbot::ballisticsData;
                    const float sample_age = std::clamp(
                        std::chrono::duration<float>(now - curr_snapshot.captured_at).count(), 0.0f, 0.20f);
                    const float delay = sample_age + std::clamp(sdk::air_lead_extra_time, 0.0f, 0.5f);
                    const float speed = std::isfinite(local_bal.velocity) && local_bal.velocity > 0.0f
                        ? local_bal.velocity : 300.0f;
                    const auto solution = air_lead_model::solve(origin, unit_pos, air_velocity,
                        air_acceleration, speed, local_bal.mass, local_bal.caliber, local_bal.drag,
                        cache.turn_omega, delay, sdk::air_lead_use_turn_prediction);
                    if (!solution.valid)
                    {
                        cache.initialized = false;
                        return;
                    }
                    cache.raw_lead_point = solution.point;

                    if (!cache.initialized) {
                        cache.smoothed_lead_point = cache.raw_lead_point;
                        cache.last_smooth = now;
                        cache.initialized = true;
                    }
                    cache.last_update = now;
                }
            } // sampled work; only projection/smoothing below runs at render FPS

            if (cache.initialized)
            {
                float smooth_alpha = 0.5f;
                if (sdk::air_lead_smooth_adaptive) {
                    const float min_a = std::clamp(sdk::air_lead_smooth_min_alpha, 0.05f, 0.95f);
                    const float max_a = std::clamp(sdk::air_lead_smooth_max_alpha, min_a, 0.95f);
                    const float distance_to_target = (unit_pos - cache.solve_origin).length();
                    const float reactivity = std::clamp((2500.0f - distance_to_target) / 2500.0f, 0.0f, 1.0f);
                    smooth_alpha = min_a + (max_a - min_a) * reactivity;
                }

                const float dt_render = std::clamp(
                    std::chrono::duration<float>(now - cache.last_smooth).count(), 0.0f, 0.10f);
                cache.last_smooth = now;
                const float tau = -1.0f / (60.0f * std::log1p(-std::clamp(smooth_alpha, 0.05f, 0.95f)));
                const float alpha_time = 1.0f - std::exp(-dt_render / tau);
                const float raw_age = std::clamp(
                    std::chrono::duration<float>(now - cache.last_update).count(), 0.0f, 0.10f);
                const vec3_t live_raw = cache.raw_lead_point + cache.prediction_velocity * raw_age
                    + cache.prediction_acceleration * (0.5f * raw_age * raw_age);
                cache.smoothed_lead_point += cache.prediction_velocity * dt_render
                    + cache.prediction_acceleration * (0.5f * dt_render * dt_render);
                cache.smoothed_lead_point += (live_raw - cache.smoothed_lead_point) * alpha_time;

                if (std::isfinite(cache.smoothed_lead_point.x) &&
                    std::isfinite(cache.smoothed_lead_point.y) &&
                    std::isfinite(cache.smoothed_lead_point.z))
                {
                    vec2_t lead_screen;
                    if (world_to_screen_counted(cache.smoothed_lead_point, lead_screen, camera_matrix, projection_ctx, w2s_calls))
                    {
                        const float size = sdk::air_lead_marker_size;
                        const float thickness = sdk::air_lead_line_thickness;
                        const ImU32 color = ImGui::ColorConvertFloat4ToU32(sdk::air_lead_sim_color);

                        g_render->circle(lead_screen.x, lead_screen.y, size, color, 16);
                        const float cross = size * 1.5f;
                        g_render->line(lead_screen.x - cross, lead_screen.y, lead_screen.x + cross, lead_screen.y, color, thickness);
                        g_render->line(lead_screen.x, lead_screen.y - cross, lead_screen.x, lead_screen.y + cross, color, thickness);

                        vec2_t unit_screen_pos;
                        if (world_to_screen_counted(unit_pos, unit_screen_pos, camera_matrix, projection_ctx, w2s_calls))
                        {
                            g_render->line(unit_screen_pos.x, unit_screen_pos.y, lead_screen.x, lead_screen.y, color, thickness * 0.7f);
                        }
                    }
                }
            }
            };

        for (c_unit& unit : units)
        {
            vec3_t unit_pos;
            const auto unit_base = unit.get_base();
            const auto curr_pos_it = curr_snapshot.positions.find(unit_base);
            if (curr_pos_it == curr_snapshot.positions.end())
                continue;

            unit_pos = curr_pos_it->second;

            if (unit_pos.empty())
                continue;

            const float dx = unit_pos.x - local_pos.x;
            const float dy = unit_pos.y - local_pos.y;
            const float dz = unit_pos.z - local_pos.z;
            const float distance_sq = dx * dx + dy * dy + dz * dz;
            if (distance_sq > esp_render_distance_sq)
                continue;

            auto& cached_info = get_cached_unit_info(unit_base, unit);
            const std::string& unit_type = cached_info.unit_type;
            if (unit_type.empty())
                continue;

            const uint16_t raw_type = cached_info.raw_type;
            const auto unit_type_info = get_unit_type_info(unit_type, raw_type);

            if (sdk::esp_only_show_planes && !unit_type_info.is_plane)
                continue;

            bool process_air_lead = sdk::air_lead_enabled && sdk::air_lead_use_simulation &&
                unit_type_info.is_plane && local_team != -1 &&
                is_air_lead_target(unit_type, unit_type_info);

            if (process_air_lead)
            {
                uint8_t team_byte = get_unit_team(unit_base);
                if (static_cast<int32_t>(team_byte) != local_team)
                {
                    if (sdk::air_lead_only_target)
                    {
                        vec2_t sp;
                        if (world_to_screen_counted(unit_pos, sp, camera_matrix, projection_ctx, w2s_calls))
                        {
                            const float dx_center = sp.x - viewport_center.x;
                            const float dy_center = sp.y - viewport_center.y;
                            const float screen_dist_sq = dx_center * dx_center + dy_center * dy_center;
                            const float score = screen_dist_sq + distance_sq * 0.05f;
                            if (!has_best_air_lead_target || score < best_air_lead_score)
                            {
                                best_air_lead_unit = unit;
                                best_air_lead_pos = unit_pos;
                                best_air_lead_score = score;
                                has_best_air_lead_target = true;
                            }
                        }
                    }
                    else
                    {
                        run_air_lead(unit, unit_pos);
                    }
                }
            }

            if (unit_type_info.is_dummy || !unit_type_info.worth_to_draw)
                continue;

            if (!is_point_in_clip_frustum(unit_pos, camera_matrix, projection_ctx))
                continue;

            vec2_t screen_pos;
            if (!world_to_screen_counted(unit_pos, screen_pos, camera_matrix, projection_ctx, w2s_calls))
                continue;

            const float distance_f = std::sqrt(distance_sq);
            const std::string& vehicle_name = cached_info.vehicle_name;
            const float dx_center = screen_pos.x - viewport_center.x;
            const float dy_center = screen_pos.y - viewport_center.y;
            const float screen_dist_sq = dx_center * dx_center + dy_center * dy_center;
            const bool is_inside_fov = (aim_fov_radius_sq > 0.0f) && (screen_dist_sq <= aim_fov_radius_sq);
            bool show_name_for_unit = true;
            if (distance_sq > far_simplified_distance_sq)
                show_name_for_unit = is_inside_fov;

            float label_base_y = 0.f;

            const bool should_draw_any_box = sdk::esp_show_boxes || sdk::esp_use_3d_boxes;
            if (unit_type_info.draw_box && should_draw_any_box)
            {
                const float active_box_distance_sq = esp_box_distance_sq;
                if (distance_sq > active_box_distance_sq)
                {
                    label_base_y = screen_pos.y;
                }
                else
                {
                    if (sdk::esp_use_3d_boxes)
                    {
                        auto& box_info = get_cached_box_info(unit_base, unit, distance_f);
                        ++obb_calls;
                        const auto world_corners =
                            calculate_bbox_corners_rotated(unit_pos, box_info.bbmin, box_info.bbmax, box_info.rot);

                        std::array<vec2_t, 8> sc;
                        bool                  visible = true;

                        for (size_t i = 0; i < 8; ++i)
                        {
                            if (!world_to_screen_counted(world_corners[i], sc[i], camera_matrix, projection_ctx, w2s_calls))
                            {
                                visible = false;
                                break;
                            }
                        }

                        if (visible)
                        {
                            float box_bottom_y = sc[0].y;
                            for (size_t i = 1; i < 8; ++i)
                                box_bottom_y = std::max(box_bottom_y, sc[i].y);

                            label_base_y = box_bottom_y;
                            draw_wireframe_box(sc, unit_type_info.color, 1.f);
                        }
                    }
                    else if (sdk::esp_show_boxes)
                    {
                        float projected_max_y = 0.0f;
                        if (draw_projected_2d_box_from_obb(unit, unit_pos, distance_f, camera_matrix, projection_ctx, w2s_calls, obb_calls, unit_type_info.color, &projected_max_y))
                            label_base_y = projected_max_y;
                    }
                }
            }
            else
            {
                label_base_y = screen_pos.y;
            }

            if (label_base_y > 0.f)
            {
                float text_y = label_base_y + 5.0f;

                if (sdk::esp_show_vehicle_names && show_name_for_unit && !vehicle_name.empty())
                {
                    g_render->text(
                        { screen_pos.x, text_y },
                        unit_type_info.color,
                        0,
                        vehicle_name,
                        esp_font,
                        sdk::esp_font_size_name
                    );
                    text_y += 15.0f;
                }

                if (sdk::esp_show_distance) {
                    char  buf[16];
                    std::snprintf(buf, sizeof(buf), "%.0fm", distance_f);

                    g_render->text(
                        { screen_pos.x, text_y },
                        unit_type_info.color,
                        0,
                        buf,
                        esp_font,
                        sdk::esp_font_size_distance
                    );
                    text_y += 15.0f;
                }
            }

            const bool allow_aimbot_for_unit = unit_type_info.draw_box && !unit_type_info.is_plane;
            if (sdk::aimbot_enabled && allow_aimbot_for_unit && distance_sq <= ballistic_render_distance_sq)
            {
                if (aim_fov_radius_sq > 0.0f && screen_dist_sq <= aim_fov_radius_sq)
                {
                    const float score = screen_dist_sq + (distance_sq * 0.05f);
                    if (!has_best_aimbot_target || score < best_aimbot_score)
                    {
                        best_aimbot_unit = unit;
                        best_aimbot_pos = unit_pos;
                        best_aimbot_screen_pos = screen_pos;
                        best_aimbot_distance_f = distance_f;
                        best_aimbot_color = unit_type_info.color;
                        best_aimbot_score = score;
                        has_best_aimbot_target = true;
                    }
                }
            }

            if (sdk::bShowDamageModel && local_team != -1 && distance_f <= sdk::xray_render_distance)
            {
                if (unit_type_info.draw_box && !unit_type_info.is_plane)
                {
                    uint8_t team_byte = get_unit_team(unit_base);
                    int32_t team = static_cast<int32_t>(team_byte);
                    if (team != local_team)
                    {
                        float score = screen_dist_sq + distance_sq * 0.05f;
                        if (!has_xray_target || score < best_xray_score)
                        {
                            best_xray_unit = unit;
                            best_xray_pos = unit_pos;
                            best_xray_distance_f = distance_f;
                            best_xray_score = score;
                            has_xray_target = true;
                        }
                    }
                }
            }
        }

        if (sdk::air_lead_only_target && has_best_air_lead_target)
        {
            run_air_lead(best_air_lead_unit, best_air_lead_pos);
        }

        {
            const auto now = std::chrono::steady_clock::now();
            static auto air_lead_gc = std::chrono::steady_clock::now();
            if (now - air_lead_gc > std::chrono::seconds(10)) {
                for (auto it = air_lead_cache.begin(); it != air_lead_cache.end();) {
                    if (now - it->second.last_update > std::chrono::seconds(10))
                        it = air_lead_cache.erase(it);
                    else
                        ++it;
                }
                for (auto it = air_container_cache.begin(); it != air_container_cache.end();) {
                    if (now > it->second.next_update + std::chrono::seconds(10))
                        it = air_container_cache.erase(it);
                    else
                        ++it;
                }
                air_lead_gc = now;
            }
        }

        if (has_xray_target)
            draw_xray_modules(best_xray_unit, best_xray_pos, camera_matrix, projection_ctx, w2s_calls, best_xray_distance_f);

        if (sdk::esp_show_reload && has_best_aimbot_target)
        {
            const uint8_t reload = get_cached_reload_info(best_aimbot_unit.get_base(), best_aimbot_unit).value;
            constexpr float stat = (10.f / 16);
            constexpr float reload_ticks_max = 16.0f;
            const float seconds = stat * reload;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.1fs", seconds);

            const float clamped_reload = std::min(static_cast<float>(reload), reload_ticks_max);
            const float fill_ratio = std::clamp(1.0f - (clamped_reload / reload_ticks_max), 0.0f, 1.0f);

            const auto bounds = get_projected_box_bounds(best_aimbot_unit, best_aimbot_pos, best_aimbot_distance_f, camera_matrix, projection_ctx, w2s_calls, obb_calls);
            if (bounds.valid)
            {
                const float bar_x = bounds.max_x + 4.0f;
                const float bar_top = bounds.min_y;
                const float bar_bottom = bounds.max_y;
                const float bar_height = std::max(1.0f, bar_bottom - bar_top);
                const float fill_height = bar_height * fill_ratio;

                g_render->line(bar_x, bar_top, bar_x, bar_top + bar_height, IM_COL32(35, 35, 35, 220), 1.0f);
                g_render->line(bar_x, bar_top + bar_height, bar_x, bar_top + bar_height - fill_height, IM_COL32(40, 220, 40, 255), 1.0f);

                if (sdk::esp_show_reload_seconds)
                    g_render->text({ bar_x, bar_top - 18.0f }, best_aimbot_color, 0, buf, esp_font, sdk::esp_font_size_reload);
            }
            else
            {
                if (sdk::esp_show_reload_seconds)
                {
                    const float text_y = best_aimbot_screen_pos.y + 24.0f;
                    g_render->text({ best_aimbot_screen_pos.x, text_y }, best_aimbot_color, 0, buf, esp_font, sdk::esp_font_size_reload);
                }
            }
        }

        if (sdk::aimbot_enabled && has_best_aimbot_target)
        {
            const auto aimbot_start = std::chrono::steady_clock::now();
            aimbot::run(best_aimbot_unit, best_aimbot_pos, local_pos, camera_matrix, curr_snapshot.captured_at);
            const auto aimbot_end = std::chrono::steady_clock::now();
            aimbot_us_total += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(aimbot_end - aimbot_start).count()
                );
            ++aimbot_calls;
        }

        if (sdk::esp_show_fps && g_render->draw_list() && esp_font)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const ImVec2 anchor = viewport
                ? ImVec2(viewport->Pos.x + 15.0f, viewport->Pos.y + 15.0f)
                : ImVec2(15.0f, 15.0f);

            char fps_buf[32];
            std::snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", ImGui::GetIO().Framerate);
            g_render->draw_list()->AddText(esp_font, esp_font->LegacySize, anchor, IM_COL32(255, 255, 255, 255), fps_buf);
        }

        const auto esp_end = std::chrono::steady_clock::now();
        misc::prof_esp_us.store(
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(esp_end - esp_start).count()),
            std::memory_order_relaxed
        );
        misc::prof_esp_count.fetch_add(1, std::memory_order_relaxed);
        misc::prof_aimbot_us.store(aimbot_us_total, std::memory_order_relaxed);
        misc::prof_aimbot_calls.store(aimbot_calls, std::memory_order_relaxed);
        misc::prof_esp_w2s_calls.store(static_cast<uint64_t>(w2s_calls), std::memory_order_relaxed);
        misc::prof_esp_obb_calls.store(static_cast<uint64_t>(obb_calls), std::memory_order_relaxed);
    }
}   // namespace esp