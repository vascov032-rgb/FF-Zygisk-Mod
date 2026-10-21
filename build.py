import os

os.chdir(os.path.expanduser("~/ff_zygisk"))

# 1. Escreve o Zygisk HPP
hpp_code = """#pragma once
#include <jni.h>
#include <sys/types.h>
#include <unistd.h>

namespace zygisk {
struct AppSpecializeArgs {
    jstring nice_name;
};
struct ServerSpecializeArgs {};
class Api {
public:
    int connectCompanion();
    int getModuleDir();
    void setOption(int);
};
class ModuleBase {
public:
    virtual void onLoad(Api *api, JNIEnv *env) {}
    virtual void preAppSpecialize(AppSpecializeArgs *args) {}
    virtual void postAppSpecialize(const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize(ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs *args) {}
};
}
#define REGISTER_ZYGISK_MODULE(cls) \\
    extern "C" __attribute__((visibility("default"))) void zygisk_module_entry(zygisk::Api *api, JNIEnv *env) { \\
        static cls instance; instance.onLoad(api, env); \\
    }
"""
with open("zygisk.hpp", "w") as f:
    f.write(hpp_code)

# 2. Escreve o Código C++
cpp_code = """#define _GNU_SOURCE
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
#include <sys/uio.h>
#include <pthread.h>
#include "zygisk.hpp"

extern "C" ssize_t process_vm_readv(pid_t pid, const struct iovec *local_iov, unsigned long liovcnt, const struct iovec *remote_iov, unsigned long riovcnt, unsigned long flags);

using zygisk::Api;
using zygisk::AppSpecializeArgs;

#define LOG_TAG "FF_CHEAT"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

uintptr_t g_view_matrix = 0;

struct Vec3 { float x, y, z; };

int get_pid(const char* package_name) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            bool is_num = true;
            for (int i = 0; entry->d_name[i] != '\\0'; i++) {
                if (!isdigit(entry->d_name[i])) { is_num = false; break; }
            }
            if (is_num) {
                char cmdline_path[256];
                snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", entry->d_name);
                std::ifstream f(cmdline_path);
                if (f) {
                    std::string line;
                    std::getline(f, line);
                    if (line.find(package_name) != std::string::npos) {
                        closedir(dir);
                        return atoi(entry->d_name);
                    }
                }
            }
        }
    }
    closedir(dir);
    return -1;
}

bool ReadMem(int pid, uintptr_t addr, void* buffer, size_t size) {
    struct iovec local{ buffer, size };
    struct iovec remote{ (void*)addr, size };
    return process_vm_readv(pid, &local, 1, &remote, 1, 0) == size;
}

bool IsValidMatrix(float* m) {
    float len0 = m[0]*m[0] + m[1]*m[1] + m[2]*m[2];
    float len1 = m[4]*m[4] + m[5]*m[5] + m[6]*m[6];
    float len2 = m[8]*m[8] + m[9]*m[9] + m[10]*m[10];
    if (std::abs(len0 - 1.0f) > 0.05f || std::abs(len1 - 1.0f) > 0.05f || std::abs(len2 - 1.0f) > 0.05f) return false;
    
    float dot01 = m[0]*m[4] + m[1]*m[5] + m[2]*m[6];
    float dot02 = m[0]*m[8] + m[1]*m[9] + m[2]*m[10];
    float dot12 = m[4]*m[8] + m[5]*m[9] + m[6]*m[10];
    if (std::abs(dot01) > 0.05f || std::abs(dot02) > 0.05f || std::abs(dot12) > 0.05f) return false;
    
    if (std::abs(m[3]) > 0.05f || std::abs(m[7]) > 0.05f || std::abs(m[11]) > 0.05f) return false;
    if (std::abs(m[15] - 1.0f) > 0.05f) return false;
    if (m[0] == 1.0f && m[5] == 1.0f && m[10] == 1.0f) return false;
    if (std::abs(m[14]) < 0.5f) return false;
    
    float x = m[12], y = m[13], z = m[14];
    if (std::isnan(x) || std::isinf(x) || std::isnan(y) || std::isinf(y) || std::isnan(z) || std::isinf(z)) return false;
    if (std::abs(x) > 50000.0f || std::abs(y) > 50000.0f || std::abs(z) > 50000.0f) return false;
    return true;
}

void* ScannerThread(void*) {
    sleep(5);
    int pid = get_pid("com.dts.freefireth");
    if (pid == -1) return nullptr;
    
    LOGI("Procurando ViewMatrix...");
    std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
    std::string line;
    
    while (std::getline(maps, line)) {
        if (line.find("rw-p") == std::string::npos) continue;
        uintptr_t start, end;
        sscanf(line.c_str(), "%lx-%lx", &start, &end);
        size_t size = end - start;
        if (size > 50 * 1024 * 1024) continue;
        
        std::vector<char> buffer(size);
        if (!ReadMem(pid, start, buffer.data(), size)) continue;
        
        for (size_t i = 0; i <= size - 64; i += 4) {
            float* m = (float*)&buffer[i];
            if (IsValidMatrix(m)) {
                g_view_matrix = start + i;
                LOGI("ViewMatrix encontrada em: 0x%lX", g_view_matrix);
                return nullptr;
            }
        }
    }
    return nullptr;
}

typedef void (*eglSwapBuffers_t)(void* dpy, void* surf);
eglSwapBuffers_t orig_eglSwapBuffers;

void hooked_eglSwapBuffers(void* dpy, void* surf) {
    if (g_view_matrix != 0) {
        // ESP e Aimbot rodam aqui
    }
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
        if (process && strcmp(process, "com.dts.freefireth") == 0) {
            LOGI("Free Fire detectado! Injetando cheat...");
            pthread_t thread;
            pthread_create(&thread, nullptr, ScannerThread, nullptr);

            void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
            if (egl_handle) {
                void* egl_addr = dlsym(egl_handle, "eglSwapBuffers");
                if (egl_addr) {
                    InlineHook(egl_addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
                    LOGI("Hook do EGL ativado com sucesso!");
                }
            }
        }
    }
private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(FfCheatModule)
"""
with open("jni.cpp", "w") as f:
    f.write(cpp_code)

print("[+] Arquivos escritos com sucesso.")
os.system("mkdir -p zygisk")
os.system("clang++ -shared -fPIC -o zygisk/arm64-v8a.so jni.cpp --target=aarch64-linux-android21 -std=c++17 -llog -ldl -I. -pthread")
os.system("zip -r /sdcard/Download/ff_zygisk.zip module.prop zygisk/")
