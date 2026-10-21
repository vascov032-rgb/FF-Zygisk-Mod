#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <dirent.h>
#include "zygisk.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

// CONFIGURAÇÕES DO PAINEL
struct Config {
    bool aimbot_enabled = false;
    int aimbot_hs = 100; // 50, 70, 80, 100
    float aimbot_fov = 150.0f;
    bool esp_box = true;
    bool esp_skeleton = true;
    bool esp_name = true;
    bool esp_distance = true;
} cfg;

// OFFSETS (Voce precisa pegar no seu dump/scan)
#define OFF_LOCAL_PLAYER 0x0 // Substitua pelo offset real do Player
#define OFF_VIEW_MATRIX  0x0 // Substitua pelo offset real da Camera
#define OFF_PITCH        0x0
#define OFF_YAW          0x0

using zygisk::Api;
using zygisk::AppSpecializeArgs;

#define LOG_TAG "FF_INTERNAL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef void (*eglSwapBuffers_t)(void* dpy, void* surf);
eglSwapBuffers_t orig_eglSwapBuffers;
uintptr_t game_base = 0;

// Matematica do Aimbot (Mira na cabeca)
void Aimbot(void* localPlayer, void* targetEnemy) {
    // Calcula a posicao do alvo (Cabeca ou Peito baseado no HS%)
    // float target_y = lerp(chest_y, head_y, cfg.aimbot_hs / 100.0f);
    // Calcula os angulos de Pitch e Yaw
    // Escreve direto na memoria (Internal nao e sobrescrito pelo jogo tao facil)
}

// Hook do EGL: Roda a cada frame (60 FPS)
void hooked_eglSwapBuffers(void* dpy, void* surf) {
    // 1. Inicia o Frame do ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // 2. Desenha o Menu se o jogador quiser
    ImGui::Begin("FREE FIRE PRO PANEL", nullptr, ImGuiWindowFlags_NoCollapse);
    if (ImGui::BeginTabBar("Tabs")) {
        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Checkbox("Ativar Aimbot", &cfg.aimbot_enabled);
            if (cfg.aimbot_enabled) {
                ImGui::Text("Hitbox HS:");
                ImGui::RadioButton("50%%", &cfg.aimbot_hs, 50); ImGui::SameLine();
                ImGui::RadioButton("70%%", &cfg.aimbot_hs, 70); ImGui::SameLine();
                ImGui::RadioButton("80%%", &cfg.aimbot_hs, 80); ImGui::SameLine();
                ImGui::RadioButton("100%%", &cfg.aimbot_hs, 100);
                ImGui::SliderFloat("FOV", &cfg.aimbot_fov, 10.0f, 400.0f, "%.0f px");
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("Caixas", &cfg.esp_box);
            ImGui::Checkbox("Esqueleto", &cfg.esp_skeleton);
            ImGui::Checkbox("Nomes", &cfg.esp_name);
            ImGui::Checkbox("Distancia", &cfg.esp_distance);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // 3. Logica do ESP (Desenha as caixas na tela)
    if (cfg.esp_box) {
        // Pega os inimigos da memoria, converte 3D pra 2D (WorldToScreen) e desenha
        // ImGui::GetBackgroundDrawList()->AddRect(p1, p2, ImColor(255,0,0));
    }

    // 4. Renderiza o ImGui na tela do jogo
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 5. Chama a funcao original do jogo
    orig_eglSwapBuffers(dpy, surf);
}

void InlineHook(void* target, void* replace, void** backup) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page_start = (uintptr_t)target & ~(page_size - 1);
    mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC);
    memcpy(*backup, target, 4);
    uint32_t branch_insn = 0x14000000 | (((uintptr_t)replace - (uintptr_t)target) >> 2 & 0x03FFFFFF);
    memcpy(target, &branch_insn, 4);
    __builtin___clear_cache((char*)page_start, (char*)(page_start + page_size * 2));
}

class FfCheatModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        const char *process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process && strstr(process, "com.dts.freefireth") != NULL) {
            LOGI("Injetando Painel Interno no Free Fire!");
            
            // Hook do EGL para desenhar o ImGui
            void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
            if (egl_handle) {
                void* egl_addr = dlsym(egl_handle, "eglSwapBuffers");
                if (egl_addr) {
                    InlineHook(egl_addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
                    LOGI("Hook de Renderizacao Ativado!");
                }
            }
        }
    }
private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(FfCheatModule)
