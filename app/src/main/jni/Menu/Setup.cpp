#include "Includes/obfuscate.h"
#include "Menu/Menu.hpp"
#include "Utils.hpp"
#include "boku/jni_cache.h"
#include "boku/jni_util.h"
#include "boku/log.h"
#include "boku/mod.h"

namespace {

int daftarkan(JNIEnv *env, const char *nama, JNINativeMethod *methods, int jumlah) {
    LocalRef<jclass> clazz(env, env->FindClass(nama));
    if (!clazz) {
        bersihkanEksepsi(env, "FindClass menu");
        return JNI_ERR;
    }
    if (env->RegisterNatives(clazz.get(), methods, jumlah) != 0) {
        bersihkanEksepsi(env, "RegisterNatives menu");
        return JNI_ERR;
    }
    return JNI_OK;
}

} // namespace

int RegisterMenu(JNIEnv *env) {
    JNINativeMethod methods[] = {
            {OBFUSCATE("Icon"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(Icon)},
            {OBFUSCATE("IconWebViewData"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(IconWebViewData)},
            {OBFUSCATE("IsGameLibLoaded"), OBFUSCATE("()Z"),
             reinterpret_cast<void *>(isGameLibLoaded)},
            {OBFUSCATE("Init"),
             OBFUSCATE("(Landroid/content/Context;Landroid/widget/TextView;"
                       "Landroid/widget/TextView;)V"),
             reinterpret_cast<void *>(Init)},
            {OBFUSCATE("SettingsList"), OBFUSCATE("()[Ljava/lang/String;"),
             reinterpret_cast<void *>(SettingsList)},
            {OBFUSCATE("GetFeatureList"), OBFUSCATE("()[Ljava/lang/String;"),
             reinterpret_cast<void *>(GetFeatureList)},
            {OBFUSCATE("DaftarPemain"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(DaftarPemain)},
            {OBFUSCATE("PosisiPemain"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(PosisiPemain)},
            {OBFUSCATE("TendangPemain"), OBFUSCATE("(Ljava/lang/String;Ljava/lang/String;)Z"),
             reinterpret_cast<void *>(TendangPemain)},
            {OBFUSCATE("SalinSkinPemain"), OBFUSCATE("(Ljava/lang/String;Ljava/lang/String;)Z"),
             reinterpret_cast<void *>(SalinSkinPemain)},
    };

    return daftarkan(env, OBFUSCATE("com/android/support/Menu"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

int RegisterPreferences(JNIEnv *env) {
    JNINativeMethod methods[] = {
            {OBFUSCATE("Changes"),
             OBFUSCATE("(Landroid/content/Context;ILjava/lang/String;IJZLjava/lang/String;)V"),
             reinterpret_cast<void *>(Changes)},
    };
    return daftarkan(env, OBFUSCATE("com/android/support/Preferences"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

int RegisterMain(JNIEnv *env) {
    JNINativeMethod methods[] = {
            {OBFUSCATE("CheckOverlayPermission"), OBFUSCATE("(Landroid/content/Context;)V"),
             reinterpret_cast<void *>(CheckOverlayPermission)},
    };
    return daftarkan(env, OBFUSCATE("com/android/support/Main"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;
    // Satu-satunya JNI_OnLoad di library ini: milik menu, yang juga menyerahkan
    // JavaVM dan kelas activity ke sisi mod.
    siapkanMod(vm, env);

    // Kelas menu hanya ada saat pustaka ini dimuat dari APK yang sudah digabung
    // dengan template. Bila pustaka disuntikkan ke game asli, kelas itu tidak
    // terdaftar; hook tetap harus berjalan dengan sakelar bawaannya, jadi
    // kegagalan pendaftaran dicatat dan tidak membatalkan pemuatan.
    bool menuTerdaftar = true;
    if (RegisterMenu(env) != 0)
        menuTerdaftar = false;
    if (RegisterPreferences(env) != 0)
        menuTerdaftar = false;
    if (RegisterMain(env) != 0)
        menuTerdaftar = false;
    // Kelas pembantu diikat setelah pendaftaran. Bila kelas menu tidak ada,
    // setiap entri cache gagal sendiri-sendiri dan pemanggilnya memakai jalur
    // aman tanpa JNI.
    siapkanCacheJni(env);
    if (!menuTerdaftar)
        LOGI("Kelas menu tidak terdaftar, fitur berjalan tanpa menu");
    return JNI_VERSION_1_6;
}
