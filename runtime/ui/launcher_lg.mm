// Let's Go Recomp start screen: resolution, fullscreen, anti-aliasing, translation (mods/), the keyboard / mouse
// controls, Play - nothing else. Replaces LASRecomp's start screen (launcher.mm: Link's Awakening's randomizer, seeds, update check, game-file
// setup) on Windows / Linux; macOS keeps the old one. The choices go to larecomp.ini (settings.c: render_height,
// fullscreen, smaa, mod, mouse_sens; bind_key_* from runtime/hle/bindings.c).
// Tests: LARECOMP_LAUNCHER_SHOT=<frame> saves build/launcher.png (or LARECOMP_SHOT_PATH) and exits;
// LARECOMP_LAUNCHER_AUTOPLAY=1 plays at once; LARECOMP_LAUNCHER_KEYS=1 opens the keyboard section,
// LARECOMP_LAUNCHER_KEYS=<slot> also waits for a new key for that input (runtime/input_bind.h BIND_*), and
// <slot>:<key name> presses that key for it.
#include <SDL3/SDL.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "imgui.h"
#ifndef __APPLE__
#include "imgui_impl_vulkan.h"
#endif
#include "imgui_impl_sdl3.h"

extern "C" {
#include "input_bind.h"
#include "mods.h"
#include "paths.h"
#include "settings.h"
void settings_save(void);
void ui_window_init(void);
SDL_Window *ui_window(void);
void menu_apply_window_requests(void *sdl_window);
#ifndef __APPLE__
bool vk_frame_begin(const float clear[4]);
void vk_frame_end(const char *shot_path);
void *vk_frame_cmd(void);
bool vk_imgui_backend_init(void);
void vk_imgui_backend_shutdown(void);
#endif
}

#ifndef __APPLE__
// Keyboard: the game's actions in Let's Go's words with the key bound to each. Click one, then press the new key (Esc
// or 5 s cancels; a key already in use swaps over). Controllers keep their buttons: nothing to set for them.
static const struct { int slot; const char *name; } k_actions[] = {
    {BIND_LS_UP, "Andar para cima"},        {BIND_A, "A: confirmar / arremessar"},
    {BIND_LS_DOWN, "Andar para baixo"},     {BIND_B, "B: cancelar"},
    {BIND_LS_LEFT, "Andar para a esquerda"}, {BIND_X, "X: menu"},
    {BIND_LS_RIGHT, "Andar para a direita"}, {BIND_Y, "Y: opções / caixa"},
    {BIND_UP, "Direcional cima"},           {BIND_L, "L: mover Pokémon / itens"},
    {BIND_DOWN, "Direcional baixo"},        {BIND_R, "R: mover Pokémon / itens"},
    {BIND_LEFT, "Direcional esquerda"},     {BIND_PLUS, "+: parceiro"},
    {BIND_RIGHT, "Direcional direita"},     {BIND_MINUS, "-: Pokédex"},
};

static const char *key_name(int sc) {
    switch (sc) {
    case SDL_SCANCODE_SPACE: return "Espaço";
    case SDL_SCANCODE_RETURN: return "Enter";
    case SDL_SCANCODE_UP: return "Seta cima";
    case SDL_SCANCODE_DOWN: return "Seta baixo";
    case SDL_SCANCODE_LEFT: return "Seta esquerda";
    case SDL_SCANCODE_RIGHT: return "Seta direita";
    case SDL_SCANCODE_LSHIFT: return "Shift esq.";
    case SDL_SCANCODE_RSHIFT: return "Shift dir.";
    case SDL_SCANCODE_LCTRL: return "Ctrl esq.";
    case SDL_SCANCODE_RCTRL: return "Ctrl dir.";
    case 0: return "(nenhuma)";
    default: return bind_key_label(sc);
    }
}

static void keys_section(void) {
    int cap_dev, cap_slot, ms_left;
    bind_capture_active(&cap_dev, &cap_slot, &ms_left);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 1));
    if (ImGui::BeginTable("##keys", 4, ImGuiTableFlags_SizingFixedFit)) {
        for (size_t i = 0; i < sizeof k_actions / sizeof k_actions[0]; i++) {
            int slot = k_actions[i].slot;
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(k_actions[i].name);
            ImGui::TableNextColumn();
            bool waiting = cap_dev == BIND_DEV_KEY && cap_slot == slot;
            char lbl[96];
            if (waiting) snprintf(lbl, sizeof lbl, "Aperte a tecla... %d##k%d", (ms_left + 999) / 1000, slot);
            else snprintf(lbl, sizeof lbl, "%s##k%d", key_name(g_bind_key[slot]), slot);
            int pushed = 0;
            if (waiting) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.42f, 0.12f, 1)); pushed++; }
            if (!bind_is_default_slot(BIND_DEV_KEY, slot)) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.82f, 0.38f, 1)); pushed++; }
            if (ImGui::Button(lbl, ImVec2(230, 0))) {
                if (waiting) bind_capture_cancel();
                else bind_capture_begin(BIND_DEV_KEY, slot);
            }
            ImGui::PopStyleColor(pushed);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar(2);
    ImGui::TextDisabled("Enter e Backspace também valem como A e B. Esc cancela a troca.");
    if (!bind_is_default(BIND_DEV_KEY) && ImGui::Button("Restaurar teclas padrão")) bind_reset(BIND_DEV_KEY);
    ImGui::TextUnformatted("Mouse: clique na janela para prender (Esc solta); esquerdo = A, direito = B, mover = mirar.");
    ImGui::PushItemWidth(420);
    ImGui::SliderFloat("Sensibilidade do mouse", &g_settings.mouse_sens, 0.02f, 0.30f, "%.2f");
    ImGui::PopItemWidth();
}

extern "C" void launcher_lg_run(const char *root) {
    (void)root;
    ui_window_init();
    SDL_Window *win = ui_window();
    ImGuiContext *prev_ctx = ImGui::GetCurrentContext();  // (the in-game menu's, when back here after a reboot)
    ImGuiContext *ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = NULL;
    static const ImWchar ranges[] = {0x20, 0xFF, 0};  // (Latin-1: Portuguese accents)
    if (!io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 26, NULL, ranges)) io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.5f);
    ImGui_ImplSDL3_InitForVulkan(win);
    vk_imgui_backend_init();

    static const char *const res_names[] = {"Automática (tamanho da janela)", "720p", "1080p", "1440p", "4K"};
    static const int res_lines[] = {0, 720, 1080, 1440, 2160};
    int res = 0;
    for (int i = 0; i < 5; i++) if (res_lines[i] == g_settings.res_height) res = i;
    bool full = g_settings.fullscreen, smaa = g_settings.antialias == AA_SMAA;
    // Translation: the folders under mods/ (runtime/mods.h); 0 = none (the game's own English)
    static char mods[MODS_MAX][128];
    int nmods = mods_list(mods, MODS_MAX), mod = 0;
    for (int i = 0; i < nmods; i++) if (!strcmp(mods[i], g_settings.mod)) mod = i + 1;
    long shot_at = getenv("LARECOMP_LAUNCHER_SHOT") ? atol(getenv("LARECOMP_LAUNCHER_SHOT")) : 0, frame = 0;
    bool play = getenv("LARECOMP_LAUNCHER_AUTOPLAY") != NULL;
    const char *keys_test = getenv("LARECOMP_LAUNCHER_KEYS");
    if (keys_test && atoi(keys_test) > 0) bind_capture_begin(BIND_DEV_KEY, atoi(keys_test));
    static const float bg[4] = {0.08f, 0.08f, 0.10f, 1};

    while (!play) {
        menu_apply_window_requests(win);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (bind_capture_event(&e)) continue;  // (waiting for a new key: the press is the binding's)
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) exit(0);
        }
        if (!vk_frame_begin(bg)) { SDL_Delay(16); continue; }
        if (keys_test && strchr(keys_test, ':') && frame == 5) {  // "<slot>:<key name>": the key pressed for the test
            SDL_Event k = {};
            k.type = SDL_EVENT_KEY_DOWN;
            k.key.scancode = SDL_GetScancodeFromName(strchr(keys_test, ':') + 1);
            k.key.down = true;
            SDL_PushEvent(&k);
        }
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin(RT_APP_NAME, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushItemWidth(420);
        ImGui::Combo("Resolução", &res, res_names, 5);
        if (ImGui::Checkbox("Tela cheia", &full)) SDL_SetWindowFullscreen(win, full);
        ImGui::Checkbox("Suavizar serrilhado", &smaa);
        if (nmods && ImGui::BeginCombo("Tradução", mod ? mods[mod - 1] : "Nenhuma (inglês)")) {
            if (ImGui::Selectable("Nenhuma (inglês)", mod == 0)) mod = 0;
            for (int i = 0; i < nmods; i++)
                if (ImGui::Selectable(mods[i], mod == i + 1)) mod = i + 1;
            ImGui::EndCombo();
        }
        if (keys_test) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::CollapsingHeader("Teclado e mouse")) keys_section();
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (frame == 0) ImGui::SetKeyboardFocusHere();
        if (ImGui::Button("Jogar", ImVec2(-FLT_MIN, 0))) play = true;
        ImGui::End();

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)vk_frame_cmd());
        bool shot = shot_at && ++frame == shot_at;
        if (!shot_at) frame++;
        vk_frame_end(shot ? (getenv("LARECOMP_SHOT_PATH") ? getenv("LARECOMP_SHOT_PATH") : "build/launcher.png") : NULL);
        if (shot) exit(0);
    }
    g_settings.res_height = res_lines[res];
    g_settings.fullscreen = full;
    g_settings.antialias = smaa ? AA_SMAA : AA_OFF;
    snprintf(g_settings.mod, sizeof g_settings.mod, "%s", mod ? mods[mod - 1] : "");
    settings_save();
    vk_imgui_backend_shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(prev_ctx);
}
#endif
