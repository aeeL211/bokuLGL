#include "jni_bridge.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {

std::atomic<JavaVM*> vmSaved{nullptr};
std::atomic<jclass> klassUnity{nullptr};

} // namespace

JavaVM* vmJava() { return vmSaved.load(std::memory_order_acquire); }

void setVmJava(JavaVM* vm) { vmSaved.store(vm, std::memory_order_release); }

jclass klassUnityPlayer() { return klassUnity.load(std::memory_order_acquire); }

void saveKlassUnityPlayer(JNIEnv* env) {
    if (klassUnity.load(std::memory_order_acquire))
        return;
    jclass klass = env->FindClass("com.unity3d.player.UnityPlayer");
    if (klass) {
        auto global = static_cast<jclass>(env->NewGlobalRef(klass));
        env->DeleteLocalRef(klass);
        if (global) {
            jclass kosong = nullptr;
            // Dua thread bisa menyimpan kelas yang sama; yang kalah menghapus
            // referensi globalnya sendiri supaya tidak ada yang bocor.
            if (!klassUnity.compare_exchange_strong(kosong, global))
                env->DeleteGlobalRef(global);
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

JavaVM* findVmJava() {
    JavaVM* saved = vmSaved.load(std::memory_order_acquire);
    if (saved)
        return saved;
    typedef jint (*GetVms)(JavaVM**, jsize, jsize*);
    auto getVm = reinterpret_cast<GetVms>(dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
    if (!getVm)
        return nullptr;
    JavaVM* vm = nullptr;
    jsize count = 0;
    if (getVm(&vm, 1, &count) != JNI_OK || count < 1)
        return nullptr;
    vmSaved.store(vm, std::memory_order_release);
    return vm;
}

bool lekatJni(JNIEnv** env, bool* release) {
    JavaVM* vm = findVmJava();
    if (!vm)
        return false;
    *release = false;
    if (vm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6) == JNI_OK)
        return true;
    if (vm->AttachCurrentThread(env, nullptr) != JNI_OK)
        return false;
    *release = true;
    return true;
}

bool rootStorageExternal(char* out, size_t size) {
    JNIEnv* env = nullptr;
    bool release = false;
    if (!lekatJni(&env, &release))
        return false;

    bool success = false;
    jclass klassEnv = env->FindClass("android/os/Environment");
    if (klassEnv) {
        jmethodID methodRoot =
            env->GetStaticMethodID(klassEnv, "getExternalStorageDirectory", "()Ljava/io/File;");
        if (methodRoot) {
            jobject root = env->CallStaticObjectMethod(klassEnv, methodRoot);
            if (root) {
                jclass klassFile = env->FindClass("java/io/File");
                jmethodID methodPath = klassFile ? env->GetMethodID(klassFile, "getAbsolutePath",
                                                                    "()Ljava/lang/String;")
                                                 : nullptr;
                jstring path = methodPath
                                   ? static_cast<jstring>(env->CallObjectMethod(root, methodPath))
                                   : nullptr;
                if (path) {
                    const char* character = env->GetStringUTFChars(path, nullptr);
                    if (character) {
                        snprintf(out, size, "%s", character);
                        success = true;
                        env->ReleaseStringUTFChars(path, character);
                    }
                    env->DeleteLocalRef(path);
                }
                env->DeleteLocalRef(root);
            }
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (release)
        findVmJava()->DetachCurrentThread();
    return success;
}

bool packetApplication(char* out, size_t size) {
    int fd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char buffer[192];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (n <= 0)
        return false;
    buffer[n] = '\0';

    // Proses sekunder menambahkan ":name" pada nama paket, jadi hanya bagian
    // sebelum titik dua yang termasuk direktori aplikasi.
    char* pointTwo = strchr(buffer, ':');
    if (pointTwo)
        *pointTwo = '\0';
    size_t len = strlen(buffer);
    if (len == 0 || len >= size)
        return false;
    memcpy(out, buffer, len + 1);
    return true;
}
