#pragma once

#include "offsets.hpp"
#include "datatypes\vector2.hpp"
#include "datatypes\vector3.hpp"
#include "datatypes\matrix.hpp"

#include <string>
#include <vector>

#define OFFSETS_WITH_BA(type, offset) \
    TargetProcess->Read< type >(this->base_address + (offset));

#define OFFSETS(type, offset) \
    TargetProcess->Read< type >((offset));

// classes
#include "classes\ballistic.hpp"
#include "classes\camera.hpp"
#include "classes\game.hpp"
#include "classes\movement.hpp"
#include "classes\info.hpp"
#include "classes\units.hpp"
#include "classes\entity.hpp"

enum GuiState : std::uint8_t {
	NONE = 0,
	MENU = 1,
	ALIVE = 2,
	DEAD = 3,
	SPEC = 6,
	SPAWN_MENU = 8,
	LOADING = 10
};

namespace sdk
{
	inline int screen_width = GetSystemMetrics(SM_CXSCREEN);
	inline int screen_height = GetSystemMetrics(SM_CYSCREEN);
	inline int monitor_count = 1;
	inline int monitor_index = 0;
	inline bool auto_fullscreen_on_start = false;
	inline bool request_monitor_apply = false;
	inline bool request_manual_reload = false;
	inline std::string manual_reload_status = "";
	inline std::vector<std::string> monitor_labels = { "Display 1" };

	inline bool  esp_enabled = true;
	inline bool  radar_enabled = true; // Marcadores enemigos sobre el mapa/minimapa nativo.
	inline ImVec4 radar_enemy_color = ImVec4(1.0f, 0.255f, 0.255f, 0.96f);
	inline bool  aimbot_enabled = false;
	inline float esp_render_distance = 2000.0f;
	inline float esp_box_distance = 2000.0f;
	inline bool  esp_show_boxes = true;
	inline bool  esp_use_3d_boxes = true;
	inline bool  esp_show_vehicle_names = true;
	inline bool  esp_show_distance = true;
	inline bool  esp_show_reload = false;
	inline bool  esp_show_reload_seconds = false;
	inline bool  esp_show_fps = true;
	inline bool  esp_vsync_enabled = false;
	inline int   esp_vsync_fps = 120;
	inline float esp_font_scale = 1.0f;
	inline bool  esp_show_bomb_impact = true;

	// Filtros de vehículos
	inline bool show_helicopter = true;
	inline bool show_bomber = true;
	inline bool show_fighter = true;
	inline bool show_attacker = true;
	inline bool show_light_tank = true;
	inline bool show_heavy_tank = true;
	inline bool show_medium_tank = true;
	inline bool show_spaa = true;
	inline bool show_gun_boat = true;
	inline bool show_destroyer = true;
	inline bool show_cruiser = true;
	inline bool show_torpedo_boat = true;
	inline bool show_recon_drone = true;

	inline int death_state_threshold = 1;   // 1 o 2

	// Colores de vehículos
	inline ImVec4 color_helicopter = ImVec4(247.f / 255.f, 163.f / 255.f, 159.f / 255.f, 1.f);
	inline ImVec4 color_bomber = ImVec4(155.f / 255.f, 165.f / 255.f, 237.f / 255.f, 1.f);
	inline ImVec4 color_fighter = ImVec4(241.f / 255.f, 158.f / 255.f, 112.f / 255.f, 1.f);
	inline ImVec4 color_attacker = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
	inline ImVec4 color_light_tank = ImVec4(246.f / 255.f, 234.f / 255.f, 237.f / 255.f, 1.f);
	inline ImVec4 color_heavy_tank = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
	inline ImVec4 color_medium_tank = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
	inline ImVec4 color_spaa = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
	inline ImVec4 color_gun_boat = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
	inline ImVec4 color_destroyer = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
	inline ImVec4 color_cruiser = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
	inline ImVec4 color_torpedo_boat = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
	inline ImVec4 color_recon_drone = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);

	// X-Ray (Damage Model)
	inline bool bShowDamageModel = false;
	inline bool bShowModuleNames = false;
	inline float xray_render_distance = 1000.0f;
	inline float xray_names_distance = 500.0f;

	inline bool bShowModuleGunBarrel = true;
	inline bool bShowModuleCannonBreech = true;
	inline bool bShowModuleCrew = true;
	inline bool bShowModuleFuelTank = true;
	inline bool bShowModuleAmmoRack = true;
	inline bool bShowModuleEngine = true;
	inline bool bShowModuleTransmission = true;

	inline float xray_small_module_distance = 300.0f;
	inline float xray_line_thickness = 1.5f;
	inline int   xray_render_mode = 2;
	inline bool  xray_wireframe_overlay = false;
	inline bool  xray_alpha_falloff = true;
	inline float xray_module_alpha = 1.0f;
	inline int   xray_axis_fix_mode = 3;   // 0=(-y,x,z)  1=(x,z,-y)  2=(y,-x,z)  3=sin conversion (default: TMatrix Dagor ya convierte ejes)

	// Colores de módulos X-Ray
	inline ImVec4 color_mod_gun_barrel = ImVec4(0.90f, 0.40f, 0.00f, 0.25f);
	inline ImVec4 color_mod_cannon_breech = ImVec4(0.60f, 0.00f, 0.60f, 0.25f);
	inline ImVec4 color_mod_crew = ImVec4(0.00f, 0.80f, 0.80f, 0.25f);
	inline ImVec4 color_mod_fuel_tank = ImVec4(0.80f, 0.80f, 0.00f, 0.25f);
	inline ImVec4 color_mod_ammo_rack = ImVec4(0.80f, 0.00f, 0.00f, 0.25f);
	inline ImVec4 color_mod_engine = ImVec4(1.0f, 0.55f, 0.1f, 0.7f);
	inline ImVec4 color_mod_transmission = ImVec4(0.55f, 0.65f, 1.0f, 0.7f);

	// Tamaños de fuente
	inline float esp_font_size_name = 16.0f;
	inline float esp_font_size_distance = 16.0f;
	inline float esp_font_size_reload = 16.0f;
	inline float esp_font_size_module = 16.0f;

	// Keybind para X-Ray
	inline int   xray_toggle_key = 0x58;   // 'X' por defecto

	// Keybinds
	inline int key_esp_enable = 0;
	inline int key_aimbot_enable = 0;
	inline int key_air_lead_enable = 0;
	inline int key_3d_box = 0;
	inline int key_2d_box = 0;
	inline int key_vehicle_name = 0;
	inline int key_distance = 0;
	inline int key_show_crew = 0;
	inline int key_show_barrel = 0;
	inline int key_show_ammorack = 0;
	inline int key_show_fueltank = 0;

	// Air Lead Indicator
	inline bool air_lead_enabled = true;
	inline bool air_lead_use_simulation = true;
	inline ImVec4 air_lead_sim_color = ImVec4(1.0f, 0.5f, 0.0f, 0.8f);
	inline float air_lead_marker_size = 5.0f;
	inline float air_lead_line_thickness = 1.0f;

	// Air Lead Avanzado
	inline bool  air_lead_use_weapon_position = false;
	inline bool  air_lead_smooth_adaptive = true;
	inline float air_lead_smooth_min_alpha = 0.30f;
	inline float air_lead_smooth_max_alpha = 0.85f;
	inline bool  air_lead_use_turn_prediction = false;

	// Air Lead Targets
	inline bool air_lead_target_helicopter = true;
	inline bool air_lead_target_recon_drone = true;
	inline bool air_lead_target_fighter = true;
	inline bool air_lead_target_bomber = true;
	inline bool air_lead_target_attacker = true;

	inline bool  air_lead_only_target = true;
	inline float air_lead_extra_time = 0.0f;
	inline int   air_lead_weapon_selection = -1;
	inline bool  air_lead_use_heaviest_weapon = false;

	// Estilo del menú
	inline int   menu_style = 0;

	inline c_game* cGame = new c_game;
	inline c_entity* cLocalPlayer = new c_entity;

	inline auto init() -> bool
	{
		if (!cGame->init())
			return false;

		if (!cLocalPlayer->init())
			return false;

		if (!cGame->ballistics->init(cGame->get_base()))
			return false;

		if (!cGame->camera->init(cGame->get_base()))
			return false;

		return true;
	}
}