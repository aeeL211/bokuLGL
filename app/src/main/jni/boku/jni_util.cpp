#include "jni_util.h"

#include "Includes/Logger.h"
#include "jni_bridge.h"

EnvScope::EnvScope() {
    if (!lekatJni(&env_, &lepas_)) {
        env_ = nullptr;
        lepas_ = false;
    }
}

EnvScope::~EnvScope() {
    if (!lepas_ || !env_)
        return;
    JavaVM* vm = cariVmJava();
    if (vm)
        vm->DetachCurrentThread();
}

void bersihkanEksepsi(JNIEnv* env, const char* konteks) {
    if (!env || !env->ExceptionCheck())
        return;
    env->ExceptionClear();
    LOGW("Eksepsi JNI dibersihkan: %s", konteks ? konteks : "tanpa konteks");
}
