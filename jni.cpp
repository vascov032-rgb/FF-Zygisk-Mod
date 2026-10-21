#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cmath>
#include <cstring>
#include <string>
#include <fstream>
#include "zygisk.hpp"
#include "imgui.h"
#include "imgui_impl_opengl3.h"

#define LOG_TAG "FF_MENU"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

bool show_menu = true;

typedef void (*eglSwapBuffers_t)(void* dpy, void* surf);
eglSwapBuffers_t orig_eglSwapBuffers;

void hooked_eglSwapBuffers(void* dpy, void* surf) {
    static bool initialized = false;
    if (!initialized) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL; 
        ImGui::StyleColorsDark();
        ImGui_ImplOpenGL3_Init("#version 300 es");
        initialized = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (show_menu) {
        ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_FirstUseEver);
        ImGui::Begin("V CARRIER - FREE FIRE PRO", &show_menu);
        ImGui::Text("Aimbot e ESP (Em breve)");
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

class FfMenu : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->env = env;
    }
    
    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        const char *process = env->GetStringUTFChars(args->nice_name, nullptr);
        
        // MUDANÇA: Salvando em /data/local/tmp/ em vez de /sdcard/
        std::ofstream log("/data/local/tmp/zygisk_debug.txt", std::ios::app);
        if (log.is_open()) {
            log << "Zygisk viu: " << (process ? process : "null") << "\n";
            log.close();
        }

        if (process && strstr(process, "com.dts.freefireth") != NULL) {
            std::ofstream ff_log("/data/local/tmp/zygisk_debug.txt", std::ios::app);
            ff_log << "!!! FREE FIRE DETECTADO, INJETANDO PAINEL !!!\n";
            ff_log.close();
            
            void* egl = dlopen("libEGL.so", RTLD_LAZY);
            if (egl) {
                void* addr = dlsym(egl, "eglSwapBuffers");
                if (addr) {
                    InlineHook(addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
                }
            }
        }
    }

private:
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(FfMenu)
