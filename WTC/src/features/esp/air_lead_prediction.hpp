#pragma once

#include <algorithm>
#include <cmath>

// Modelo exclusivo del indicador aéreo; no modifica el predictor del aim terrestre.
namespace esp::air_lead_model
{
    inline bool finite(const vec3_t& p)
    {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }

    inline float distance(const vec3_t& p)
    {
        return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    }

    // Aceleración constante sólo durante un horizonte corto: extrapolar 5 s
    // con la última aceleración de un avión en giro genera puntos absurdos.
    inline vec3_t propagate(const vec3_t& pos, const vec3_t& vel,
        const vec3_t& acceleration, float seconds, bool turn = false)
    {
        seconds = std::clamp(seconds, 0.0f, 8.0f);
        const float active = std::min(seconds, 0.85f);
        if (!turn || vel.x * vel.x + vel.z * vel.z < 400.0f)
            return pos + vel * seconds + acceleration * (active * (seconds - 0.5f * active));

        // omega=(vx*az-vz*ax)/|v_horizontal|²; dimensiones rad/s.
        // Al rotar la velocidad no se vuelve a sumar la aceleración lateral.
        const float speed_sq = vel.x * vel.x + vel.z * vel.z;
        const float omega = std::clamp((vel.x * acceleration.z - vel.z * acceleration.x) / speed_sq,
            -0.7f, 0.7f);
        const float speed = std::sqrt(speed_sq);
        const float along = (vel.x * acceleration.x + vel.z * acceleration.z) / speed;
        const float theta = std::atan2(vel.z, vel.x);
        vec3_t result = pos;
        float elapsed = 0.0f;
        float x_speed = vel.x, z_speed = vel.z;
        while (elapsed < seconds)
        {
            const float dt = std::min(0.05f, seconds - elapsed);
            const float mid = elapsed + 0.5f * dt;
            const float turn_time = std::min(mid, active);
            const float heading = theta + omega * turn_time;
            const float magnitude = std::max(0.0f, speed + along * turn_time);
            x_speed = magnitude * std::cos(heading);
            z_speed = magnitude * std::sin(heading);
            result.x += x_speed * dt;
            result.z += z_speed * dt;
            elapsed += dt;
        }
        result.y += vel.y * seconds + acceleration.y * (active * (seconds - 0.5f * active));
        return result;
    }

    struct solution_t
    {
        vec3_t point{};
        float time = 0.0f;
        bool valid = false;
    };

    inline solution_t solve(const vec3_t& origin, const vec3_t& target,
        const vec3_t& velocity, const vec3_t& acceleration,
        float muzzle_speed, float mass, float caliber_mm, float drag_area,
        float prelaunch_seconds = 0.0f, bool turn = false)
    {
        solution_t out{};
        if (!finite(origin) || !finite(target) || !finite(velocity) || !finite(acceleration) ||
            !std::isfinite(muzzle_speed) || muzzle_speed < 50.0f || muzzle_speed > 4000.0f)
            return out;

        // drag_area es área efectiva; si faltan datos de masa, degradar a
        // balística sin drag en vez de dividir por cero o acelerar la bala.
        float area = drag_area;
        if ((!std::isfinite(area) || area <= 0.0f) &&
            std::isfinite(caliber_mm) && caliber_mm > 0.0f)
        {
            const float radius = caliber_mm * 0.0005f;
            area = 0.32f * 3.14159265f * radius * radius;
        }
        float drag_k = 0.0f;
        if (std::isfinite(mass) && mass > 0.00001f && std::isfinite(area) && area > 0.0f)
        {
            const float altitude = std::max(0.0f, 0.5f * (origin.y + target.y));
            const float density = 1.225f * std::exp(-altitude / 9000.0f);
            drag_k = std::clamp(0.5f * density * area / mass, 0.0f, 0.02f);
        }

        const float delay = std::clamp(prelaunch_seconds, 0.0f, 0.75f);
        const auto travel = [&](float t) {
            // dv/dt=-k*v² => recorrido ln(1+k*v0*t)/k, disipativo.
            return drag_k < 1e-7f ? muzzle_speed * t : std::log1p(drag_k * muzzle_speed * t) / drag_k;
        };
        const auto aim_at = [&](float t) {
            const vec3_t p = propagate(target, velocity, acceleration, t + delay, turn);
            return p + vec3_t{ 0.0f, 4.905f * t * t, 0.0f }; // compensar gravedad Y negativa
        };
        const auto residual = [&](float t) { return distance(aim_at(t) - origin) - travel(t); };

        // Primer cruce del cono balístico; no asumir monotonía si el avión
        // se aleja más deprisa que el proyectil o cambia de dirección.
        float lo = 0.0f, hi = 0.0f;
        float previous = residual(0.0f);
        if (!std::isfinite(previous) || previous < 0.01f)
            return out;
        for (int step = 1; step <= 160; ++step)
        {
            const float t = 0.05f * static_cast<float>(step);
            const float error = residual(t);
            if (!std::isfinite(error)) return out;
            if (error <= 0.0f)
            {
                hi = t;
                lo = t - 0.05f;
                break;
            }
        }
        if (hi == 0.0f) return out; // objetivo fuera de alcance: no mostrar predicción falsa
        for (int i = 0; i < 22; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            if (residual(mid) > 0.0f) lo = mid;
            else hi = mid;
        }
        out.time = 0.5f * (lo + hi);
        out.point = aim_at(out.time);
        out.valid = finite(out.point);
        return out;
    }
}
