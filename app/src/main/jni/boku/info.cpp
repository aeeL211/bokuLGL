#include "info.h"

#include <cstddef>
#include <cstdio>

#include "jni_bridge.h"
#include "jni_cache.h"
#include "jni_util.h"
#include "log.h"

#ifndef BOKU_BUILD_COMMIT
#define BOKU_BUILD_COMMIT "dev"
#endif

namespace {

bool versionApplication(char* name, size_t size, jint& code) {
    EnvScope lingkup;
    if (!lingkup)
        return false;
    JNIEnv* env = lingkup.env();

    bool success = false;
    code = 0;

    LocalRef<jobject> activity;
    if (klassUnityPlayer()) {
        jfieldID fieldActivity =
            env->GetStaticFieldID(klassUnityPlayer(), "currentActivity", "Landroid/app/Activity;");
        if (fieldActivity)
            activity = LocalRef<jobject>(
                env, env->GetStaticObjectField(klassUnityPlayer(), fieldActivity));
        else
            clearException(env, "UnityPlayer.currentActivity");
    }

    const ActivityClass* activity = cacheActivity(env);
    LocalRef<jstring> packet;
    LocalRef<jobject> manager;
    if (activity && activity && activity->packetName && activity->managerPacket) {
        packet = LocalRef<jstring>(
            env, static_cast<jstring>(env->CallObjectMethod(activity.get(), activity->packetName)));
        manager =
            LocalRef<jobject>(env, env->CallObjectMethod(activity.get(), activity->managerPacket));
    }

    const PacketManagerClass* infoManager = cacheManagerPacket(env);
    LocalRef<jobject> informasi;
    if (manager && packet && infoManager && infoManager->infoPacket) {
        informasi = LocalRef<jobject>(
            env, env->CallObjectMethod(manager.get(), infoManager->infoPacket, packet.get(), 0));
    }

    const PacketInfoClass* info = cacheInfoPacket(env);
    LocalRef<jstring> version;
    if (informasi && info) {
        if (info->versionName)
            version = LocalRef<jstring>(
                env, static_cast<jstring>(env->GetObjectField(informasi.get(), info->versionName)));
        if (info->codeVersion)
            code = env->GetIntField(informasi.get(), info->codeVersion);
        if (version) {
            StringUtf character(env, version.get());
            if (character) {
                snprintf(name, size, "%s", character.get());
                success = true;
            }
        }
    }

    clearException(env, "versi aplikasi");
    return success;
}

} // namespace

void recordEnvironment() {
    char version[64] = {};
    jint code = 0;
    if (versionApplication(version, sizeof(version), code))
        LOGI("Boku Boku %s (code %d)", version, code);
    else
        LOGI("Versi Boku Boku tidak terbaca");
    LOGI("Mod by Febrian/Ian (code aeeL), build %s", BOKU_BUILD_COMMIT);
}
