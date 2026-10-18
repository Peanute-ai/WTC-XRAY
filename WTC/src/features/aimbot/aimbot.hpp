#pragma once
#include <corecrt_math_defines.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include "ground_prediction.hpp"

namespace aimbot
{
    struct AimCacheEntry
    {
        vec3_t aim_point{};
        vec3_t raw_aim_point{};
        bool initialized = false;
        bool have_motion = false;
        uintptr_t owner = 0, movement = 0;
        vec3_t velocity{}, acceleration{}, last_position{};
        std::chrono::steady_clock::time_point last_seen{}, last_read{}, last_sample{}, last_solve{}, last_render{};
    };

    struct BallisticsData
    {
        float mass = 0.0f;
        float caliber = 0.0f;
        float velocity = 0.0f;
        float length = 0.0f;
        float drag = 0.0f;
        float max_dist = 0.0f;
        bool caliber_in_mm = false; // weapon tables convert to mm; global fallback retains meters
    };

    inline BallisticsData ballisticsData;

    // -----------------------------------------------------------------------
    // Utilidades de armamento
    // -----------------------------------------------------------------------
    struct WeaponInfoData {
        float mass = 0.0f;
        float caliber = 0.0f;
        float velocity = 0.0f;
        float length = 0.0f;
        float drag = 0.0f;
        float max_dist = 0.0f;
    };

    inline bool TryGetWeaponInfoFromGun(uintptr_t gun, WeaponInfoData& out)
    {
        if (!gun) return false;

        uintptr_t wInfo = TargetProcess->Read<uintptr_t>(gun + offsets::weapon_offsets::weapon_info);
        if (!wInfo) return false;

        int8_t activeSlot = TargetProcess->Read<int8_t>(gun + offsets::weapon_offsets::active_slot);
        uintptr_t slotBase = wInfo + static_cast<uintptr_t>(static_cast<int>(activeSlot)) * 0xA0;

        uintptr_t ammoArr = TargetProcess->Read<uintptr_t>(slotBase + offsets::weapon_info_offsets::ammo);
        int32_t ammoCount = TargetProcess->Read<int32_t>(slotBase + 0xA8);
        if (ammoCount >= 9) {
            ammoArr = TargetProcess->Read<uintptr_t>(ammoArr);
        }

        uintptr_t ammo = TargetProcess->Read<uintptr_t>(ammoArr);
        if (!ammo) return false;

        float speed = TargetProcess->Read<float>(ammo + 0x20);
        int propsId = TargetProcess->Read<int32_t>(ammo + 0x4);
        if (propsId == -1) propsId = TargetProcess->Read<int32_t>(ammo + 0x0);

        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        if (!module_base) return false;

        if (speed <= 0.0f) {
            uintptr_t bulletTable = TargetProcess->Read<uintptr_t>(module_base + offsets::bullet_props_positive_offset);
            if (propsId < 0) bulletTable = TargetProcess->Read<uintptr_t>(module_base + offsets::bullet_props_negative_offset);

            if (bulletTable && propsId != 0) {
                uint32_t id = static_cast<uint32_t>(propsId) & 0x7FFFFFFF;
                uint32_t maxId = TargetProcess->Read<uint32_t>(bulletTable + 0x60);
                if (id < maxId) {
                    uintptr_t pagePtr = TargetProcess->Read<uintptr_t>(bulletTable + (id >> 27) * 8);
                    if (pagePtr) {
                        uintptr_t entry = TargetProcess->Read<uintptr_t>(pagePtr + (id & 0x7FFFFFF) * 8);
                        if (entry) {
                            speed = TargetProcess->Read<float>(entry + 0x20);
                        }
                    }
                }
            }
        }

        if (speed <= 0.0f) return false;

        uintptr_t projTable = TargetProcess->Read<uintptr_t>(module_base + offsets::proj_ballistics_props_positive_offset);
        if (propsId < 0) projTable = TargetProcess->Read<uintptr_t>(module_base + offsets::proj_ballistics_props_negative_offset);

        float mass = 0.0f, caliber = 0.0f, drag = 0.0f, maxDist = 0.0f;

        if (projTable && propsId != 0) {
            uint32_t id = static_cast<uint32_t>(propsId) & 0x7FFFFFFF;
            uint32_t maxId = TargetProcess->Read<uint32_t>(projTable + 0x60);
            if (id < maxId) {
                uintptr_t pagePtr = TargetProcess->Read<uintptr_t>(projTable + (id >> 27) * 8);
                if (pagePtr) {
                    uintptr_t entry = TargetProcess->Read<uintptr_t>(pagePtr + (id & 0x7FFFFFF) * 8);
                    if (entry) {
                        mass = TargetProcess->Read<float>(entry + 0x4);
                        caliber = TargetProcess->Read<float>(entry + 0x8);
                        drag = TargetProcess->Read<float>(entry + 0xC);
                        maxDist = TargetProcess->Read<float>(entry + 0x10);
                    }
                }
            }
        }

        out.velocity = speed;
        out.mass = mass;
        out.caliber = caliber * 1000.0f;   // milímetros
        out.length = caliber;              // metros (aprox)
        out.drag = drag;
        out.max_dist = maxDist;
        return true;
    }

    inline bool GetAircraftBallisticsInfoEx(int weapon_index = -1, bool use_heaviest = false)
    {
        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        if (!module_base) return false;

        uintptr_t raw_unit = TargetProcess->Read<uintptr_t>(module_base + offsets::my_unit_offset);
        uintptr_t local_unit = (raw_unit & 1) ? raw_unit - 1 : raw_unit;
        if (!local_unit) return false;

        uintptr_t armory = TargetProcess->Read<uintptr_t>(local_unit + offsets::unit_offsets::turret_offset);
        if (!armory) return false;

        uintptr_t weapons = TargetProcess->Read<uintptr_t>(armory + offsets::armory_offsets::weapons);
        if (!weapons) return false;

        int32_t weapon_count = TargetProcess->Read<int32_t>(armory + offsets::armory_offsets::weapon_count);
        if (weapon_count <= 0) return false;

        WeaponInfoData selected_info;
        bool found = false;

        if (weapon_index >= 0 && weapon_index < weapon_count) {
            uintptr_t gun = TargetProcess->Read<uintptr_t>(weapons + weapon_index * sizeof(uintptr_t));
            found = TryGetWeaponInfoFromGun(gun, selected_info);
        }
        else if (use_heaviest) {
            float best_caliber = -1.0f;
            for (int i = 0; i < weapon_count; ++i) {
                uintptr_t gun = TargetProcess->Read<uintptr_t>(weapons + i * sizeof(uintptr_t));
                WeaponInfoData info;
                if (TryGetWeaponInfoFromGun(gun, info)) {
                    if (info.caliber > best_caliber) {
                        best_caliber = info.caliber;
                        selected_info = info;
                        found = true;
                    }
                }
            }
        }
        else {
            // automático: usar primera arma que funcione (comportamiento anterior)
            for (int i = 0; i < weapon_count; ++i) {
                uintptr_t gun = TargetProcess->Read<uintptr_t>(weapons + i * sizeof(uintptr_t));
                if (TryGetWeaponInfoFromGun(gun, selected_info)) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) return false;

        ballisticsData.velocity = selected_info.velocity;
        ballisticsData.mass = selected_info.mass;
        ballisticsData.caliber = selected_info.caliber;
        ballisticsData.caliber_in_mm = true;
        ballisticsData.length = selected_info.length;
        ballisticsData.drag = selected_info.drag;
        ballisticsData.max_dist = selected_info.max_dist;

        return true;
    }

    inline bool GetAircraftBallisticsInfo()
    {
        return GetAircraftBallisticsInfoEx(sdk::air_lead_weapon_selection, sdk::air_lead_use_heaviest_weapon);
    }

    inline bool GetLocalWeaponLocalPosition(vec3_t& out)
    {
        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        if (!module_base) return false;

        uintptr_t raw_unit = TargetProcess->Read<uintptr_t>(module_base + offsets::my_unit_offset);
        uintptr_t local_unit = (raw_unit & 1) ? raw_unit - 1 : raw_unit;
        if (!local_unit) return false;

        uintptr_t armory = TargetProcess->Read<uintptr_t>(local_unit + offsets::unit_offsets::turret_offset);
        if (!armory) return false;

        uintptr_t weapon_pos_arr = TargetProcess->Read<uintptr_t>(armory + offsets::armory_offsets::weapon_position_info);
        int32_t   weapon_pos_count = TargetProcess->Read<int32_t>(armory + offsets::armory_offsets::weapon_position_info_count);
        if (!weapon_pos_arr || weapon_pos_count < 1) return false;

        out = TargetProcess->Read<vec3_t>(weapon_pos_arr + offsets::weapon_position_info_offsets::position);
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z) && out.length() < 100.0f;
    }

    // -----------------------------------------------------------------------
    // Obtener datos balísticos para tanques (cañón principal)
    // -----------------------------------------------------------------------
    inline auto GetBallisticsInfo() -> void
    {
        // Intentar obtener el cañón principal (mayor calibre) desde Armory
        const uintptr_t module_base = TargetProcess->GetBaseAddress("aces.exe");
        uintptr_t local_unit = 0;
        if (module_base) {
            uintptr_t raw_unit = TargetProcess->Read<uintptr_t>(module_base + offsets::my_unit_offset);
            local_unit = (raw_unit & 1) ? raw_unit - 1 : raw_unit;
        }

        bool found = false;
        if (local_unit) {
            uintptr_t armory = TargetProcess->Read<uintptr_t>(local_unit + offsets::unit_offsets::turret_offset);
            if (armory) {
                uintptr_t weapons = TargetProcess->Read<uintptr_t>(armory + offsets::armory_offsets::weapons);
                int32_t weapon_count = TargetProcess->Read<int32_t>(armory + offsets::armory_offsets::weapon_count);
                if (weapons && weapon_count > 0) {
                    WeaponInfoData best_info;
                    float best_caliber = -1.0f;
                    for (int i = 0; i < weapon_count; ++i) {
                        uintptr_t gun = TargetProcess->Read<uintptr_t>(weapons + i * sizeof(uintptr_t));
                        WeaponInfoData info;
                        if (TryGetWeaponInfoFromGun(gun, info)) {
                            if (info.caliber > best_caliber) {
                                best_caliber = info.caliber;
                                best_info = info;
                                found = true;
                            }
                        }
                    }
                    if (found) {
                        ballisticsData.mass = best_info.mass;
                        ballisticsData.caliber = best_info.caliber;
                        ballisticsData.caliber_in_mm = true;
                        ballisticsData.velocity = best_info.velocity;
                        ballisticsData.length = best_info.length;
                        ballisticsData.max_dist = best_info.max_dist;
                        ballisticsData.drag = best_info.drag;
                    }
                }
            }
        }

        // Fallback al contenedor global si no se encontró cañón
        if (!found) {
            ballisticsData.mass = sdk::cGame->ballistics->getMass();
            ballisticsData.caliber = sdk::cGame->ballistics->getCaliber();
            ballisticsData.caliber_in_mm = false;
            ballisticsData.velocity = sdk::cGame->ballistics->getVelocity();
            ballisticsData.length = sdk::cGame->ballistics->getLength();
            ballisticsData.max_dist = sdk::cGame->ballistics->getMaxDistance();
            ballisticsData.drag = 0.0f;
        }
    }

    // -----------------------------------------------------------------------
    // BeginFrame
    // -----------------------------------------------------------------------
    inline auto BeginFrame() -> void
    {
        static std::chrono::steady_clock::time_point next_update{};
        const auto now = std::chrono::steady_clock::now();
        if (now < next_update)
            return;

        bool aircraft = GetAircraftBallisticsInfo();
        if (!aircraft) {
            GetBallisticsInfo();
        }

        next_update = now + std::chrono::milliseconds(50);
    }

    // Only the selected ground target reaches this path: one fixed-size cache.
    // Solving and DMA are paced independently of rendering (60/120 ms).
    inline auto run(c_unit& unit, vec3_t unit_position, vec3_t local_position,
        ViewMatrix_t camera_matrix, std::chrono::steady_clock::time_point captured_at) -> void {
        static AimCacheEntry cache{};
        static std::chrono::steady_clock::time_point next_work{}; // survives target switches
        const auto now = std::chrono::steady_clock::now();
        const auto base = unit.get_base();
        if (cache.owner != base || now-cache.last_seen > std::chrono::milliseconds(500)) {
            cache = {};
            cache.owner = base;
        }
        cache.last_seen = now;
        if (!ground_prediction::finite(unit_position) || !ground_prediction::finite(local_position)) return;
        const float sample_age = std::chrono::duration<float>(now-captured_at).count();
        if (sample_age < 0 || sample_age > 0.25f) { cache.initialized = false; return; }
        // Cheap discontinuity check on every frame, before reusing a previous marker.
        const float position_dt = std::chrono::duration<float>(captured_at-cache.last_sample).count();
        if (cache.have_motion && (position_dt < 0 ||
            (unit_position-cache.last_position).length() > 5.0f + cache.velocity.length()*std::max(0.0f,position_dt)*2.0f)) {
            cache.have_motion = false;
            cache.initialized = false;
            cache.acceleration = {};
        }
        if (now >= next_work) {
            next_work = now + std::chrono::milliseconds(60); // throttle failures and target switches too
            uintptr_t movement = 0;
            vec3_t velocity{};
            if (!TargetProcess->Read(base + offsets::unit_offsets::groundmovement_offset, &movement, sizeof(movement))
                || !movement || !TargetProcess->Read(movement + 0x5C, &velocity, sizeof(velocity))
                || !ground_prediction::finite(velocity) || velocity.length() > 150.0f) {
                cache.initialized = false;
                cache.have_motion = false;
                return;
            }
            const float dt = std::chrono::duration<float>(now-cache.last_read).count();
            if (cache.movement != movement) {
                cache.have_motion = false;
                cache.initialized = false;
            }
            if (cache.have_motion && dt >= 0.025f && dt <= 0.3f) {
                vec3_t measured = (velocity-cache.velocity)/dt;
                measured.y = 0;
                if (ground_prediction::finite(measured) && measured.length() < 25.0f) {
                    const float alpha = 1.0f-std::exp(-dt/0.18f);
                    cache.acceleration += (measured-cache.acceleration)*alpha;
                } else cache.acceleration = {};
            } else cache.acceleration = {};
            cache.movement = movement;
            cache.velocity = velocity;
            cache.last_read = now;
            cache.last_sample = captured_at;
            cache.last_position = unit_position;
            cache.have_motion = true;
            const float speed = velocity.length();
            const vec3_t historical_acceleration = cache.acceleration;
            if (speed < 0.2f) cache.acceleration = {};
            next_work = now + std::chrono::milliseconds(speed > 0.1f ? 60 : 120);
            // Preserve existing marker height; ballistic drop is handled only by the solver.
            const float dx = unit_position.x-local_position.x, dz = unit_position.z-local_position.z;
            vec3_t target = ground_prediction::align_position(unit_position,velocity,historical_acceleration,sample_age);
            target.y += 1.0f + std::sqrt(dx*dx+dz*dz)*(0.143f/150.0f);
            const ground_prediction::Ballistics b{ballisticsData.velocity, ballisticsData.mass,
                ballisticsData.caliber * (ballisticsData.caliber_in_mm ? 0.001f : 1.0f),
                ballisticsData.length, ballisticsData.drag};
            const auto solution = ground_prediction::solve(local_position,target,velocity,cache.acceleration,b);
            if (!solution.valid) { cache.initialized = false; return; }
            cache.raw_aim_point = solution.point;
            cache.last_solve = now;
            if (!cache.initialized) {
                cache.aim_point = solution.point;
                cache.last_render = now;
                cache.initialized = true;
            }
        }
        if (!cache.initialized) return;
        const float render_dt = std::clamp(std::chrono::duration<float>(now-cache.last_render).count(),0.0f,0.1f);
        const float raw_age = std::clamp(std::chrono::duration<float>(now-cache.last_solve).count(),0.0f,0.15f);
        cache.last_render = now;
        const vec3_t live = cache.raw_aim_point + cache.velocity*raw_age;
        // Time-invariant smoothing with velocity feed-forward, not a frame-dependent lag.
        cache.aim_point += cache.velocity*render_dt;
        cache.aim_point += (live-cache.aim_point)*(1.0f-std::exp(-render_dt/0.045f));
        vec2_t screen;
        if (!g_render->world_to_screen(cache.aim_point,screen,camera_matrix)) return;
        g_render->circle_filled(screen.x,screen.y,3.0f,IM_COL32(255,0,0,255),20);
        vec2_t unitScreen;
        if (g_render->world_to_screen(unit_position,unitScreen,camera_matrix))
            g_render->line(unitScreen.x,unitScreen.y,screen.x,screen.y,IM_COL32(255,0,0,24),1.0f);
    }
}
