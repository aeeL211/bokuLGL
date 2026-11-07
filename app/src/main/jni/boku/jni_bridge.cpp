#include "jni_bridge.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>

namespace {

std::atomic<JavaVM*> vmTersimpan{nullptr};
std::atomic<jclass> kelasUnity{nullptr};

} // namespace

JavaVM* vmJava() { return vmTersimpan.load(std::memory_order_acquire); }

void setelVmJava(JavaVM* vm) { vmTersimpan.store(vm, std::memory_order_release); }

jclass kelasUnityPlayer() { return kelasUnity.load(std::memory_order_acquire); }

void simpanKelasUnityPlayer(JNIEnv* env) {
    if (kelasUnity.load(std::memory_order_acquire))
        return;
    jclass kelas = env->FindClass("com.unity3d.player.UnityPlayer");
    if (kelas) {
        auto global = static_cast<jclass>(env->NewGlobalRef(kelas));
        env->DeleteLocalRef(kelas);
        if (global) {
            jclass kosong = nullptr;
            // Dua thread bisa menyimpan kelas yang sama; yang kalah menghapus
            // referensi globalnya sendiri supaya tidak ada yang bocor.
            if (!kelasUnity.compare_exchange_strong(kosong, global))
                env->DeleteGlobalRef(global);
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

JavaVM* cariVmJava() {
    JavaVM* tersimpan = vmTersimpan.load(std::memory_order_acquire);
    if (tersimpan)
        return tersimpan;
    typedef jint (*GetVms)(JavaVM**, jsize, jsize*);
    auto ambilVm = reinterpret_cast<GetVms>(dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
    if (!ambilVm)
        return nullptr;
    JavaVM* vm = nullptr;
    jsize jumlah = 0;
    if (ambilVm(&vm, 1, &jumlah) != JNI_OK || jumlah < 1)
        return nullptr;
    vmTersimpan.store(vm, std::memory_order_release);
    return vm;
}

bool lekatJni(JNIEnv** env, bool* lepas) {
    JavaVM* vm = cariVmJava();
    if (!vm)
        return false;
    *lepas = false;
    if (vm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6) == JNI_OK)
        return true;
    if (vm->AttachCurrentThread(env, nullptr) != JNI_OK)
        return false;
    *lepas = true;
    return true;
}

bool akarPenyimpananEksternal(char* out, size_t size) {
    JNIEnv* env = nullptr;
    bool lepas = false;
    if (!lekatJni(&env, &lepas))
        return false;

    bool berhasil = false;
    jclass kelasEnv = env->FindClass("android/os/Environment");
    if (kelasEnv) {
        jmethodID metodeAkar =
            env->GetStaticMethodID(kelasEnv, "getExternalStorageDirectory", "()Ljava/io/File;");
        if (metodeAkar) {
            jobject akar = env->CallStaticObjectMethod(kelasEnv, metodeAkar);
            if (akar) {
                jclass kelasFile = env->FindClass("java/io/File");
                jmethodID metodePath = kelasFile ? env->GetMethodID(kelasFile, "getAbsolutePath",
                                                                    "()Ljava/lang/String;")
                                                 : nullptr;
                jstring path = metodePath
                                   ? static_cast<jstring>(env->CallObjectMethod(akar, metodePath))
                                   : nullptr;
                if (path) {
                    const char* karakter = env->GetStringUTFChars(path, nullptr);
                    if (karakter) {
                        snprintf(out, size, "%s", karakter);
                        berhasil = true;
                        env->ReleaseStringUTFChars(path, karakter);
                    }
                    env->DeleteLocalRef(path);
                }
                env->DeleteLocalRef(akar);
            }
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (lepas)
        cariVmJava()->DetachCurrentThread();
    return berhasil;
}

bool paketAplikasi(char* out, size_t size) {
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
    char* titikDua = strchr(buffer, ':');
    if (titikDua)
        *titikDua = '\0';
    size_t len = strlen(buffer);
    if (len == 0 || len >= size)
        return false;
    memcpy(out, buffer, len + 1);
    return true;
}
