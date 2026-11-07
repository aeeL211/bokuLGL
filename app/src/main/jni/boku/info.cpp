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
    EnvScope scope;
    if (!scope)
        return false;
    JNIEnv* env = scope.env();

    bool success = false;
    code = 0;

    LocalRef<jobject> activityObject;
    if (klassUnityPlayer()) {
        jfieldID fieldActivity =
            env->GetStaticFieldID(klassUnityPlayer(), "currentActivity", "Landroid/app/Activity;");
        if (fieldActivity)
            activityObject = LocalRef<jobject>(
                env, env->GetStaticObjectField(klassUnityPlayer(), fieldActivity));
        else
            clearException(env, "UnityPlayer.currentActivity");
    }

    const ActivityClass* activity = cacheActivity(env);
    LocalRef<jstring> appPackage;
    LocalRef<jobject> manager;
    if (activityObject && activity && activity->packageName && activity->managerPackage) {
        appPackage = LocalRef<jstring>(env, static_cast<jstring>(env->CallObjectMethod(
                                                activityObject.get(), activity->packageName)));
        manager = LocalRef<jobject>(
            env, env->CallObjectMethod(activityObject.get(), activity->managerPackage));
    }

    const PackageManagerClass* infoManager = cacheManagerPackage(env);
    LocalRef<jobject> infoObject;
    if (manager && appPackage && infoManager && infoManager->infoPackage) {
        infoObject =
            LocalRef<jobject>(env, env->CallObjectMethod(manager.get(), infoManager->infoPackage,
                                                         appPackage.get(), 0));
    }

    const PackageInfoClass* packageInfo = cacheInfoPackage(env);
    LocalRef<jstring> version;
    if (infoObject && packageInfo) {
        if (packageInfo->versionName)
            version = LocalRef<jstring>(env, static_cast<jstring>(env->GetObjectField(
                                                 infoObject.get(), packageInfo->versionName)));
        if (packageInfo->versionCode)
            code = env->GetIntField(infoObject.get(), packageInfo->versionCode);
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
        LOGI("Boku Boku version unreadable");
    LOGI("Mod by Febrian/Ian (code aeeL), build %s", BOKU_BUILD_COMMIT);
}
