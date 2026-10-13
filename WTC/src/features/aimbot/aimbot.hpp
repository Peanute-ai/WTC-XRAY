#pragma once
#include <corecrt_math_defines.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace aimbot
{
    struct AimCacheEntry
    {
        vec3_t aim_point{};
        vec3_t raw_aim_point{};
        std::chrono::steady_clock::time_point next_update{};
        bool initialized = false;
    };

    struct BallisticsData
    {
        float mass = 0.0f;
        float caliber = 0.0f;
        float velocity = 0.0f;
        float length = 0.0f;
        float drag = 0.0f;
        float max_dist = 0.0f;
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

    // -----------------------------------------------------------------------
    // Predictor balístico (sin cambios)
    // -----------------------------------------------------------------------
    class BallisticsPrediction {
    private:
        static constexpr float GRAVITY = -9.81f;
        static constexpr float TIME_STEP = 1.0f / 96.0f;
        static constexpr float MAX_SIMULATION_TIME = 10.0f;
        static constexpr float CONVERGENCE_THRESHOLD = 0.01f;
        static constexpr int MAX_ITERATIONS = 50;

        float GetDragConstant(float altitude) const {
            const float max_alt = 18300.0f;
            const float alt_mult = 1.225f;
            const auto clamped_alt = (std::fmin)(altitude, max_alt);

            const float c1 = 2.2871901e-19f;
            const float c2 = 5.8355603e-14f;
            const float c3 = 0.00000000353118f;
            const float c4 = 0.000095938703f;

            return alt_mult *
                ((max_alt / (std::fmax)(altitude, max_alt)) *
                    ((((((c1 * clamped_alt) - c2) * clamped_alt) + c3) *
                        clamped_alt) -
                        c4) *
                    clamped_alt +
                    1.0f);
        }

        float GetMachDragFactor(float speed, float altitude) const {
            (void)altitude;
            if (speed <= 0.01f) return 1.0f;

            const float speed_of_sound = 343.0f;
            const float mach = speed / speed_of_sound;

            if (mach < 0.80f) return 1.00f;
            if (mach < 1.20f) {
                const float t = (mach - 0.80f) / 0.40f;
                return 1.0f + 1.8f * t;
            }
            return 2.80f / std::sqrt(mach);
        }

        float GetBallisticCoefficientAtSpeed(float speed, const BallisticsData& data,
            float dragConstant, float altitude) const {
            if (data.drag != 0.0f) {
                const float mach_factor = GetMachDragFactor(speed, altitude);
                return -1.0f * (dragConstant * (data.drag * mach_factor)) / data.mass;
            }

            return -1.0f * (dragConstant * static_cast<float>(M_PI) * 0.5f *
                std::pow(data.caliber * 0.5f, 2.0f) * data.length) / data.mass;
        }

        float GetBallisticCoefficient(const BallisticsData& data, float dragConstant) const {
            return GetBallisticCoefficientAtSpeed(data.velocity, data, dragConstant, 0.0f);
        }

        void ApplyPhysics(vec3_t& position, vec3_t& velocity, const BallisticsData& data,
            float dragConstant, float deltaTime) const {
            float speed = velocity.length();
            if (speed > 0.01f) {
                const float ballistic_coeff =
                    GetBallisticCoefficientAtSpeed(speed, data, dragConstant, position.y);

                const float drag_force = ballistic_coeff * speed * speed;
                const vec3_t normalized_vel = velocity.normalized();
                const vec3_t drag_acceleration = normalized_vel * (-drag_force);

                velocity = velocity + drag_acceleration * deltaTime;
            }

            velocity.y += GRAVITY * deltaTime;
            position = position + velocity * deltaTime;
        }

        bool SimulateTrajectory(vec3_t start_pos, vec3_t initial_velocity, vec3_t target_pos,
            const BallisticsData& data, float drag_constant, float max_time,
            vec3_t& hit_point, float& hit_time) const {
            vec3_t pos = start_pos;
            vec3_t vel = initial_velocity;
            float time = 0.0f;
            vec3_t last_pos = pos;

            while (time < max_time) {
                last_pos = pos;
                ApplyPhysics(pos, vel, data, drag_constant, TIME_STEP);
                time += TIME_STEP;

                const vec3_t to_target = target_pos - start_pos;
                const vec3_t to_current = pos - start_pos;

                const float target_dist_2d =
                    std::sqrt(to_target.x * to_target.x + to_target.z * to_target.z);
                const float current_dist_2d =
                    std::sqrt(to_current.x * to_current.x + to_current.z * to_current.z);

                if (current_dist_2d >= target_dist_2d) {
                    const float last_dist_2d =
                        std::sqrt((last_pos - start_pos).x * (last_pos - start_pos).x +
                            (last_pos - start_pos).z * (last_pos - start_pos).z);

                    const float alpha =
                        (target_dist_2d - last_dist_2d) /
                        std::max(0.0001f, current_dist_2d - last_dist_2d);

                    hit_point = last_pos + (pos - last_pos) * alpha;
                    hit_time = time - TIME_STEP + TIME_STEP * alpha;
                    return true;
                }

                if (pos.y < (std::fmin)(start_pos.y - 1000.0f, target_pos.y - 100.0f))
                    break;
            }

            hit_point = pos;
            hit_time = time;
            return false;
        }

        vec3_t PredictTargetPosition(const vec3_t& target_pos, const vec3_t& target_vel,
            const vec3_t& target_accel, float time,
            float turn_rate, bool use_turn) const {
            if (!use_turn || std::abs(turn_rate) < 0.0001f) {
                return target_pos + target_vel * time +
                    target_accel * (0.5f * time * time);
            }

            const int steps = (std::max)(1, static_cast<int>(time / 0.05f));
            const float dt = time / static_cast<float>(steps);

            vec3_t pos = target_pos;
            vec3_t vel = target_vel;

            for (int i = 0; i < steps; ++i) {
                pos = pos + vel * dt + target_accel * (0.5f * dt * dt);

                const float angle = turn_rate * dt;
                const float c = std::cos(angle);
                const float s = std::sin(angle);

                const float vx = vel.x * c - vel.z * s;
                const float vz = vel.x * s + vel.z * c;
                vel = { vx, vel.y, vz };
            }

            return pos;
        }

        float ComputeTurnRate(const vec3_t& velocity, const vec3_t& acceleration) const {
            const float speed = velocity.length();
            if (speed < 1.0f) return 0.0f;

            const vec3_t vn = velocity / speed;
            const float dot = vn.x * acceleration.x + vn.y * acceleration.y + vn.z * acceleration.z;
            const vec3_t perp = acceleration - vn * dot;

            const float horizontal_speed =
                std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
            if (horizontal_speed < 1.0f || perp.length() < 0.1f) return 0.0f;

            const float cross_y = vn.x * perp.z - vn.z * perp.x;
            return cross_y / (horizontal_speed * horizontal_speed);
        }

    public:
        vec3_t PredictInterceptPoint(vec3_t shooter_pos, vec3_t target_pos, vec3_t target_velocity,
            const BallisticsData& ballistics,
            vec3_t target_acceleration = vec3_t{},
            bool use_turn = false) const {
            if (ballistics.velocity <= 0.0f) return target_pos;

            const float altitude = (std::max)(shooter_pos.y, target_pos.y);
            const float drag_constant = GetDragConstant(altitude);

            const float turn_rate =
                use_turn ? ComputeTurnRate(target_velocity, target_acceleration) : 0.0f;

            vec3_t predicted_target_pos = target_pos;

            for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
                const vec3_t direction = (predicted_target_pos - shooter_pos).normalized();
                const vec3_t initial_velocity = direction * ballistics.velocity;

                vec3_t hit_point;
                float hit_time;

                if (SimulateTrajectory(shooter_pos, initial_velocity, predicted_target_pos,
                    ballistics, drag_constant, MAX_SIMULATION_TIME,
                    hit_point, hit_time)) {
                    const vec3_t new_prediction =
                        PredictTargetPosition(target_pos, target_velocity, target_acceleration,
                            hit_time, turn_rate, use_turn);

                    const float prediction_error = (new_prediction - predicted_target_pos).length();
                    if (prediction_error < CONVERGENCE_THRESHOLD) {
                        return new_prediction;
                    }

                    const float drop = 0.5f * std::abs(GRAVITY) * hit_time * hit_time;
                    predicted_target_pos = new_prediction;
                    predicted_target_pos.y += drop;
                }
                else {
                    const float estimated_time = shooter_pos.dist_to(predicted_target_pos) / ballistics.velocity;
                    predicted_target_pos =
                        PredictTargetPosition(target_pos, target_velocity, target_acceleration,
                            estimated_time, turn_rate, use_turn);
                    break;
                }
            }

            return predicted_target_pos;
        }

        float CalculateOptimalAngle(vec3_t shooter_pos, vec3_t target_pos, float muzzle_velocity,
            const BallisticsData& ballistics) const {
            vec3_t displacement = target_pos - shooter_pos;
            float horizontal_dist = std::sqrt(displacement.x * displacement.x + displacement.z * displacement.z);
            float vertical_dist = displacement.y;

            float altitude = (std::max)(shooter_pos.y, target_pos.y);
            float drag_constant = GetDragConstant(altitude);

            float low_angle = -M_PI / 6.0f;
            float high_angle = M_PI / 3.0f;
            float best_angle = 0.0f;
            float min_error = FLT_MAX;

            for (int i = 0; i < 30; ++i) {
                const float test_angle = (low_angle + high_angle) * 0.5f;

                vec3_t direction = vec3_t(
                    horizontal_dist > 0.0f ? displacement.x / horizontal_dist : 1.0f,
                    std::sin(test_angle),
                    horizontal_dist > 0.0f ? displacement.z / horizontal_dist : 0.0f
                ).normalized();

                vec3_t initial_velocity = direction * muzzle_velocity;
                vec3_t hit_point;
                float hit_time;

                if (SimulateTrajectory(shooter_pos, initial_velocity, target_pos,
                    ballistics, drag_constant, MAX_SIMULATION_TIME,
                    hit_point, hit_time)) {
                    const float error = (hit_point - target_pos).length();
                    if (error < min_error) {
                        min_error = error;
                        best_angle = test_angle;
                    }

                    if (hit_point.y > target_pos.y) high_angle = test_angle;
                    else low_angle = test_angle;
                }
                else {
                    high_angle = test_angle;
                }
            }

            return best_angle;
        }

        vec3_t GetAimPoint(vec3_t shooter_pos, vec3_t target_pos,
            const BallisticsData& ballistics) const {
            vec3_t direction = target_pos - shooter_pos;
            const float distance = direction.length();
            direction.normalized();

            const float altitude = (std::max)(shooter_pos.y, target_pos.y);
            const float drag_constant = GetDragConstant(altitude);
            const float ballistic_coeff = GetBallisticCoefficient(ballistics, drag_constant);

            const float flight_time = distance / ballistics.velocity;
            const float drop = 0.5f * std::abs(GRAVITY) * flight_time * flight_time;
            const float drag_effect = ballistic_coeff * distance * 0.1f;

            vec3_t aim_point = target_pos;
            aim_point.y += drop + drag_effect;
            return aim_point;
        }
    };

    // -----------------------------------------------------------------------
    // run() para aimbot terrestre (sin cambios)
    // -----------------------------------------------------------------------
    inline auto run(c_unit& unit, vec3_t unit_position, vec3_t local_position, ViewMatrix_t camera_matrix) -> void {
        static BallisticsPrediction pred;
        static std::unordered_map<uintptr_t, AimCacheEntry> aim_cache;
        constexpr float smooth_alpha = 0.30f;

        const auto now = std::chrono::steady_clock::now();
        const uintptr_t unit_base = unit.get_base();
        auto& cache = aim_cache[unit_base];

        vec3_t targetVelocity = unit.get_movement_ground().velocity();
        const float targetSpeed = targetVelocity.length();

        if (now >= cache.next_update)
        {
            float horizontalDist = std::sqrt(
                (unit_position.x - local_position.x) * (unit_position.x - local_position.x) +
                (unit_position.z - local_position.z) * (unit_position.z - local_position.z)
            );

            float distanceFactor = horizontalDist / 150.0f;
            float extraOffset = distanceFactor * 0.143f;

            vec3_t adjusted_position = unit_position;
            adjusted_position.y += 1.0f + extraOffset;

            if (targetSpeed > 0.1f)
                cache.raw_aim_point = pred.PredictInterceptPoint(local_position, adjusted_position, targetVelocity, ballisticsData);
            else
                cache.raw_aim_point = pred.GetAimPoint(local_position, adjusted_position, ballisticsData);

            if (!cache.initialized)
            {
                cache.aim_point = cache.raw_aim_point;
                cache.initialized = true;
            }

            cache.next_update = now + (targetSpeed > 0.1f ? std::chrono::milliseconds(60) : std::chrono::milliseconds(120));
        }

        if (cache.initialized)
            cache.aim_point = cache.aim_point + (cache.raw_aim_point - cache.aim_point) * smooth_alpha;

        const vec3_t& aimPoint = cache.aim_point;

        vec2_t screen;
        if (!g_render->world_to_screen(aimPoint, screen, camera_matrix))
            return;

        g_render->circle_filled(screen.x, screen.y, 3.0f, IM_COL32(255, 0, 0, 255), 20);

        vec2_t unitScreen;
        if (g_render->world_to_screen(unit_position, unitScreen, camera_matrix))
            g_render->line(unitScreen.x, unitScreen.y, screen.x, screen.y, IM_COL32(255, 0, 0, 24), 1.0f);
    }
}