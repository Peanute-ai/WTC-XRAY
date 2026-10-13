#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

// Símbolos tácticos vectoriales: no dependen de la fuente/atlas del proceso remoto.
// El proceso DMA nunca entrega punteros útiles al ImGui del overlay.
namespace esp::radar::icons
{
    enum class kind : uint8_t { aircraft, unknown, light, medium, heavy, tank_destroyer, spaa };
    struct entry_t
    {
        kind symbol = kind::unknown;
        std::chrono::steady_clock::time_point next_refresh{};
        std::chrono::steady_clock::time_point last_seen{};
    };

    inline bool safe_pointer(uintptr_t p)
    {
        // Usuarios x64 canónicos, sin punteros nulos, bajos o kernel.
        return p >= 0x10000 && p < static_cast<uintptr_t>(0x0000800000000000ull);
    }

    inline std::string read_hint(uintptr_t address)
    {
        if (!safe_pointer(address) || address > static_cast<uintptr_t>(0x0000800000000000ull - 97))
            return {};
        char buf[96]{};
        if (!TargetProcess->Read(address, buf, sizeof(buf)))
            return {};
        buf[95] = '\0';
        const size_t end = std::char_traits<char>::length(buf);
        if (end == 95) return {}; // no confiar en strings truncados/no terminados
        return std::string(buf, end);
    }

    inline std::string canonical(std::string_view raw)
    {
        std::string out;
        out.reserve(raw.size());
        for (unsigned char c : raw)
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c));
        return out;
    }

    inline bool has(std::string_view text, std::string_view token)
    {
        return text.find(token) != std::string_view::npos;
    }

    inline kind classify(std::string_view subtype, std::string_view coarse)
    {
        const std::string type = canonical(coarse);
        if (has(type, "fighter") || has(type, "bomber") || has(type, "assault") ||
            has(type, "helicopter") || has(type, "drone")) return kind::aircraft;
        const std::string sub = canonical(subtype);
        // El subtipo tiene prioridad: exp_tank es solo la familia genérica.
        if (has(sub, "spaa") || has(sub, "antiair") || has(sub, "antiaereo") || has(sub, "flak") || has(sub, "aaa") || sub == "aa" || has(sub, "zsu") || has(sub, "\xD0\x97\xD0\xA1\xD0\xA3") || has(subtype, "antiaéreo") || has(subtype, "Antiaéreo") || has(subtype, "antiaérea")) return kind::spaa;
        if (has(sub, "tankdestroyer") || has(sub, "cazacarros") || has(sub, "cazatanques") || has(sub, "panzerjager") || has(sub, "td")) return kind::tank_destroyer;
        if (has(sub, "heavy") || has(sub, "pesado")) return kind::heavy;
        if (has(sub, "medium") || has(sub, "medio")) return kind::medium;
        if (has(sub, "light") || has(sub, "ligero")) return kind::light;
        if (has(type, "spaa") || has(type, "antiair") || type == "expaaa") return kind::spaa;
        if (has(type, "tankdestroyer")) return kind::tank_destroyer;
        if (has(type, "heavytank")) return kind::heavy;
        if (has(type, "mediumtank")) return kind::medium;
        if (has(type, "lighttank")) return kind::light;
        if (has(type, "tank")) return kind::medium;
        return kind::unknown;
    }

    inline kind classify_unit(uintptr_t unit_base, uintptr_t image)
    {
        using clock = std::chrono::steady_clock;
        static std::unordered_map<uintptr_t, entry_t> cache;
        static uintptr_t cached_image = 0;
        static auto next_gc = clock::now();
        if (cached_image != image) { cache.clear(); cached_image = image; }
        const auto now = clock::now();
        if (now >= next_gc)
        {
            for (auto it = cache.begin(); it != cache.end();)
                if (now - it->second.last_seen > std::chrono::seconds(10)) it = cache.erase(it);
                else ++it;
            next_gc = now + std::chrono::seconds(5);
        }
        if (!TargetProcess || !safe_pointer(unit_base)) return kind::unknown;
        auto& e = cache[unit_base];
        e.last_seen = now;
        if (now < e.next_refresh) return e.symbol;
        e.next_refresh = now + std::chrono::milliseconds(1500);
        e.symbol = kind::unknown;
        if (unit_base > static_cast<uintptr_t>(0x0000800000000000ull - 0x2000)) return e.symbol;
        const uintptr_t info = TargetProcess->Read<uintptr_t>(unit_base + offsets::unit_offsets::info_offset);
        if (!safe_pointer(info) || info > static_cast<uintptr_t>(0x0000800000000000ull - 0x100)) return e.symbol;
        // UnitInfo +0x30 subtipo localizable, +0x38 familia gruesa.
        const uintptr_t sub_ptr = TargetProcess->Read<uintptr_t>(info + 0x30);
        const uintptr_t type_ptr = TargetProcess->Read<uintptr_t>(info + offsets::unit_info_offsets::unit_type_offset);
        e.symbol = classify(read_hint(sub_ptr), read_hint(type_ptr));
        return e.symbol;
    }

    inline void draw(ImDrawList* list, ImVec2 p, kind symbol, ImU32 color)
    {
        constexpr ImU32 shadow = IM_COL32(10, 10, 10, 240);
        constexpr float r = 5.5f;
        if (symbol == kind::aircraft || symbol == kind::unknown)
        {
            list->AddCircleFilled(p, 4.5f, shadow, 12);
            list->AddCircleFilled(p, 3.5f, color, 12);
            return;
        }
        // Halo oscuro y siluetas inequívocas también a escala pequeña.
        list->AddCircleFilled(p, 7.0f, shadow, 16);
        switch (symbol)
        {
        case kind::spaa:
            list->AddCircle(p, 4.5f, color, 16, 1.5f);
            list->AddLine(ImVec2(p.x - r, p.y), ImVec2(p.x + r, p.y), color, 1.4f);
            list->AddLine(ImVec2(p.x, p.y - r), ImVec2(p.x, p.y + r), color, 1.4f);
            break;
        case kind::tank_destroyer:
            list->AddTriangleFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y + r), ImVec2(p.x - r, p.y + r), color);
            list->AddLine(ImVec2(p.x, p.y - r + 1), ImVec2(p.x, p.y + 3), shadow, 1.5f);
            break;
        case kind::heavy:
            list->AddRectFilled(ImVec2(p.x - r, p.y - 4), ImVec2(p.x + r, p.y + 4), color);
            list->AddRect(ImVec2(p.x - r + 2, p.y - 2), ImVec2(p.x + r - 2, p.y + 2), shadow, 0.0f, 0, 1.2f);
            break;
        case kind::medium:
            list->AddQuadFilled(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), color);
            list->AddCircleFilled(p, 1.5f, shadow, 8);
            break;
        case kind::light:
            list->AddQuad(ImVec2(p.x, p.y - r), ImVec2(p.x + r, p.y), ImVec2(p.x, p.y + r), ImVec2(p.x - r, p.y), color, 2.0f);
            break;
        default: break;
        }
    }
}
