// Let's Go Recomp start screen: resolution, fullscreen, Play - nothing else. Replaces LASRecomp's start
// screen (launcher.mm: Link's Awakening's randomizer, seeds, update check, game-file setup) on Windows / Linux; macOS
// keeps the old one. The choices go to larecomp.ini (settings.c: render_height, fullscreen).
// Tests: LARECOMP_LAUNCHER_SHOT=<frame> saves build/launcher.png (or LARECOMP_SHOT_PATH) and exits;
// LARECOMP_LAUNCHER_AUTOPLAY=1 plays at once.
#include <SDL3/SDL.h>
#include <float.h>
#include <stdlib.h>

#include "imgui.h"
#ifndef __APPLE__
#include "imgui_impl_vulkan.h"
#endif
#include "imgui_impl_sdl3.h"

extern "C" {
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
    bool full = g_settings.fullscreen;
    long shot_at = getenv("LARECOMP_LAUNCHER_SHOT") ? atol(getenv("LARECOMP_LAUNCHER_SHOT")) : 0, frame = 0;
    bool play = getenv("LARECOMP_LAUNCHER_AUTOPLAY") != NULL;
    static const float bg[4] = {0.08f, 0.08f, 0.10f, 1};

    while (!play) {
        menu_apply_window_requests(win);
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) exit(0);
        }
        if (!vk_frame_begin(bg)) { SDL_Delay(16); continue; }
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2, io.DisplaySize.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin(RT_APP_NAME, NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushItemWidth(420);
        ImGui::Combo("Resolução", &res, res_names, 5);
        if (ImGui::Checkbox("Tela cheia", &full)) SDL_SetWindowFullscreen(win, full);
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
    settings_save();
    vk_imgui_backend_shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(ctx);
    ImGui::SetCurrentContext(prev_ctx);
}
#endif
