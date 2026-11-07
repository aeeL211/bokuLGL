#include "Includes/obfuscate.h"
#include "Menu/Menu.hpp"
#include "Utils.hpp"
#include "boku/jni_cache.h"
#include "boku/jni_util.h"
#include "boku/log.h"
#include "boku/mod.h"

namespace {

int registerNative(JNIEnv *env, const char *name, JNINativeMethod *methods, int count) {
    LocalRef<jclass> clazz(env, env->FindClass(name));
    if (!clazz) {
        clearException(env, "FindClass menu");
        return JNI_ERR;
    }
    if (env->RegisterNatives(clazz.get(), methods, count) != 0) {
        clearException(env, "RegisterNatives menu");
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
            {OBFUSCATE("PlayerList"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(PlayerList)},
            {OBFUSCATE("PlayerPosition"), OBFUSCATE("()Ljava/lang/String;"),
             reinterpret_cast<void *>(PlayerPosition)},
            {OBFUSCATE("KickPlayer"),
             OBFUSCATE("(Ljava/lang/String;Ljava/lang/String;Z)Z"),
             reinterpret_cast<void *>(KickPlayer)},
            {OBFUSCATE("KickHost"), OBFUSCATE("(Ljava/lang/String;)Z"),
             reinterpret_cast<void *>(KickHost)},
            {OBFUSCATE("KickAll"), OBFUSCATE("()Z"),
             reinterpret_cast<void *>(KickAll)},
            {OBFUSCATE("CopyPlayerSkin"), OBFUSCATE("(Ljava/lang/String;Ljava/lang/String;)Z"),
             reinterpret_cast<void *>(CopyPlayerSkin)},
            {OBFUSCATE("CopyPlayerAvatar"), OBFUSCATE("(Ljava/lang/String;)Z"),
             reinterpret_cast<void *>(CopyPlayerAvatar)},
    };

    return registerNative(env, OBFUSCATE("com/android/support/Menu"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

int RegisterPreferences(JNIEnv *env) {
    JNINativeMethod methods[] = {
            {OBFUSCATE("Changes"),
             OBFUSCATE("(Landroid/content/Context;ILjava/lang/String;IJZLjava/lang/String;)V"),
             reinterpret_cast<void *>(Changes)},
    };
    return registerNative(env, OBFUSCATE("com/android/support/Preferences"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

int RegisterMain(JNIEnv *env) {
    JNINativeMethod methods[] = {
            {OBFUSCATE("CheckOverlayPermission"), OBFUSCATE("(Landroid/content/Context;)V"),
             reinterpret_cast<void *>(CheckOverlayPermission)},
    };
    return registerNative(env, OBFUSCATE("com/android/support/Main"), methods,
                     static_cast<int>(sizeof(methods) / sizeof(methods[0])));
}

extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)reserved;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;
    // The only JNI_OnLoad in this library: it belongs to the menu, which also
    // hands the JavaVM and activity class to the mod side.
    setupMod(vm, env);

    // The menu classes exist only when this library is loaded from an APK merged
    // with the template. If the library is injected into the stock game they are
    // not registered; the hooks must still run with their default switches, so
    // a failed registration is logged and does not abort loading.
    bool menuRegistered = true;
    if (RegisterMenu(env) != 0)
        menuRegistered = false;
    if (RegisterPreferences(env) != 0)
        menuRegistered = false;
    if (RegisterMain(env) != 0)
        menuRegistered = false;
    // Helper classes are bound after registration. If the menu classes are
    // missing, each cache entry fails on its own and its caller uses the safe
    // path without JNI.
    prepareJniCache(env);
    if (!menuRegistered)
        LOGI("Menu class not registered, features run without a menu");
    return JNI_VERSION_1_6;
}
