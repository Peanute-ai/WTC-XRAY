#pragma once
// ============================================================================
//   features/esp/xray_parts.hpp
//   Clasificación compartida por el decoder BVH y el renderer X-Ray.
//   Body incluye las partes desconocidas: no se decodifican ni dibujan.
// ============================================================================

#include <cstdint>
#include <string_view>

namespace xray
{
    enum class xray_part_kind_t : uint8_t
    {
        Body = 0,
        GunBarrel,
        CannonBreech,
        Crew,
        Fuel,
        Ammo,
        Engine,
        Transmission
    };

    inline xray_part_kind_t classify_part_name(std::string_view name)
    {
        using K = xray_part_kind_t;
        if (name.empty()) return K::Body;

        if (name.find("gun_barrel") != std::string_view::npos ||
            (name.find("barrel_") != std::string_view::npos && name.find("gun") != std::string_view::npos))
            return K::GunBarrel;
        if (name.find("cannon_breech") != std::string_view::npos ||
            name.find("gun_breech") != std::string_view::npos ||
            name.find("_breech_dm") != std::string_view::npos)
            return K::CannonBreech;
        if (name.find("loader_dm") != std::string_view::npos ||
            name.find("driver_dm") != std::string_view::npos ||
            name.find("commander_dm") != std::string_view::npos ||
            name.find("gunner_dm") != std::string_view::npos ||
            name.find("_crew_dm") != std::string_view::npos ||
            name.find("crewman") != std::string_view::npos)
            return K::Crew;
        // "fuel_" also covers "fuel_tank"; avoid a redundant scan.
        if (name.find("fuel_") != std::string_view::npos)
            return K::Fuel;
        // "ammo_" covers all ammo-specific names previously checked here.
        if (name.find("ammo_") != std::string_view::npos ||
            name.find("shell_rack") != std::string_view::npos ||
            name.find("shell_storage") != std::string_view::npos)
            return K::Ammo;
        if (name.find("transmission") != std::string_view::npos ||
            name.find("gearbox") != std::string_view::npos ||
            name.find("gear_box") != std::string_view::npos)
            return K::Transmission;
        if (name.find("engine") != std::string_view::npos ||
            name.find("motor_dm") != std::string_view::npos)
            return K::Engine;
        return K::Body;
    }
}
