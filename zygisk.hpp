#pragma once
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
#define REGISTER_ZYGISK_MODULE(cls) \
    extern "C" __attribute__((visibility("default"))) void zygisk_module_entry(zygisk::Api *api, JNIEnv *env) { \
        static cls instance; instance.onLoad(api, env); \
    }
