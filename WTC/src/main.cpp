#include "includes.hpp"
#include "core/update.hpp"
#include "core/core.hpp"
#include "core/config.hpp"
#include <commdlg.h>

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HMONITOR                 g_CurrentMonitor = nullptr;
static bool                     g_F1DownLastFrame = false;
static bool                     g_F2DownLastFrame = false;

// Forward declarations
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void UpdateCurrentMonitor(HWND hWnd);
void ApplyFullscreenToMonitor(HWND hWnd, int index);
int clamp_monitor_index(int value, int count);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void ApplyMenuStyle(int style)
{
    ImGuiStyle& st = ImGui::GetStyle();
    ImVec4* colors = st.Colors;
    ImGui::StyleColorsDark();

    switch (style)
    {
    case 0: // Dark
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.22f, 0.60f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.11f, 0.95f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.38f, 0.43f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.20f, 0.20f, 0.80f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
        st.WindowRounding = 6.0f;
        break;
    case 1: // Classic
        ImGui::StyleColorsClassic();
        st.WindowRounding = 4.0f;
        break;
    case 2: // Crimson
        ImGui::StyleColorsDark();
        colors[ImGuiCol_Button] = ImVec4(0.40f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.60f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.15f, 0.15f, 0.90f);
        st.WindowRounding = 8.0f;
        break;
    case 3: // Midnight
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.15f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.18f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.30f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.15f, 0.45f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.20f, 0.80f, 0.80f);
        st.WindowRounding = 10.0f;
        break;
    case 4: // Minimal Light
        ImGui::StyleColorsLight();
        colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.97f, 0.97f, 0.99f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.88f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.75f, 0.78f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.60f, 0.20f, 1.00f);
        st.WindowRounding = 5.0f;
        st.FrameRounding = 3.0f;
        break;
    case 5: // Forest
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.12f, 0.08f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.15f, 0.10f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.30f, 0.18f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.40f, 0.25f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.90f, 0.30f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 0.30f, 0.80f);
        st.WindowRounding = 7.0f;
        break;
    case 6: // Ocean
        ImGui::StyleColorsDark();
        colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.10f, 0.16f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.03f, 0.12f, 0.19f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.10f, 0.25f, 0.35f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.35f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.55f, 0.70f, 0.80f);
        st.WindowRounding = 9.0f;
        break;
    }

    st.FrameRounding = 4.0f;
    st.GrabRounding = 4.0f;
    st.ScrollbarRounding = 6.0f;
    st.TabRounding = 6.0f;
    st.WindowPadding = ImVec2(10.0f, 10.0f);
    st.FramePadding = ImVec2(6.0f, 4.0f);
    st.ItemSpacing = ImVec2(8.0f, 6.0f);
    st.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    st.Alpha = 1.0f;
    st.AntiAliasedLines = true;
    st.AntiAliasedFill = true;
}

namespace GUI
{
    struct menu_preset_t
    {
        // ESP
        bool  esp_enabled = true;
        bool  radar_enabled = true;
        bool  aimbot_enabled = false;
        float esp_render_distance = 2000.0f;
        float esp_box_distance = 2000.0f;
        bool  esp_show_boxes = true;
        bool  esp_use_3d_boxes = true;
        bool  esp_show_vehicle_names = true;
        bool  esp_show_distance = true;
        bool  esp_show_reload = false;
        bool  esp_show_reload_seconds = false;
        bool  esp_show_fps = true;
        bool  esp_show_bomb_impact = true;

        // Vehicle filters
        bool show_helicopter = true;
        bool show_bomber = true;
        bool show_fighter = true;
        bool show_attacker = true;
        bool show_light_tank = true;
        bool show_heavy_tank = true;
        bool show_medium_tank = true;
        bool show_spaa = true;
        bool show_gun_boat = true;
        bool show_destroyer = true;
        bool show_cruiser = true;
        bool show_torpedo_boat = true;
        bool show_recon_drone = true;

        int death_state_threshold = 1;

        // Vehicle colors
        ImVec4 color_helicopter = ImVec4(247.f / 255.f, 163.f / 255.f, 159.f / 255.f, 1.f);
        ImVec4 color_bomber = ImVec4(155.f / 255.f, 165.f / 255.f, 237.f / 255.f, 1.f);
        ImVec4 color_fighter = ImVec4(241.f / 255.f, 158.f / 255.f, 112.f / 255.f, 1.f);
        ImVec4 color_attacker = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_light_tank = ImVec4(246.f / 255.f, 234.f / 255.f, 237.f / 255.f, 1.f);
        ImVec4 color_heavy_tank = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
        ImVec4 color_medium_tank = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_spaa = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
        ImVec4 color_gun_boat = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_destroyer = ImVec4(246.f / 255.f, 95.f / 255.f, 99.f / 255.f, 1.f);
        ImVec4 color_cruiser = ImVec4(196.f / 255.f, 249.f / 255.f, 180.f / 255.f, 1.f);
        ImVec4 color_torpedo_boat = ImVec4(184.f / 255.f, 146.f / 255.f, 238.f / 255.f, 1.f);
        ImVec4 color_recon_drone = ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
        ImVec4 radar_enemy_color = ImVec4(1.0f, 0.255f, 0.255f, 0.96f);

        // X-Ray
        bool  bShowDamageModel = false;
        bool  bShowModuleNames = false;
        int   xray_render_mode = 1;
        bool  xray_wireframe_overlay = true;
        bool  bShowModuleGunBarrel = true;
        bool  bShowModuleCannonBreech = true;
        bool  bShowModuleCrew = true;
        bool  bShowModuleFuelTank = true;
        bool  bShowModuleAmmoRack = true;
        bool  bShowModuleEngine = true;
        bool  bShowModuleTransmission = true;
        float xray_small_module_distance = 150.0f;
        float xray_line_thickness = 0.8f;
        float xray_module_alpha = 0.9f;
        float xray_render_distance = 1500.0f;
        float xray_names_distance = 800.0f;
        ImVec4 color_mod_gun_barrel = ImVec4(1.0f, 0.6f, 0.2f, 0.7f);
        ImVec4 color_mod_cannon_breech = ImVec4(0.8f, 0.2f, 0.2f, 0.7f);
        ImVec4 color_mod_crew = ImVec4(0.2f, 0.8f, 0.2f, 0.7f);
        ImVec4 color_mod_fuel_tank = ImVec4(0.2f, 0.5f, 1.0f, 0.7f);
        ImVec4 color_mod_ammo_rack = ImVec4(1.0f, 0.8f, 0.0f, 0.7f);
        ImVec4 color_mod_engine = ImVec4(1.0f, 0.55f, 0.1f, 0.7f);
        ImVec4 color_mod_transmission = ImVec4(0.55f, 0.65f, 1.0f, 0.7f);
        bool  xray_alpha_falloff = true;

        // Air Lead
        bool  air_lead_enabled = false;
        bool  air_lead_use_simulation = true;
        ImVec4 air_lead_sim_color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
        float air_lead_marker_size = 5.0f;
        float air_lead_line_thickness = 1.2f;
        bool  air_lead_use_weapon_position = false;
        bool  air_lead_smooth_adaptive = true;
        float air_lead_smooth_min_alpha = 0.30f;
        float air_lead_smooth_max_alpha = 0.85f;
        bool  air_lead_use_turn_prediction = false;
        bool  air_lead_target_helicopter = true;
        bool  air_lead_target_recon_drone = true;
        bool  air_lead_target_fighter = true;
        bool  air_lead_target_bomber = true;
        bool  air_lead_target_attacker = true;
        bool  air_lead_only_target = true;
        float air_lead_extra_time = 0.0f;
        int   air_lead_weapon_selection = -1;
        bool  air_lead_use_heaviest_weapon = false;

        // Settings
        int   menu_style = 0;
        bool  esp_vsync_enabled = false;
        int   esp_vsync_fps = 144;
        bool  auto_fullscreen_on_start = false;

        // Font sizes
        float esp_font_size_name = 14.0f;
        float esp_font_size_distance = 13.0f;
        float esp_font_size_reload = 12.0f;
        float esp_font_size_module = 11.0f;

        // Keybinds
        int   xray_toggle_key = 0x58;
        int   key_esp_enable = 0;
        int   key_aimbot_enable = 0;
        int   key_air_lead_enable = 0;
        int   key_3d_box = 0;
        int   key_2d_box = 0;
        int   key_vehicle_name = 0;
        int   key_distance = 0;
        int   key_show_crew = 0;
        int   key_show_barrel = 0;
        int   key_show_ammorack = 0;
        int   key_show_fueltank = 0;

        // Monitor
        int   monitor_index = 0;
        std::string name = "Preset";
    };

    bool menu_open = true;
    int active_preset = 0;
    std::array<menu_preset_t, 5> presets = {};

    struct bool_binding_t { bool menu_preset_t::* preset; bool* runtime; };
    struct float_binding_t { float menu_preset_t::* preset; float* runtime; };
    struct color_binding_t { ImVec4 menu_preset_t::* preset; ImVec4* runtime; };
    struct int_binding_t { int menu_preset_t::* preset; int* runtime; };

    // ---- Tamaños EXACTOS (bug #1 corregido) ----
    inline const std::array<bool_binding_t, 49> bool_bindings = { {
        { &menu_preset_t::esp_enabled, &sdk::esp_enabled },
        { &menu_preset_t::radar_enabled, &sdk::radar_enabled },
        { &menu_preset_t::aimbot_enabled, &sdk::aimbot_enabled },
        { &menu_preset_t::esp_show_boxes, &sdk::esp_show_boxes },
        { &menu_preset_t::esp_use_3d_boxes, &sdk::esp_use_3d_boxes },
        { &menu_preset_t::esp_show_vehicle_names, &sdk::esp_show_vehicle_names },
        { &menu_preset_t::esp_show_distance, &sdk::esp_show_distance },
        { &menu_preset_t::esp_show_reload, &sdk::esp_show_reload },
        { &menu_preset_t::esp_show_reload_seconds, &sdk::esp_show_reload_seconds },
        { &menu_preset_t::esp_show_fps, &sdk::esp_show_fps },
        { &menu_preset_t::esp_show_bomb_impact, &sdk::esp_show_bomb_impact },
        { &menu_preset_t::show_helicopter, &sdk::show_helicopter },
        { &menu_preset_t::show_bomber, &sdk::show_bomber },
        { &menu_preset_t::show_fighter, &sdk::show_fighter },
        { &menu_preset_t::show_attacker, &sdk::show_attacker },
        { &menu_preset_t::show_light_tank, &sdk::show_light_tank },
        { &menu_preset_t::show_heavy_tank, &sdk::show_heavy_tank },
        { &menu_preset_t::show_medium_tank, &sdk::show_medium_tank },
        { &menu_preset_t::show_spaa, &sdk::show_spaa },
        { &menu_preset_t::show_gun_boat, &sdk::show_gun_boat },
        { &menu_preset_t::show_destroyer, &sdk::show_destroyer },
        { &menu_preset_t::show_cruiser, &sdk::show_cruiser },
        { &menu_preset_t::show_torpedo_boat, &sdk::show_torpedo_boat },
        { &menu_preset_t::show_recon_drone, &sdk::show_recon_drone },
        { &menu_preset_t::bShowDamageModel, &sdk::bShowDamageModel },
        { &menu_preset_t::bShowModuleNames, &sdk::bShowModuleNames },
        { &menu_preset_t::xray_wireframe_overlay, &sdk::xray_wireframe_overlay },
        { &menu_preset_t::bShowModuleGunBarrel, &sdk::bShowModuleGunBarrel },
        { &menu_preset_t::bShowModuleCannonBreech, &sdk::bShowModuleCannonBreech },
        { &menu_preset_t::bShowModuleCrew, &sdk::bShowModuleCrew },
        { &menu_preset_t::bShowModuleFuelTank, &sdk::bShowModuleFuelTank },
        { &menu_preset_t::bShowModuleAmmoRack, &sdk::bShowModuleAmmoRack },
        { &menu_preset_t::bShowModuleEngine, &sdk::bShowModuleEngine },
        { &menu_preset_t::bShowModuleTransmission, &sdk::bShowModuleTransmission },
        { &menu_preset_t::xray_alpha_falloff, &sdk::xray_alpha_falloff },
        { &menu_preset_t::air_lead_enabled, &sdk::air_lead_enabled },
        { &menu_preset_t::air_lead_use_simulation, &sdk::air_lead_use_simulation },
        { &menu_preset_t::air_lead_use_weapon_position, &sdk::air_lead_use_weapon_position },
        { &menu_preset_t::air_lead_smooth_adaptive, &sdk::air_lead_smooth_adaptive },
        { &menu_preset_t::air_lead_use_turn_prediction, &sdk::air_lead_use_turn_prediction },
        { &menu_preset_t::air_lead_target_helicopter, &sdk::air_lead_target_helicopter },
        { &menu_preset_t::air_lead_target_recon_drone, &sdk::air_lead_target_recon_drone },
        { &menu_preset_t::air_lead_target_fighter, &sdk::air_lead_target_fighter },
        { &menu_preset_t::air_lead_target_bomber, &sdk::air_lead_target_bomber },
        { &menu_preset_t::air_lead_target_attacker, &sdk::air_lead_target_attacker },
        { &menu_preset_t::air_lead_only_target, &sdk::air_lead_only_target },
        { &menu_preset_t::air_lead_use_heaviest_weapon, &sdk::air_lead_use_heaviest_weapon },
        { &menu_preset_t::esp_vsync_enabled, &sdk::esp_vsync_enabled },
        { &menu_preset_t::auto_fullscreen_on_start, &sdk::auto_fullscreen_on_start }
    } };

    inline const std::array<float_binding_t, 16> float_bindings = { {
        { &menu_preset_t::esp_render_distance, &sdk::esp_render_distance },
        { &menu_preset_t::esp_box_distance, &sdk::esp_box_distance },
        { &menu_preset_t::xray_small_module_distance, &sdk::xray_small_module_distance },
        { &menu_preset_t::xray_line_thickness, &sdk::xray_line_thickness },
        { &menu_preset_t::xray_module_alpha, &sdk::xray_module_alpha },
        { &menu_preset_t::xray_render_distance, &sdk::xray_render_distance },
        { &menu_preset_t::xray_names_distance, &sdk::xray_names_distance },
        { &menu_preset_t::air_lead_marker_size, &sdk::air_lead_marker_size },
        { &menu_preset_t::air_lead_line_thickness, &sdk::air_lead_line_thickness },
        { &menu_preset_t::air_lead_smooth_min_alpha, &sdk::air_lead_smooth_min_alpha },
        { &menu_preset_t::air_lead_smooth_max_alpha, &sdk::air_lead_smooth_max_alpha },
        { &menu_preset_t::air_lead_extra_time, &sdk::air_lead_extra_time },
        { &menu_preset_t::esp_font_size_name, &sdk::esp_font_size_name },
        { &menu_preset_t::esp_font_size_distance, &sdk::esp_font_size_distance },
        { &menu_preset_t::esp_font_size_reload, &sdk::esp_font_size_reload },
        { &menu_preset_t::esp_font_size_module, &sdk::esp_font_size_module }
    } };

    inline const std::array<color_binding_t, 22> color_bindings = { {
        { &menu_preset_t::color_helicopter, &sdk::color_helicopter },
        { &menu_preset_t::color_bomber, &sdk::color_bomber },
        { &menu_preset_t::color_fighter, &sdk::color_fighter },
        { &menu_preset_t::color_attacker, &sdk::color_attacker },
        { &menu_preset_t::color_light_tank, &sdk::color_light_tank },
        { &menu_preset_t::color_heavy_tank, &sdk::color_heavy_tank },
        { &menu_preset_t::color_medium_tank, &sdk::color_medium_tank },
        { &menu_preset_t::color_spaa, &sdk::color_spaa },
        { &menu_preset_t::color_gun_boat, &sdk::color_gun_boat },
        { &menu_preset_t::color_destroyer, &sdk::color_destroyer },
        { &menu_preset_t::color_cruiser, &sdk::color_cruiser },
        { &menu_preset_t::color_torpedo_boat, &sdk::color_torpedo_boat },
        { &menu_preset_t::color_recon_drone, &sdk::color_recon_drone },
        { &menu_preset_t::radar_enemy_color, &sdk::radar_enemy_color },
        { &menu_preset_t::color_mod_gun_barrel, &sdk::color_mod_gun_barrel },
        { &menu_preset_t::color_mod_cannon_breech, &sdk::color_mod_cannon_breech },
        { &menu_preset_t::color_mod_crew, &sdk::color_mod_crew },
        { &menu_preset_t::color_mod_fuel_tank, &sdk::color_mod_fuel_tank },
        { &menu_preset_t::color_mod_ammo_rack, &sdk::color_mod_ammo_rack },
        { &menu_preset_t::color_mod_engine, &sdk::color_mod_engine },
        { &menu_preset_t::color_mod_transmission, &sdk::color_mod_transmission },
        { &menu_preset_t::air_lead_sim_color, &sdk::air_lead_sim_color }
    } };

    inline const std::array<int_binding_t, 17> int_bindings = { {
        { &menu_preset_t::xray_render_mode, &sdk::xray_render_mode },
        { &menu_preset_t::menu_style, &sdk::menu_style },
        { &menu_preset_t::esp_vsync_fps, &sdk::esp_vsync_fps },
        { &menu_preset_t::death_state_threshold, &sdk::death_state_threshold },
        { &menu_preset_t::air_lead_weapon_selection, &sdk::air_lead_weapon_selection },
        { &menu_preset_t::xray_toggle_key, &sdk::xray_toggle_key },
        { &menu_preset_t::key_esp_enable, &sdk::key_esp_enable },
        { &menu_preset_t::key_aimbot_enable, &sdk::key_aimbot_enable },
        { &menu_preset_t::key_air_lead_enable, &sdk::key_air_lead_enable },
        { &menu_preset_t::key_3d_box, &sdk::key_3d_box },
        { &menu_preset_t::key_2d_box, &sdk::key_2d_box },
        { &menu_preset_t::key_vehicle_name, &sdk::key_vehicle_name },
        { &menu_preset_t::key_distance, &sdk::key_distance },
        { &menu_preset_t::key_show_crew, &sdk::key_show_crew },
        { &menu_preset_t::key_show_barrel, &sdk::key_show_barrel },
        { &menu_preset_t::key_show_ammorack, &sdk::key_show_ammorack },
        { &menu_preset_t::key_show_fueltank, &sdk::key_show_fueltank }
    } };

    menu_preset_t capture_current_settings()
    {
        menu_preset_t preset = {};
        for (const auto& b : bool_bindings)   preset.*(b.preset) = *b.runtime;
        for (const auto& f : float_bindings)  preset.*(f.preset) = *f.runtime;
        for (const auto& c : color_bindings)  preset.*(c.preset) = *c.runtime;
        for (const auto& i : int_bindings)    preset.*(i.preset) = *i.runtime;
        preset.monitor_index = sdk::monitor_index;
        return preset;
    }

    void apply_preset(const menu_preset_t& preset)
    {
        for (const auto& b : bool_bindings)   *b.runtime = preset.*(b.preset);
        for (const auto& f : float_bindings)  *f.runtime = preset.*(f.preset);
        for (const auto& c : color_bindings)  *c.runtime = preset.*(c.preset);
        for (const auto& i : int_bindings)    *i.runtime = preset.*(i.preset);
        sdk::monitor_index = clamp_monitor_index(preset.monitor_index, sdk::monitor_count);
    }

    void initialize_presets()
    {
        presets[0] = capture_current_settings();
        presets[0].name = "Default";
        for (int i = 1; i < 5; ++i)
        {
            presets[i] = presets[0];
            presets[i].name = "Preset " + std::to_string(i + 1);
        }
        presets[1].aimbot_enabled = true;
        presets[1].name = "Aimbot";
        presets[2].esp_show_boxes = false;
        presets[2].esp_use_3d_boxes = false;
        presets[2].name = "Clean";
        active_preset = 0;
    }

    void sync_shared_settings_to_all_presets()
    {
        const int safe_monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        for (auto& preset : presets)
        {
            preset.monitor_index = safe_monitor_index;
            preset.auto_fullscreen_on_start = sdk::auto_fullscreen_on_start;
        }
    }

    void cycle_preset()
    {
        const std::string active_name = presets[active_preset].name;
        presets[active_preset] = capture_current_settings();
        presets[active_preset].name = active_name;
        sync_shared_settings_to_all_presets();
        active_preset = (active_preset + 1) % static_cast<int>(presets.size());
        apply_preset(presets[active_preset]);
    }

    void load_presets_from_map(const std::unordered_map<std::string, std::string>& values)
    {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i)
        {
            menu_preset_t loaded = presets[i];
            const std::string prefix = "preset" + std::to_string(i + 1) + "_";

            if (auto it = values.find(prefix + "name"); it != values.end())
                loaded.name = it->second;

            auto read_bool = [&](const char* key, bool& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = config::parse_bool(it->second, target);
                };
            auto read_float = [&](const char* key, float& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = std::clamp(config::parse_float(it->second, target), 0.0f, 100000.0f);
                };
            auto read_int = [&](const char* key, int& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) target = config::parse_int(it->second, target);
                };
            auto read_color = [&](const char* key, ImVec4& target) {
                const auto it = values.find(prefix + key);
                if (it != values.end()) config::apply_color(target, it->second);
                };

            // ESP
            read_bool("esp_enabled", loaded.esp_enabled);
            read_bool("radar_enabled", loaded.radar_enabled);
            read_bool("aimbot_enabled", loaded.aimbot_enabled);
            read_float("esp_render_distance", loaded.esp_render_distance);
            read_float("esp_box_distance", loaded.esp_box_distance);
            read_bool("esp_show_boxes", loaded.esp_show_boxes);
            read_bool("esp_use_3d_boxes", loaded.esp_use_3d_boxes);
            read_bool("esp_show_vehicle_names", loaded.esp_show_vehicle_names);
            read_bool("esp_show_distance", loaded.esp_show_distance);
            read_bool("esp_show_reload", loaded.esp_show_reload);
            read_bool("esp_show_reload_seconds", loaded.esp_show_reload_seconds);
            read_bool("esp_show_fps", loaded.esp_show_fps);
            read_bool("esp_show_bomb_impact", loaded.esp_show_bomb_impact);

            // Vehicle filters
            read_bool("show_helicopter", loaded.show_helicopter);
            read_bool("show_bomber", loaded.show_bomber);
            read_bool("show_fighter", loaded.show_fighter);
            read_bool("show_attacker", loaded.show_attacker);
            read_bool("show_light_tank", loaded.show_light_tank);
            read_bool("show_heavy_tank", loaded.show_heavy_tank);
            read_bool("show_medium_tank", loaded.show_medium_tank);
            read_bool("show_spaa", loaded.show_spaa);
            read_bool("show_gun_boat", loaded.show_gun_boat);
            read_bool("show_destroyer", loaded.show_destroyer);
            read_bool("show_cruiser", loaded.show_cruiser);
            read_bool("show_torpedo_boat", loaded.show_torpedo_boat);
            read_bool("show_recon_drone", loaded.show_recon_drone);

            read_int("death_state_threshold", loaded.death_state_threshold);

            // Vehicle colors
            read_color("color_helicopter", loaded.color_helicopter);
            read_color("color_bomber", loaded.color_bomber);
            read_color("color_fighter", loaded.color_fighter);
            read_color("color_attacker", loaded.color_attacker);
            read_color("color_light_tank", loaded.color_light_tank);
            read_color("color_heavy_tank", loaded.color_heavy_tank);
            read_color("color_medium_tank", loaded.color_medium_tank);
            read_color("color_spaa", loaded.color_spaa);
            read_color("color_gun_boat", loaded.color_gun_boat);
            read_color("color_destroyer", loaded.color_destroyer);
            read_color("color_cruiser", loaded.color_cruiser);
            read_color("color_torpedo_boat", loaded.color_torpedo_boat);
            read_color("color_recon_drone", loaded.color_recon_drone);
            read_color("radar_enemy_color", loaded.radar_enemy_color);

            // X-Ray
            read_bool("bShowDamageModel", loaded.bShowDamageModel);
            read_bool("bShowModuleNames", loaded.bShowModuleNames);
            read_int("xray_render_mode", loaded.xray_render_mode);
            read_bool("xray_wireframe_overlay", loaded.xray_wireframe_overlay);
            read_bool("bShowModuleGunBarrel", loaded.bShowModuleGunBarrel);
            read_bool("bShowModuleCannonBreech", loaded.bShowModuleCannonBreech);
            read_bool("bShowModuleCrew", loaded.bShowModuleCrew);
            read_bool("bShowModuleFuelTank", loaded.bShowModuleFuelTank);
            read_bool("bShowModuleAmmoRack", loaded.bShowModuleAmmoRack);
            read_bool("bShowModuleEngine", loaded.bShowModuleEngine);
            read_bool("bShowModuleTransmission", loaded.bShowModuleTransmission);
            read_float("xray_small_module_distance", loaded.xray_small_module_distance);
            read_float("xray_line_thickness", loaded.xray_line_thickness);
            read_float("xray_module_alpha", loaded.xray_module_alpha);
            read_float("xray_render_distance", loaded.xray_render_distance);
            read_float("xray_names_distance", loaded.xray_names_distance);
            read_color("color_mod_gun_barrel", loaded.color_mod_gun_barrel);
            read_color("color_mod_cannon_breech", loaded.color_mod_cannon_breech);
            read_color("color_mod_crew", loaded.color_mod_crew);
            read_color("color_mod_fuel_tank", loaded.color_mod_fuel_tank);
            read_color("color_mod_ammo_rack", loaded.color_mod_ammo_rack);
            read_color("color_mod_engine", loaded.color_mod_engine);
            read_color("color_mod_transmission", loaded.color_mod_transmission);
            read_bool("xray_alpha_falloff", loaded.xray_alpha_falloff);

            // Air Lead
            read_bool("air_lead_enabled", loaded.air_lead_enabled);
            read_bool("air_lead_use_simulation", loaded.air_lead_use_simulation);
            read_color("air_lead_sim_color", loaded.air_lead_sim_color);
            read_float("air_lead_marker_size", loaded.air_lead_marker_size);
            read_float("air_lead_line_thickness", loaded.air_lead_line_thickness);
            read_bool("air_lead_use_weapon_position", loaded.air_lead_use_weapon_position);
            read_bool("air_lead_smooth_adaptive", loaded.air_lead_smooth_adaptive);
            read_float("air_lead_smooth_min_alpha", loaded.air_lead_smooth_min_alpha);
            read_float("air_lead_smooth_max_alpha", loaded.air_lead_smooth_max_alpha);
            read_bool("air_lead_use_turn_prediction", loaded.air_lead_use_turn_prediction);
            read_bool("air_lead_target_helicopter", loaded.air_lead_target_helicopter);
            read_bool("air_lead_target_recon_drone", loaded.air_lead_target_recon_drone);
            read_bool("air_lead_target_fighter", loaded.air_lead_target_fighter);
            read_bool("air_lead_target_bomber", loaded.air_lead_target_bomber);
            read_bool("air_lead_target_attacker", loaded.air_lead_target_attacker);
            read_bool("air_lead_only_target", loaded.air_lead_only_target);
            read_float("air_lead_extra_time", loaded.air_lead_extra_time);
            read_int("air_lead_weapon_selection", loaded.air_lead_weapon_selection);
            read_bool("air_lead_use_heaviest_weapon", loaded.air_lead_use_heaviest_weapon);

            // Settings
            read_int("menu_style", loaded.menu_style);
            read_bool("esp_vsync_enabled", loaded.esp_vsync_enabled);
            read_int("esp_vsync_fps", loaded.esp_vsync_fps);

            // Font sizes
            read_float("esp_font_size_name", loaded.esp_font_size_name);
            read_float("esp_font_size_distance", loaded.esp_font_size_distance);
            read_float("esp_font_size_reload", loaded.esp_font_size_reload);
            read_float("esp_font_size_module", loaded.esp_font_size_module);

            // Keybinds
            read_int("xray_toggle_key", loaded.xray_toggle_key);
            read_int("key_esp_enable", loaded.key_esp_enable);
            read_int("key_aimbot_enable", loaded.key_aimbot_enable);
            read_int("key_air_lead_enable", loaded.key_air_lead_enable);
            read_int("key_3d_box", loaded.key_3d_box);
            read_int("key_2d_box", loaded.key_2d_box);
            read_int("key_vehicle_name", loaded.key_vehicle_name);
            read_int("key_distance", loaded.key_distance);
            read_int("key_show_crew", loaded.key_show_crew);
            read_int("key_show_barrel", loaded.key_show_barrel);
            read_int("key_show_ammorack", loaded.key_show_ammorack);
            read_int("key_show_fueltank", loaded.key_show_fueltank);

            // Monitor
            if (const auto it = values.find(prefix + "monitor_index"); it != values.end())
                loaded.monitor_index = config::parse_int(it->second, loaded.monitor_index);

            loaded.esp_render_distance = std::clamp(loaded.esp_render_distance, 1000.0f, 55000.0f);
            loaded.esp_box_distance = std::clamp(loaded.esp_box_distance, 500.0f, 3500.0f);
            loaded.xray_render_mode = std::clamp(loaded.xray_render_mode, 0, 2);
            loaded.menu_style = std::clamp(loaded.menu_style, 0, 6);
            loaded.esp_vsync_fps = std::clamp(loaded.esp_vsync_fps, 30, 500);
            loaded.death_state_threshold = std::clamp(loaded.death_state_threshold, 1, 2);
            loaded.air_lead_weapon_selection = std::clamp(loaded.air_lead_weapon_selection, -1, 20);
            presets[i] = loaded;
        }

        if (const auto it = values.find("active_preset"); it != values.end())
            active_preset = std::clamp(config::parse_int(it->second, active_preset), 0, static_cast<int>(presets.size()) - 1);

        apply_preset(presets[active_preset]);
    }

    void sync_active_preset_from_runtime()
    {
        active_preset = std::clamp(active_preset, 0, static_cast<int>(presets.size()) - 1);
        const std::string name = presets[active_preset].name;
        presets[active_preset] = capture_current_settings();
        presets[active_preset].name = name;
        sync_shared_settings_to_all_presets();
    }

    void append_presets_to_stream(std::ostream& output)
    {
        // Serialize the live active profile without modifying the UI/profile state.
        auto saved_presets = presets;
        const int selected = std::clamp(active_preset, 0, static_cast<int>(saved_presets.size()) - 1);
        const std::string active_name = saved_presets[selected].name;
        saved_presets[selected] = capture_current_settings();
        saved_presets[selected].name = active_name;
        const int safe_monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        for (auto& preset : saved_presets)
        {
            preset.monitor_index = safe_monitor_index;
            preset.auto_fullscreen_on_start = sdk::auto_fullscreen_on_start;
        }
        output << "active_preset=" << selected << '\n';

        for (int i = 0; i < static_cast<int>(saved_presets.size()); ++i)
        {
            const menu_preset_t& p = saved_presets[i];
            const std::string prefix = "preset" + std::to_string(i + 1) + "_";

            output << prefix << "name=" << p.name << '\n';

            output << prefix << "esp_enabled=" << (p.esp_enabled ? 1 : 0) << '\n';
            output << prefix << "radar_enabled=" << (p.radar_enabled ? 1 : 0) << '\n';
            output << prefix << "aimbot_enabled=" << (p.aimbot_enabled ? 1 : 0) << '\n';
            output << prefix << "esp_render_distance=" << p.esp_render_distance << '\n';
            output << prefix << "esp_box_distance=" << p.esp_box_distance << '\n';
            output << prefix << "esp_show_boxes=" << (p.esp_show_boxes ? 1 : 0) << '\n';
            output << prefix << "esp_use_3d_boxes=" << (p.esp_use_3d_boxes ? 1 : 0) << '\n';
            output << prefix << "esp_show_vehicle_names=" << (p.esp_show_vehicle_names ? 1 : 0) << '\n';
            output << prefix << "esp_show_distance=" << (p.esp_show_distance ? 1 : 0) << '\n';
            output << prefix << "esp_show_reload=" << (p.esp_show_reload ? 1 : 0) << '\n';
            output << prefix << "esp_show_reload_seconds=" << (p.esp_show_reload_seconds ? 1 : 0) << '\n';
            output << prefix << "esp_show_fps=" << (p.esp_show_fps ? 1 : 0) << '\n';
            output << prefix << "esp_show_bomb_impact=" << (p.esp_show_bomb_impact ? 1 : 0) << '\n';

            output << prefix << "show_helicopter=" << (p.show_helicopter ? 1 : 0) << '\n';
            output << prefix << "show_bomber=" << (p.show_bomber ? 1 : 0) << '\n';
            output << prefix << "show_fighter=" << (p.show_fighter ? 1 : 0) << '\n';
            output << prefix << "show_attacker=" << (p.show_attacker ? 1 : 0) << '\n';
            output << prefix << "show_light_tank=" << (p.show_light_tank ? 1 : 0) << '\n';
            output << prefix << "show_heavy_tank=" << (p.show_heavy_tank ? 1 : 0) << '\n';
            output << prefix << "show_medium_tank=" << (p.show_medium_tank ? 1 : 0) << '\n';
            output << prefix << "show_spaa=" << (p.show_spaa ? 1 : 0) << '\n';
            output << prefix << "show_gun_boat=" << (p.show_gun_boat ? 1 : 0) << '\n';
            output << prefix << "show_destroyer=" << (p.show_destroyer ? 1 : 0) << '\n';
            output << prefix << "show_cruiser=" << (p.show_cruiser ? 1 : 0) << '\n';
            output << prefix << "show_torpedo_boat=" << (p.show_torpedo_boat ? 1 : 0) << '\n';
            output << prefix << "show_recon_drone=" << (p.show_recon_drone ? 1 : 0) << '\n';

            output << prefix << "death_state_threshold=" << p.death_state_threshold << '\n';

            auto write_color = [&](const char* key, const ImVec4& col) {
                output << prefix << key << "=" << config::color_to_string(col) << '\n';
                };
            write_color("color_helicopter", p.color_helicopter);
            write_color("color_bomber", p.color_bomber);
            write_color("color_fighter", p.color_fighter);
            write_color("color_attacker", p.color_attacker);
            write_color("color_light_tank", p.color_light_tank);
            write_color("color_heavy_tank", p.color_heavy_tank);
            write_color("color_medium_tank", p.color_medium_tank);
            write_color("color_spaa", p.color_spaa);
            write_color("color_gun_boat", p.color_gun_boat);
            write_color("color_destroyer", p.color_destroyer);
            write_color("color_cruiser", p.color_cruiser);
            write_color("color_torpedo_boat", p.color_torpedo_boat);
            write_color("color_recon_drone", p.color_recon_drone);
            write_color("radar_enemy_color", p.radar_enemy_color);

            output << prefix << "bShowDamageModel=" << (p.bShowDamageModel ? 1 : 0) << '\n';
            output << prefix << "bShowModuleNames=" << (p.bShowModuleNames ? 1 : 0) << '\n';
            output << prefix << "xray_render_mode=" << p.xray_render_mode << '\n';
            output << prefix << "xray_wireframe_overlay=" << (p.xray_wireframe_overlay ? 1 : 0) << '\n';
            output << prefix << "bShowModuleGunBarrel=" << (p.bShowModuleGunBarrel ? 1 : 0) << '\n';
            output << prefix << "bShowModuleCannonBreech=" << (p.bShowModuleCannonBreech ? 1 : 0) << '\n';
            output << prefix << "bShowModuleCrew=" << (p.bShowModuleCrew ? 1 : 0) << '\n';
            output << prefix << "bShowModuleFuelTank=" << (p.bShowModuleFuelTank ? 1 : 0) << '\n';
            output << prefix << "bShowModuleAmmoRack=" << (p.bShowModuleAmmoRack ? 1 : 0) << '\n';
            output << prefix << "bShowModuleEngine=" << (p.bShowModuleEngine ? 1 : 0) << '\n';
            output << prefix << "bShowModuleTransmission=" << (p.bShowModuleTransmission ? 1 : 0) << '\n';
            output << prefix << "xray_small_module_distance=" << p.xray_small_module_distance << '\n';
            output << prefix << "xray_line_thickness=" << p.xray_line_thickness << '\n';
            output << prefix << "xray_module_alpha=" << p.xray_module_alpha << '\n';
            output << prefix << "xray_render_distance=" << p.xray_render_distance << '\n';
            output << prefix << "xray_names_distance=" << p.xray_names_distance << '\n';
            write_color("color_mod_gun_barrel", p.color_mod_gun_barrel);
            write_color("color_mod_cannon_breech", p.color_mod_cannon_breech);
            write_color("color_mod_crew", p.color_mod_crew);
            write_color("color_mod_fuel_tank", p.color_mod_fuel_tank);
            write_color("color_mod_ammo_rack", p.color_mod_ammo_rack);
            write_color("color_mod_engine", p.color_mod_engine);
            write_color("color_mod_transmission", p.color_mod_transmission);
            output << prefix << "xray_alpha_falloff=" << (p.xray_alpha_falloff ? 1 : 0) << '\n';

            output << prefix << "air_lead_enabled=" << (p.air_lead_enabled ? 1 : 0) << '\n';
            output << prefix << "air_lead_use_simulation=" << (p.air_lead_use_simulation ? 1 : 0) << '\n';
            write_color("air_lead_sim_color", p.air_lead_sim_color);
            output << prefix << "air_lead_marker_size=" << p.air_lead_marker_size << '\n';
            output << prefix << "air_lead_line_thickness=" << p.air_lead_line_thickness << '\n';
            output << prefix << "air_lead_use_weapon_position=" << (p.air_lead_use_weapon_position ? 1 : 0) << '\n';
            output << prefix << "air_lead_smooth_adaptive=" << (p.air_lead_smooth_adaptive ? 1 : 0) << '\n';
            output << prefix << "air_lead_smooth_min_alpha=" << p.air_lead_smooth_min_alpha << '\n';
            output << prefix << "air_lead_smooth_max_alpha=" << p.air_lead_smooth_max_alpha << '\n';
            output << prefix << "air_lead_use_turn_prediction=" << (p.air_lead_use_turn_prediction ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_helicopter=" << (p.air_lead_target_helicopter ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_recon_drone=" << (p.air_lead_target_recon_drone ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_fighter=" << (p.air_lead_target_fighter ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_bomber=" << (p.air_lead_target_bomber ? 1 : 0) << '\n';
            output << prefix << "air_lead_target_attacker=" << (p.air_lead_target_attacker ? 1 : 0) << '\n';
            output << prefix << "air_lead_only_target=" << (p.air_lead_only_target ? 1 : 0) << '\n';
            output << prefix << "air_lead_extra_time=" << p.air_lead_extra_time << '\n';
            output << prefix << "air_lead_weapon_selection=" << p.air_lead_weapon_selection << '\n';
            output << prefix << "air_lead_use_heaviest_weapon=" << (p.air_lead_use_heaviest_weapon ? 1 : 0) << '\n';

            output << prefix << "menu_style=" << p.menu_style << '\n';
            output << prefix << "esp_vsync_enabled=" << (p.esp_vsync_enabled ? 1 : 0) << '\n';
            output << prefix << "esp_vsync_fps=" << p.esp_vsync_fps << '\n';

            output << prefix << "esp_font_size_name=" << p.esp_font_size_name << '\n';
            output << prefix << "esp_font_size_distance=" << p.esp_font_size_distance << '\n';
            output << prefix << "esp_font_size_reload=" << p.esp_font_size_reload << '\n';
            output << prefix << "esp_font_size_module=" << p.esp_font_size_module << '\n';

            output << prefix << "xray_toggle_key=" << p.xray_toggle_key << '\n';
            output << prefix << "key_esp_enable=" << p.key_esp_enable << '\n';
            output << prefix << "key_aimbot_enable=" << p.key_aimbot_enable << '\n';
            output << prefix << "key_air_lead_enable=" << p.key_air_lead_enable << '\n';
            output << prefix << "key_3d_box=" << p.key_3d_box << '\n';
            output << prefix << "key_2d_box=" << p.key_2d_box << '\n';
            output << prefix << "key_vehicle_name=" << p.key_vehicle_name << '\n';
            output << prefix << "key_distance=" << p.key_distance << '\n';
            output << prefix << "key_show_crew=" << p.key_show_crew << '\n';
            output << prefix << "key_show_barrel=" << p.key_show_barrel << '\n';
            output << prefix << "key_show_ammorack=" << p.key_show_ammorack << '\n';
            output << prefix << "key_show_fueltank=" << p.key_show_fueltank << '\n';

            output << prefix << "monitor_index=" << p.monitor_index << '\n';
        }
    }

    struct settings_snapshot_t
    {
        menu_preset_t current;
        std::array<menu_preset_t, 5> saved_presets;
        int selected = 0;
    };

    bool same_preset(const menu_preset_t& a, const menu_preset_t& b)
    {
        for (const auto& binding : bool_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : float_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : int_bindings)
            if (a.*(binding.preset) != b.*(binding.preset)) return false;
        for (const auto& binding : color_bindings)
        {
            const ImVec4& x = a.*(binding.preset);
            const ImVec4& y = b.*(binding.preset);
            if (x.x != y.x || x.y != y.y || x.z != y.z || x.w != y.w) return false;
        }
        return a.monitor_index == b.monitor_index && a.name == b.name;
    }

    settings_snapshot_t snapshot_settings()
    {
        return { capture_current_settings(), presets, active_preset };
    }

    bool same_settings(const settings_snapshot_t& a, const settings_snapshot_t& b)
    {
        if (a.selected != b.selected || !same_preset(a.current, b.current)) return false;
        for (size_t i = 0; i < a.saved_presets.size(); ++i)
            if (!same_preset(a.saved_presets[i], b.saved_presets[i])) return false;
        return true;
    }

    enum class save_status_t { saved, pending, error };
    save_status_t save_status = save_status_t::saved;
    settings_snapshot_t last_observed;
    bool autosave_ready = false;
    std::chrono::steady_clock::time_point next_save_time;

    void initialize_autosave()
    {
        last_observed = snapshot_settings();
        autosave_ready = true;
        save_status = save_status_t::saved;
    }

    bool save_now()
    {
        const bool saved = config::save();
        if (saved)
        {
            last_observed = snapshot_settings();
            save_status = save_status_t::saved;
        }
        else
        {
            save_status = save_status_t::error;
            next_save_time = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        }
        return saved;
    }

    void update_autosave()
    {
        if (!autosave_ready) return;
        const auto now = std::chrono::steady_clock::now();
        const auto current = snapshot_settings();
        if (!same_settings(current, last_observed))
        {
            last_observed = current;
            save_status = save_status_t::pending;
            next_save_time = now + std::chrono::seconds(1);
        }
        if (save_status != save_status_t::saved && now >= next_save_time)
            save_now();
    }

    bool ExportConfig(const std::string& filepath)
    {
        std::ofstream out(filepath, std::ios::trunc);
        if (!out.is_open()) return false;
        out << "// War Thunder DMA Cheat Configuration\n";
        out << "// Version 2.3.2+\n\n";
        append_presets_to_stream(out);
        out.close();
        return true;
    }

    bool ImportConfig(const std::string& filepath)
    {
        std::ifstream in(filepath);
        if (!in.is_open()) return false;
        std::unordered_map<std::string, std::string> values;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '/' || line[0] == ' ') continue;
            auto pos = line.find('=');
            if (pos != std::string::npos)
                values[line.substr(0, pos)] = line.substr(pos + 1);
        }
        load_presets_from_map(values);
        apply_preset(presets[active_preset]);
        save_now();
        return true;
    }

    void DrawKeybindRow(const char* label, int& key, bool* waiting = nullptr)
    {
        ImGui::PushID(label);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label);
        ImGui::SameLine(120);
        static std::unordered_map<int, bool> waiting_map;
        if (waiting == nullptr) waiting = &waiting_map[key];

        if (*waiting)
        {
            ImGui::Button("Press any key...");
            for (int k = 0; k < 256; k++)
            {
                if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k)))
                {
                    key = k;
                    *waiting = false;
                    break;
                }
            }
            for (int mb = 0; mb < 3; mb++)
            {
                if (ImGui::IsMouseClicked(mb))
                {
                    key = (mb == 0) ? VK_LBUTTON : (mb == 1) ? VK_RBUTTON : VK_MBUTTON;
                    *waiting = false;
                    break;
                }
            }
        }
        else
        {
            if (ImGui::Button("Set Key")) *waiting = true;
        }
        ImGui::SameLine();
        const char* name = "None";
        if (key == VK_LBUTTON) name = "LButton";
        else if (key == VK_RBUTTON) name = "RButton";
        else if (key == VK_MBUTTON) name = "MButton";
        else if (key > 0 && key < 256) {
            static char buf[2] = { 0 };
            buf[0] = (char)key;
            name = buf;
        }
        ImGui::Text("%s", name);
        ImGui::PopID();
    }

    void OnRender()
    {
        if (!menu_open) return;

        ImGui::SetNextWindowSize(ImVec2(820, 640), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("DMA Cheat - War Thunder 2.59.0.54", &menu_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Export Config..."))
                    {
                        OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                        char file[260] = "cheat_config.cfg";
                        ofn.lpstrFile = file;
                        ofn.nMaxFile = 260;
                        ofn.lpstrFilter = "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                        ofn.Flags = OFN_OVERWRITEPROMPT;
                        if (GetSaveFileNameA(&ofn)) ExportConfig(file);
                    }
                    if (ImGui::MenuItem("Import Config..."))
                    {
                        OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                        char file[260] = "";
                        ofn.lpstrFile = file;
                        ofn.nMaxFile = 260;
                        ofn.lpstrFilter = "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                        ofn.Flags = OFN_FILEMUSTEXIST;
                        if (GetOpenFileNameA(&ofn)) ImportConfig(file);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Close Menu")) menu_open = false;
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            if (ImGui::BeginTabBar("MainTabs"))
            {
                // ESP tab
                if (ImGui::BeginTabItem("ESP"))
                {
                    if (ImGui::BeginTabBar("ESP Sections"))
                    {
                        if (ImGui::BeginTabItem("General"))
                        {
                    ImGui::Checkbox("Enable ESP", &sdk::esp_enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("Show FPS", &sdk::esp_show_fps);
                    ImGui::Checkbox("Show Bomb Impact", &sdk::esp_show_bomb_impact);
                    ImGui::SliderFloat("Render Distance", &sdk::esp_render_distance, 1000.f, 55000.f, "%.0f m");
                    ImGui::SliderFloat("Box Distance", &sdk::esp_box_distance, 500.f, 3500.f, "%.0f m");

                    ImGui::SeparatorText("Boxes");
                    ImGui::Checkbox("2D Box", &sdk::esp_show_boxes);
                    ImGui::SameLine();
                    ImGui::Checkbox("3D Box", &sdk::esp_use_3d_boxes);

                    ImGui::SeparatorText("Labels");
                    ImGui::Checkbox("Vehicle Names", &sdk::esp_show_vehicle_names);
                    ImGui::SameLine();
                    ImGui::Checkbox("Distance", &sdk::esp_show_distance);
                    ImGui::Checkbox("Reload Progress", &sdk::esp_show_reload);
                    ImGui::SameLine();
                    ImGui::Checkbox("Seconds", &sdk::esp_show_reload_seconds);

                    ImGui::SeparatorText("Font Sizes");
                    ImGui::SliderFloat("Name Size", &sdk::esp_font_size_name, 8.0f, 36.0f, "%.0f");
                    ImGui::SliderFloat("Distance Size", &sdk::esp_font_size_distance, 8.0f, 36.0f, "%.0f");
                    ImGui::SliderFloat("Reload Size", &sdk::esp_font_size_reload, 8.0f, 36.0f, "%.0f");
                    ImGui::SliderFloat("Module Name Size", &sdk::esp_font_size_module, 8.0f, 36.0f, "%.0f");

                    ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Vehicles"))
                        {
                    ImGui::TextDisabled("Los colores se configuran en la pestaña Colors.");
                    const auto draw_vehicle_row = [](const char* label, bool* enabled)
                        {
                            ImGui::PushID(label);
                            ImGui::Checkbox(label, enabled);
                            ImGui::PopID();
                        };
                    draw_vehicle_row("Helicopter", &sdk::show_helicopter);
                    draw_vehicle_row("Bomber", &sdk::show_bomber);
                    draw_vehicle_row("Fighter", &sdk::show_fighter);
                    draw_vehicle_row("Attacker", &sdk::show_attacker);
                    draw_vehicle_row("Recon Drone", &sdk::show_recon_drone);
                    draw_vehicle_row("Light Tank", &sdk::show_light_tank);
                    draw_vehicle_row("Heavy Tank", &sdk::show_heavy_tank);
                    draw_vehicle_row("Medium Tank", &sdk::show_medium_tank);
                    draw_vehicle_row("SPAA", &sdk::show_spaa);
                    draw_vehicle_row("Gun Boat", &sdk::show_gun_boat);
                    draw_vehicle_row("Destroyer", &sdk::show_destroyer);
                    draw_vehicle_row("Cruiser", &sdk::show_cruiser);
                    draw_vehicle_row("Torpedo Boat", &sdk::show_torpedo_boat);

                    ImGui::SeparatorText("Filter");
                    ImGui::SliderInt("Death State Threshold", &sdk::death_state_threshold, 1, 2);
                    ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("X-Ray"))
                        {
                    ImGui::Text("Damage Model");
                    ImGui::Checkbox("Enable X-Ray", &sdk::bShowDamageModel);
                    ImGui::SameLine();
                    ImGui::Checkbox("Module Names", &sdk::bShowModuleNames);
                    ImGui::Combo("Render Mode", &sdk::xray_render_mode, "Wireframe\0Solid\0Surface\0");
                    ImGui::Checkbox("Wireframe Overlay", &sdk::xray_wireframe_overlay);

                    ImGui::SeparatorText("Module Visibility");
                    ImGui::Checkbox("Gun Barrel", &sdk::bShowModuleGunBarrel); ImGui::SameLine();
                    ImGui::Checkbox("Cannon Breech", &sdk::bShowModuleCannonBreech);
                    ImGui::Checkbox("Crew", &sdk::bShowModuleCrew); ImGui::SameLine();
                    ImGui::Checkbox("Fuel Tank", &sdk::bShowModuleFuelTank);
                    ImGui::Checkbox("Ammo Rack", &sdk::bShowModuleAmmoRack); ImGui::SameLine();
                    ImGui::Checkbox("Engine", &sdk::bShowModuleEngine);
                    ImGui::Checkbox("Transmission", &sdk::bShowModuleTransmission);

                    ImGui::SliderFloat("Hide small modules >", &sdk::xray_small_module_distance, 50.0f, 2000.0f, "%.0f m");
                    ImGui::SliderFloat("Line Thickness", &sdk::xray_line_thickness, 0.2f, 3.0f, "%.1f");
                    ImGui::SliderFloat("Module Alpha", &sdk::xray_module_alpha, 0.0f, 1.0f, "%.2f");
                    {
                        static int xd = (int)sdk::xray_render_distance, nd = (int)sdk::xray_names_distance;
                        xd = (int)sdk::xray_render_distance; nd = (int)sdk::xray_names_distance;
                        ImGui::SliderInt("X-Ray Distance", &xd, 100, 5000, "%d m"); sdk::xray_render_distance = (float)xd;
                        ImGui::SliderInt("Names Distance", &nd, 100, 5000, "%d m"); sdk::xray_names_distance = (float)nd;
                    }
                    ImGui::TextDisabled("Colores de modulos en Colors > X-Ray.");
                    ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::EndTabItem();
                }

                // Radar: sin sliders; usa la geometria nativa del mapa.
                if (ImGui::BeginTabItem("Radar"))
                {
                    ImGui::Checkbox("Enable Radar", &sdk::radar_enabled);
                    ImGui::TextDisabled("Marcadores enemigos sobre minimapa y mapa tactico.");
                    ImGui::TextDisabled("La lista de entidades actual excluye aliados.");
                    ImGui::EndTabItem();
                }

                // Aimbot tab
                if (ImGui::BeginTabItem("Aimbot"))
                {
                    if (ImGui::BeginTabBar("Aimbot Sections"))
                    {
                        if (ImGui::BeginTabItem("Ground"))
                        {
                            ImGui::Checkbox("Enable Aimbot", &sdk::aimbot_enabled);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Air Lead"))
                        {
                    ImGui::Text("Air Lead Indicator");
                    ImGui::Checkbox("Enable Air Lead", &sdk::air_lead_enabled);
                    ImGui::Checkbox("Use Simulation", &sdk::air_lead_use_simulation);
                    ImGui::Checkbox("Only Target (center)", &sdk::air_lead_only_target);
                    ImGui::TextDisabled("Color del indicador en Colors > Air Lead.");
                    ImGui::SliderFloat("Marker Size", &sdk::air_lead_marker_size, 2.0f, 15.0f, "%.1f");
                    ImGui::SliderFloat("Line Thickness", &sdk::air_lead_line_thickness, 0.5f, 3.0f, "%.1f");

                    ImGui::SeparatorText("Air Lead Extra");
                    ImGui::SliderFloat("Extra Lead Time (s)", &sdk::air_lead_extra_time, 0.0f, 0.5f, "%.2f");

                    ImGui::SeparatorText("Air Lead Targets");
                    ImGui::Checkbox("Helicopters", &sdk::air_lead_target_helicopter);
                    ImGui::Checkbox("Recon Drones", &sdk::air_lead_target_recon_drone);
                    ImGui::Checkbox("Fighters", &sdk::air_lead_target_fighter);
                    ImGui::Checkbox("Bombers", &sdk::air_lead_target_bomber);
                    ImGui::Checkbox("Attackers", &sdk::air_lead_target_attacker);
                    ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::EndTabItem();
                }

                // Todos los colores editables reunidos por categoria.
                if (ImGui::BeginTabItem("Colors"))
                {
                    if (ImGui::BeginTabBar("Color Sections"))
                    {
                        if (ImGui::BeginTabItem("Aircraft"))
                        {
                            ImGui::ColorEdit4("Helicopter", (float*)&sdk::color_helicopter);
                            ImGui::ColorEdit4("Bomber", (float*)&sdk::color_bomber);
                            ImGui::ColorEdit4("Fighter", (float*)&sdk::color_fighter);
                            ImGui::ColorEdit4("Attacker", (float*)&sdk::color_attacker);
                            ImGui::ColorEdit4("Recon Drone", (float*)&sdk::color_recon_drone);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Ground"))
                        {
                            ImGui::ColorEdit4("Light Tank", (float*)&sdk::color_light_tank);
                            ImGui::ColorEdit4("Heavy Tank", (float*)&sdk::color_heavy_tank);
                            ImGui::ColorEdit4("Medium Tank", (float*)&sdk::color_medium_tank);
                            ImGui::ColorEdit4("SPAA", (float*)&sdk::color_spaa);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Naval"))
                        {
                            ImGui::ColorEdit4("Gun Boat", (float*)&sdk::color_gun_boat);
                            ImGui::ColorEdit4("Destroyer", (float*)&sdk::color_destroyer);
                            ImGui::ColorEdit4("Cruiser", (float*)&sdk::color_cruiser);
                            ImGui::ColorEdit4("Torpedo Boat", (float*)&sdk::color_torpedo_boat);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("X-Ray"))
                        {
                            ImGui::ColorEdit4("Gun Barrel", (float*)&sdk::color_mod_gun_barrel);
                            ImGui::ColorEdit4("Cannon Breech", (float*)&sdk::color_mod_cannon_breech);
                            ImGui::ColorEdit4("Crew", (float*)&sdk::color_mod_crew);
                            ImGui::ColorEdit4("Fuel Tank", (float*)&sdk::color_mod_fuel_tank);
                            ImGui::ColorEdit4("Ammo Rack", (float*)&sdk::color_mod_ammo_rack);
                            ImGui::ColorEdit4("Engine", (float*)&sdk::color_mod_engine);
                            ImGui::ColorEdit4("Transmission", (float*)&sdk::color_mod_transmission);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Radar"))
                        {
                            ImGui::ColorEdit4("Enemy Marker", (float*)&sdk::radar_enemy_color);
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Air Lead"))
                        {
                            ImGui::ColorEdit4("Marker", (float*)&sdk::air_lead_sim_color);
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::EndTabItem();
                }

                // Profiles tab
                if (ImGui::BeginTabItem("Profiles"))
                {
                    ImGui::Text("Active Profile: %d", active_preset + 1);
                    static char name_buf[5][64] = {};
                    static bool initialized = false;
                    if (!initialized) {
                        for (int i = 0; i < 5; i++) {
                            strncpy_s(name_buf[i], sizeof(name_buf[i]), presets[i].name.c_str(), sizeof(name_buf[i]) - 1);
                            name_buf[i][sizeof(name_buf[i]) - 1] = '\0';
                        }
                        initialized = true;
                    }
                    for (int i = 0; i < 5; i++) {
                        ImGui::PushID(i);
                        ImGui::SetNextItemWidth(150);
                        if (ImGui::InputText("##name", name_buf[i], IM_ARRAYSIZE(name_buf[i])))
                            presets[i].name = name_buf[i];
                        if (!ImGui::IsItemActive() && presets[i].name != name_buf[i])
                            strncpy_s(name_buf[i], sizeof(name_buf[i]), presets[i].name.c_str(), _TRUNCATE);
                        ImGui::SameLine();
                        if (ImGui::Button("Load")) {
                            if (i != active_preset) sync_active_preset_from_runtime();
                            presets[i].name = name_buf[i];
                            active_preset = i;
                            apply_preset(presets[i]);
                            save_now();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Save")) {
                            presets[i] = capture_current_settings();
                            presets[i].name = name_buf[i];
                            save_now();
                        }
                        ImGui::SameLine();
                        ImGui::Text("%s", (active_preset == i) ? "[Active]" : " ");
                        ImGui::PopID();
                    }
                    ImGui::EndTabItem();
                }

                // Settings tab
                if (ImGui::BeginTabItem("Settings"))
                {
                    if (ImGui::BeginTabBar("Settings Sections"))
                    {
                        if (ImGui::BeginTabItem("Video & Monitor"))
                        {
                    int safe_monitor_index = sdk::monitor_index;
                    if (sdk::monitor_count <= 0) safe_monitor_index = 0;
                    else { if (safe_monitor_index < 0) safe_monitor_index = 0; if (safe_monitor_index > sdk::monitor_count - 1) safe_monitor_index = sdk::monitor_count - 1; }
                    sdk::monitor_index = safe_monitor_index;

                    const char* preview = "Unknown monitor";
                    if (!sdk::monitor_labels.empty() && safe_monitor_index < (int)sdk::monitor_labels.size())
                        preview = sdk::monitor_labels[safe_monitor_index].c_str();
                    if (ImGui::BeginCombo("Monitor", preview))
                    {
                        for (int i = 0; i < (int)sdk::monitor_labels.size(); ++i)
                        {
                            const bool is_selected = (sdk::monitor_index == i);
                            if (ImGui::Selectable(sdk::monitor_labels[i].c_str(), is_selected))
                            {
                                sdk::monitor_index = i;
                                sync_shared_settings_to_all_presets();
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::Button("Apply Fullscreen to Monitor")) sdk::request_monitor_apply = true;
                    if (ImGui::Checkbox("Apply Fullscreen on Start", &sdk::auto_fullscreen_on_start))
                    {
                        sync_shared_settings_to_all_presets();
                    }
                    ImGui::SeparatorText("Video");
                    ImGui::Checkbox("V-Sync", &sdk::esp_vsync_enabled);
                    ImGui::SetNextItemWidth(135);
                    if (ImGui::InputInt("Target FPS", &sdk::esp_vsync_fps))
                        sdk::esp_vsync_fps = std::clamp(sdk::esp_vsync_fps, 30, 500);
                    ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Hotkeys"))
                        {
                            ImGui::SeparatorText("ESP");
                            DrawKeybindRow("Toggle ESP", sdk::key_esp_enable);
                            DrawKeybindRow("2D Box", sdk::key_2d_box);
                            DrawKeybindRow("3D Box", sdk::key_3d_box);
                            DrawKeybindRow("Vehicle Name", sdk::key_vehicle_name);
                            DrawKeybindRow("Distance", sdk::key_distance);
                            ImGui::SeparatorText("X-Ray");
                            DrawKeybindRow("Toggle X-Ray", sdk::xray_toggle_key);
                            DrawKeybindRow("Show Crew", sdk::key_show_crew);
                            DrawKeybindRow("Show Barrel", sdk::key_show_barrel);
                            DrawKeybindRow("Show AmmoRack", sdk::key_show_ammorack);
                            DrawKeybindRow("Show Fueltank", sdk::key_show_fueltank);
                            ImGui::SeparatorText("Aimbot");
                            DrawKeybindRow("Toggle Aimbot", sdk::key_aimbot_enable);
                            DrawKeybindRow("Toggle Air Lead", sdk::key_air_lead_enable);
                            ImGui::TextDisabled("Menu: F1 | Profiles: F2");
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Appearance"))
                        {
                    ImGui::Combo("Theme", &sdk::menu_style, "Dark\0Classic\0Crimson\0Midnight\0Minimal Light\0Forest\0Ocean\0");
                    ApplyMenuStyle(sdk::menu_style);
                    ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Offsets"))
                        {
                    if (ImGui::Button("Create offsets.txt")) offsets::create_or_update_external_offsets_file();
                    ImGui::SameLine();
                    bool external = offsets::is_external_offsets_enabled();
                    if (ImGui::Checkbox("Read offsets.txt", &external)) {
                        offsets::set_external_offsets_enabled(external);
                        sdk::request_manual_reload = true;
                    }
                    ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("Config"))
                        {
                    if (ImGui::Button("Export Config")) {
                        OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                        char file[260] = "cheat_config.cfg";
                        ofn.lpstrFile = file;
                        ofn.nMaxFile = 260;
                        ofn.lpstrFilter = "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                        ofn.Flags = OFN_OVERWRITEPROMPT;
                        if (GetSaveFileNameA(&ofn)) ExportConfig(file);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Import Config")) {
                        OPENFILENAMEA ofn = { sizeof(OPENFILENAMEA) };
                        char file[260] = "";
                        ofn.lpstrFile = file;
                        ofn.nMaxFile = 260;
                        ofn.lpstrFilter = "Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
                        ofn.Flags = OFN_FILEMUSTEXIST;
                        if (GetOpenFileNameA(&ofn)) ImportConfig(file);
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Restart")) sdk::request_manual_reload = true;
                    if (ImGui::Button("Close Menu")) menu_open = false;
                    ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
            ImGui::Separator();
            const char* status_text = save_status == save_status_t::saved ? "Guardado"
                : save_status == save_status_t::pending ? "Pendiente" : "Error";
            ImGui::TextDisabled("Config: %s", status_text);
        }
        ImGui::End();
    }
}

// ====================== main() and helpers ======================
int main(int, char**)
{
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"SDK - ", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"SDK - ", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720, nullptr, nullptr, wc.hInstance, nullptr);
    GUI::initialize_presets();
    UpdateCurrentMonitor(hwnd);
    config::load();
    GUI::menu_open = true;
    GUI::initialize_autosave();

    if (!CreateDeviceD3D(hwnd)) { CleanupDeviceD3D(); ::UnregisterClassW(wc.lpszClassName, wc.hInstance); return TRUE; }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);
    if (sdk::auto_fullscreen_on_start) ApplyFullscreenToMonitor(hwnd, sdk::monitor_index);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_ViewportsEnable;

    ApplyMenuStyle(sdk::menu_style);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    g_render->init();
    if (!core::Thread()) return FALSE;

    bool done = false;
    while (!done)
    {
        const auto frame_start = std::chrono::steady_clock::now();
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        const bool is_foreground_process = []() {
            const HWND fg = ::GetForegroundWindow();
            if (!fg) return false;
            DWORD pid; ::GetWindowThreadProcessId(fg, &pid);
            return pid == ::GetCurrentProcessId();
            }();

        const bool f1_down_now = (::GetAsyncKeyState(VK_F1) & 0x8000) != 0;
        if (is_foreground_process && f1_down_now && !g_F1DownLastFrame) GUI::menu_open = !GUI::menu_open;
        g_F1DownLastFrame = f1_down_now;

        const bool f2_down_now = (::GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (is_foreground_process && f2_down_now && !g_F2DownLastFrame) { GUI::cycle_preset(); GUI::save_now(); }
        g_F2DownLastFrame = f2_down_now;

        if (is_foreground_process)
        {
            auto check_toggle = [](int key, bool& var, bool& last_state) {
                bool down = (::GetAsyncKeyState(key) & 0x8000) != 0;
                if (down && !last_state) var = !var;
                last_state = down;
                };
            static bool last_esp = false, last_aimbot = false, last_air_lead = false,
                last_3dbox = false, last_2dbox = false, last_vname = false,
                last_dist = false, last_crew = false, last_barrel = false,
                last_ammorack = false, last_fueltank = false;
            check_toggle(sdk::key_esp_enable, sdk::esp_enabled, last_esp);
            check_toggle(sdk::key_aimbot_enable, sdk::aimbot_enabled, last_aimbot);
            check_toggle(sdk::key_air_lead_enable, sdk::air_lead_enabled, last_air_lead);
            check_toggle(sdk::key_3d_box, sdk::esp_use_3d_boxes, last_3dbox);
            check_toggle(sdk::key_2d_box, sdk::esp_show_boxes, last_2dbox);
            check_toggle(sdk::key_vehicle_name, sdk::esp_show_vehicle_names, last_vname);
            check_toggle(sdk::key_distance, sdk::esp_show_distance, last_dist);
            check_toggle(sdk::key_show_crew, sdk::bShowModuleCrew, last_crew);
            check_toggle(sdk::key_show_barrel, sdk::bShowModuleGunBarrel, last_barrel);
            check_toggle(sdk::key_show_ammorack, sdk::bShowModuleAmmoRack, last_ammorack);
            check_toggle(sdk::key_show_fueltank, sdk::bShowModuleFuelTank, last_fueltank);
        }

        static bool xray_key_down_last = false;
        bool xray_key_down_now = (::GetAsyncKeyState(sdk::xray_toggle_key) & 0x8000) != 0;
        if (is_foreground_process && xray_key_down_now && !xray_key_down_last) sdk::bShowDamageModel = !sdk::bShowDamageModel;
        xray_key_down_last = xray_key_down_now;

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) { ::Sleep(10); continue; }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const ImVec2 display_size = io.DisplaySize;
        if (display_size.x > 0.0f && display_size.y > 0.0f) { sdk::screen_width = (int)display_size.x; sdk::screen_height = (int)display_size.y; }

        core::MaintainConnection(false);
        const int clamped = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
        if (clamped != sdk::monitor_index) sdk::monitor_index = clamped;
        if (sdk::request_monitor_apply) { ApplyFullscreenToMonitor(hwnd, sdk::monitor_index); sdk::request_monitor_apply = false; }
        if (sdk::request_manual_reload) { if (core::RestartRuntime()) sdk::manual_reload_status = "OK"; else if (sdk::manual_reload_status.empty()) sdk::manual_reload_status = "Failed"; sdk::request_manual_reload = false; }

        g_render->draw_list() = ImGui::GetBackgroundDrawList();
        GUI::OnRender();
        GUI::update_autosave();
        if (g_render->draw_list()) { misc::UpdateFrameSnapshot(); esp::run(); }

        ImGui::Render();
        const float clear_color[4] = { 0,0,0,0 };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) { ImGui::UpdatePlatformWindows(); ImGui::RenderPlatformWindowsDefault(); }

        static auto next_vsync = std::chrono::steady_clock::now();
        HRESULT hr = S_OK;
        if (sdk::esp_vsync_enabled) {
            int target = std::clamp(sdk::esp_vsync_fps, 30, 500);
            auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0 / target));
            next_vsync += period; auto now = std::chrono::steady_clock::now();
            if (next_vsync > now) std::this_thread::sleep_until(next_vsync); else next_vsync = now;
            hr = g_pSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) hr = S_OK;
            else if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) hr = S_OK;
        }
        else {
            next_vsync = std::chrono::steady_clock::now();
            hr = g_pSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING) hr = S_OK;
            else if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) hr = g_pSwapChain->Present(0, 0);
        }
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

        auto frame_end = std::chrono::steady_clock::now();
        misc::prof_main_frame_us.store((uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start).count(), std::memory_order_relaxed);
        misc::prof_main_frame_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (GUI::save_status != GUI::save_status_t::saved) GUI::save_now();
    core::Shutdown();
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    CleanupDeviceD3D(); ::DestroyWindow(hwnd); ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return FALSE;
}

// ====================== D3D / monitor / WndProc ======================
bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 0;
    sd.BufferDesc.RefreshRate.Denominator = 0;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = nullptr;
    const HRESULT get_buffer_hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(get_buffer_hr) || !pBackBuffer)
    {
        LOG("[D3D] GetBuffer failed: 0x%08X\n", static_cast<unsigned int>(get_buffer_hr));
        g_mainRenderTargetView = nullptr;
        return;
    }

    const HRESULT create_rtv_hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    if (FAILED(create_rtv_hr))
    {
        LOG("[D3D] CreateRenderTargetView failed: 0x%08X\n", static_cast<unsigned int>(create_rtv_hr));
        g_mainRenderTargetView = nullptr;
    }

    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

int clamp_monitor_index(int value, int count)
{
    if (count <= 0) return 0;
    if (value < 0)  return 0;
    if (value > count - 1) return count - 1;
    return value;
}

struct monitor_apply_t
{
    int current_index = -1;
    int target_index = 0;
    bool found = false;
    RECT rc = {};
};

BOOL CALLBACK collect_monitor_for_index(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    monitor_apply_t* result = reinterpret_cast<monitor_apply_t*>(data);
    if (!result) return FALSE;

    result->current_index++;
    if (result->current_index != result->target_index) return TRUE;

    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) return FALSE;

    result->rc = monitor_info.rcMonitor;
    result->found = true;
    return FALSE;
}

struct monitor_list_state_t
{
    int count = 0;
    int current_index = 0;
    HMONITOR current_monitor = nullptr;
    std::vector<std::string> labels = {};
};

BOOL CALLBACK collect_monitor_list_callback(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    monitor_list_state_t* state = reinterpret_cast<monitor_list_state_t*>(data);
    if (!state) return FALSE;

    MONITORINFOEXW monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) return TRUE;

    const RECT& rc = monitor_info.rcMonitor;
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    char device_name[CCHDEVICENAME] = {};
    ::WideCharToMultiByte(CP_UTF8, 0, monitor_info.szDevice, -1, device_name, static_cast<int>(sizeof(device_name)), nullptr, nullptr);

    std::ostringstream label;
    label << device_name << " (" << width << "x" << height << ")";
    state->labels.emplace_back(label.str());

    if (monitor == state->current_monitor)
        state->current_index = state->count;

    state->count++;
    return TRUE;
}

void UpdateCurrentMonitor(HWND hWnd)
{
    monitor_list_state_t state = {};
    state.current_monitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    ::EnumDisplayMonitors(nullptr, nullptr, collect_monitor_list_callback, reinterpret_cast<LPARAM>(&state));

    sdk::monitor_count = std::max(state.count, 1);
    if (!state.labels.empty())
        sdk::monitor_labels = std::move(state.labels);
    else
        sdk::monitor_labels = { "Unknown monitor" };

    sdk::monitor_index = clamp_monitor_index(sdk::monitor_index, sdk::monitor_count);
    g_CurrentMonitor = state.current_monitor;
}

void ApplyFullscreenToMonitor(HWND hWnd, int index)
{
    sdk::monitor_index = index;

    monitor_apply_t data = {};
    data.target_index = sdk::monitor_index;
    ::EnumDisplayMonitors(nullptr, nullptr, collect_monitor_for_index, reinterpret_cast<LPARAM>(&data));
    if (!data.found) return;

    const RECT& rc = data.rc;
    ::SetWindowLongPtrW(hWnd, GWL_STYLE, WS_POPUP);
    ::SetWindowLongPtrW(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_APPWINDOW);
    ::SetWindowPos(hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    g_ResizeWidth = static_cast<UINT>(rc.right - rc.left);
    g_ResizeHeight = static_cast<UINT>(rc.bottom - rc.top);
    UpdateCurrentMonitor(hWnd);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_MOVE:
    case WM_DISPLAYCHANGE:
        UpdateCurrentMonitor(hWnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}