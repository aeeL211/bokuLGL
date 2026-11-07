#include "jni_util.h"

#include "Includes/Logger.h"
#include "jni_bridge.h"

EnvScope::EnvScope() {
    if (!lekatJni(&env_, &release_)) {
        env_ = nullptr;
        release_ = false;
    }
}

EnvScope::~EnvScope() {
    if (!release_ || !env_)
        return;
    JavaVM* vm = findVmJava();
    if (vm)
        vm->DetachCurrentThread();
}

void clearException(JNIEnv* env, const char* context) {
    if (!env || !env->ExceptionCheck())
        return;
    env->ExceptionClear();
    LOGW("Eksepsi JNI dibersihkan: %s", context ? context : "tanpa konteks");
}
