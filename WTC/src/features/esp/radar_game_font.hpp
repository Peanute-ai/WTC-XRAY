#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// Fuente opcional del propio juego. Si cualquier lectura o validación falla,
// radar_icons.hpp sigue dibujando símbolos vectoriales, nunca puntos genéricos
// para vehículos terrestres.
namespace esp::radar::game_font
{
    inline ImFont* font = nullptr;
    inline uintptr_t source_image = 0;
    inline std::chrono::steady_clock::time_point next_attempt{};
    inline std::array<int8_t, 5> glyph_available{ -1, -1, -1, -1, -1 };

    inline bool valid_address(uintptr_t address, size_t bytes = 1)
    {
        constexpr uintptr_t limit = static_cast<uintptr_t>(0x0000800000000000ull);
        return address >= 0x10000 && bytes != 0 && bytes < limit && address <= limit - bytes;
    }

    inline uint16_t big16(const uint8_t* p)
    {
        return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
    }
    inline uint32_t big32(const uint8_t* p)
    {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
            (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }

    inline bool valid_sfnt(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() < 12 || bytes.size() > 2u * 1024u * 1024u) return false;
        const uint32_t tag = big32(bytes.data());
        if (tag != 0x00010000u && tag != 0x4F54544Fu && tag != 0x74727565u) return false;
        const uint16_t tables = big16(bytes.data() + 4);
        if (tables == 0 || tables > 96 || 12u + 16u * tables > bytes.size()) return false;
        bool cmap = false, head = false, hhea = false, maxp = false;
        for (uint16_t i = 0; i < tables; ++i)
        {
            const uint8_t* record = bytes.data() + 12u + 16u * i;
            const uint32_t offset = big32(record + 8);
            const uint32_t length = big32(record + 12);
            if (!length || offset > bytes.size() || length > bytes.size() - offset) return false;
            const uint32_t name = big32(record);
            cmap |= name == 0x636D6170u;
            head |= name == 0x68656164u;
            hhea |= name == 0x68686561u;
            maxp |= name == 0x6D617870u;
        }
        return cmap && head && hhea && maxp;
    }

    inline bool read_remote_font(std::vector<uint8_t>& out, uintptr_t image)
    {
        if (!TargetProcess || offsets::game_font_list_offset > 0x100000000ull ||
            !valid_address(image, static_cast<size_t>(offsets::game_font_list_offset) + 0x14u))
            return false;
        const uintptr_t slot = image + offsets::game_font_list_offset;
        uintptr_t table = 0;
        uint32_t count = 0;
        if (!TargetProcess->Read(slot, &table, sizeof(table)) ||
            !TargetProcess->Read(slot + 0x10, &count, sizeof(count)) ||
            !valid_address(table) || count == 0 || count > 512 ||
            !valid_address(table, static_cast<size_t>(count) * 0x28u)) return false;

        for (uint32_t i = 0; i < count; ++i)
        {
            const uintptr_t entry = table + static_cast<uintptr_t>(i) * 0x28u;
            uintptr_t info = 0;
            if (!TargetProcess->Read(entry, &info, sizeof(info)) || !valid_address(info, 0x20)) continue;
            uintptr_t name_ptr = 0;
            if (!TargetProcess->Read(info + 0x18, &name_ptr, sizeof(name_ptr)) ||
                !valid_address(name_ptr, 32)) continue;
            char name[32]{};
            if (!TargetProcess->Read(name_ptr, name, sizeof(name))) continue;
            name[31] = '\0';
            if (std::strcmp(name, "symbols_skyquake") != 0) continue;

            uintptr_t data_ptr = 0;
            uint32_t size = 0;
            if (!TargetProcess->Read(entry + 0x10, &data_ptr, sizeof(data_ptr)) ||
                !TargetProcess->Read(entry + 0x18, &size, sizeof(size)) ||
                size < 4096u || size > 2u * 1024u * 1024u ||
                !valid_address(data_ptr, size)) return false;
            out.resize(size);
            if (!TargetProcess->Read(data_ptr, out.data(), out.size())) { out.clear(); return false; }
            return valid_sfnt(out);
        }
        return false;
    }

    // LLAMAR desde el hilo UI antes de ImGui_ImplDX11_NewFrame/NewFrame.
    // ImGui 1.92 + backend DX11 actual actualizan el atlas dinámicamente.
    inline void prepare_before_frame()
    {
        const uintptr_t image = runtime::base_address();
        if (!image || !sdk::radar_enabled || !ImGui::GetCurrentContext()) return;
        if (font && source_image != image)
        {
            font = nullptr; // el atlas conserva la fuente anterior hasta salir
            next_attempt = {};
        }
        if (font) return;
        const auto now = std::chrono::steady_clock::now();
        if (now < next_attempt) return;
        next_attempt = now + std::chrono::seconds(10);
        std::vector<uint8_t> copy;
        if (!read_remote_font(copy, image)) return;
        static const ImWchar glyphs[] = { 0x2500, 0x25FF, 0 };
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false; // ImGui hace copia local; nunca libera memoria DMA.
        constexpr char font_name[] = "symbols_skyquake radar";
        static_assert(sizeof(font_name) <= sizeof(cfg.Name));
        std::memcpy(cfg.Name, font_name, sizeof(font_name));
        font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(copy.data(),
            static_cast<int>(copy.size()), 17.0f, &cfg, glyphs);
        if (font)
        {
            source_image = image;
            glyph_available.fill(-1);
        }
    }

    inline unsigned codepoint(icons::kind symbol)
    {
        using K = icons::kind;
        switch (symbol)
        {
        case K::spaa: return 0x2530;
        case K::tank_destroyer: return 0x2534;
        case K::heavy: return 0x2528;
        case K::light: return 0x252A;
        case K::medium: return 0x252C;
        default: return 0;
        }
    }

    inline bool draw(ImDrawList* list, ImVec2 center, icons::kind symbol, ImU32 color)
    {
        const unsigned cp = codepoint(symbol);
        if (!font || !cp || !list) return false;
        const size_t slot = static_cast<size_t>(symbol) - static_cast<size_t>(icons::kind::light);
        if (slot >= glyph_available.size()) return false;
        if (glyph_available[slot] < 0)
            glyph_available[slot] = font->IsGlyphInFont(static_cast<ImWchar>(cp)) ? 1 : 0;
        if (glyph_available[slot] == 0) return false;
        char utf8[] = { static_cast<char>(0xE0 | (cp >> 12)),
            static_cast<char>(0x80 | ((cp >> 6) & 0x3F)),
            static_cast<char>(0x80 | (cp & 0x3F)), '\0' };
        constexpr float size = 17.0f;
        const ImVec2 bounds = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), 0.0f, utf8);
        const ImVec2 at(center.x - bounds.x * 0.5f, center.y - bounds.y * 0.5f);
        list->AddText(font, size, ImVec2(at.x + 1, at.y + 1), IM_COL32(10, 10, 10, 240), utf8);
        list->AddText(font, size, at, color, utf8);
        return true;
    }
}
