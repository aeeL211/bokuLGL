#include "jni_cache.h"

#include <atomic>
#include <mutex>

#include "Includes/Logger.h"
#include "Includes/obfuscate.h"

namespace {

std::mutex cacheKey;

jclass getClass(JNIEnv* env, const char* name) {
    jclass local = env->FindClass(name);
    if (!local) {
        env->ExceptionClear();
        return nullptr;
    }
    auto global = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    if (!global)
        env->ExceptionClear();
    return global;
}

jmethodID getStaticMethod(JNIEnv* env, jclass klass, const char* name, const char* tanda) {
    if (!klass)
        return nullptr;
    jmethodID method = env->GetStaticMethodID(klass, name, tanda);
    if (!method)
        env->ExceptionClear();
    return method;
}

jmethodID getMethod(JNIEnv* env, jclass klass, const char* name, const char* tanda) {
    if (!klass)
        return nullptr;
    jmethodID method = env->GetMethodID(klass, name, tanda);
    if (!method)
        env->ExceptionClear();
    return method;
}

jfieldID getStaticField(JNIEnv* env, jclass klass, const char* name, const char* tanda) {
    if (!klass)
        return nullptr;
    jfieldID field = env->GetStaticFieldID(klass, name, tanda);
    if (!field)
        env->ExceptionClear();
    return field;
}

jfieldID getField(JNIEnv* env, jclass klass, const char* name, const char* tanda) {
    if (!klass)
        return nullptr;
    jfieldID field = env->GetFieldID(klass, name, tanda);
    if (!field)
        env->ExceptionClear();
    return field;
}

DialogClass dialog = {};
std::atomic<bool> dialogReady{false};

ToastClass toast = {};
std::atomic<bool> toastReady{false};

ClassMethod html = {};
std::atomic<bool> htmlReady{false};

ClassMethod textView = {};
std::atomic<bool> textViewReady{false};

IntentClass intent = {};
std::atomic<bool> intentReady{false};

jclass launcher = nullptr;
std::atomic<bool> launcherReady{false};

ClassMethod uri = {};
std::atomic<bool> uriReady{false};

BuildClass build = {};
std::atomic<bool> buildReady{false};

ClassMethod settings = {};
std::atomic<bool> settingsReady{false};

ContextClass context = {};
std::atomic<bool> contextReady{false};

ActivityClass activity = {};
std::atomic<bool> activityReady{false};

PacketManagerClass managerPacket = {};
std::atomic<bool> managerPacketReady{false};

PacketInfoClass infoPacket = {};
std::atomic<bool> infoPacketReady{false};

jclass klassStringGlobal = nullptr;
std::atomic<bool> stringReady{false};

} // namespace

jclass klassString(JNIEnv* env) {
    if (stringReady.load(std::memory_order_acquire))
        return klassStringGlobal;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!stringReady.load(std::memory_order_relaxed)) {
        klassStringGlobal = getClass(env, OBFUSCATE("java/lang/String"));
        stringReady.store(true, std::memory_order_release);
    }
    return klassStringGlobal;
}

const DialogClass* cacheDialog(JNIEnv* env) {
    if (dialogReady.load(std::memory_order_acquire))
        return dialog.klass ? &dialog : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!dialogReady.load(std::memory_order_relaxed)) {
        dialog.klass = getClass(env, OBFUSCATE("com/android/support/DialogHelper"));
        dialog.show = getStaticMethod(
            env, dialog.klass, OBFUSCATE("showDialogWithLink"),
            OBFUSCATE("(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;"
                      "Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V"));
        dialogReady.store(true, std::memory_order_release);
    }
    return dialog.klass ? &dialog : nullptr;
}

const ToastClass* cacheToast(JNIEnv* env) {
    if (toastReady.load(std::memory_order_acquire))
        return toast.klass ? &toast : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!toastReady.load(std::memory_order_relaxed)) {
        toast.klass = getClass(env, OBFUSCATE("android/widget/Toast"));
        toast.create =
            getStaticMethod(env, toast.klass, OBFUSCATE("makeText"),
                              OBFUSCATE("(Landroid/content/Context;Ljava/lang/CharSequence;I)"
                                        "Landroid/widget/Toast;"));
        toast.show = getMethod(env, toast.klass, OBFUSCATE("show"), OBFUSCATE("()V"));
        toastReady.store(true, std::memory_order_release);
    }
    return toast.klass ? &toast : nullptr;
}

const ClassMethod* cacheHtml(JNIEnv* env) {
    if (htmlReady.load(std::memory_order_acquire))
        return html.klass ? &html : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!htmlReady.load(std::memory_order_relaxed)) {
        html.klass = getClass(env, OBFUSCATE("android/text/Html"));
        html.method = getStaticMethod(env, html.klass, OBFUSCATE("fromHtml"),
                                        OBFUSCATE("(Ljava/lang/String;)Landroid/text/Spanned;"));
        htmlReady.store(true, std::memory_order_release);
    }
    return html.klass ? &html : nullptr;
}

const ClassMethod* cacheTextView(JNIEnv* env) {
    if (textViewReady.load(std::memory_order_acquire))
        return textView.klass ? &textView : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!textViewReady.load(std::memory_order_relaxed)) {
        textView.klass = getClass(env, OBFUSCATE("android/widget/TextView"));
        textView.method = getMethod(env, textView.klass, OBFUSCATE("setText"),
                                      OBFUSCATE("(Ljava/lang/CharSequence;)V"));
        textViewReady.store(true, std::memory_order_release);
    }
    return textView.klass ? &textView : nullptr;
}

const IntentClass* cacheIntent(JNIEnv* env) {
    if (intentReady.load(std::memory_order_acquire))
        return intent.klass ? &intent : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!intentReady.load(std::memory_order_relaxed)) {
        intent.klass = getClass(env, OBFUSCATE("android/content/Intent"));
        intent.fromContext =
            getMethod(env, intent.klass, OBFUSCATE("<init>"),
                        OBFUSCATE("(Landroid/content/Context;Ljava/lang/Class;)V"));
        intent.fromUri = getMethod(env, intent.klass, OBFUSCATE("<init>"),
                                     OBFUSCATE("(Ljava/lang/String;Landroid/net/Uri;)V"));
        intentReady.store(true, std::memory_order_release);
    }
    return intent.klass ? &intent : nullptr;
}

jclass cacheLauncher(JNIEnv* env) {
    if (launcherReady.load(std::memory_order_acquire))
        return launcher;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!launcherReady.load(std::memory_order_relaxed)) {
        launcher = getClass(env, OBFUSCATE("com/android/support/Launcher"));
        launcherReady.store(true, std::memory_order_release);
    }
    return launcher;
}

const ClassMethod* cacheUri(JNIEnv* env) {
    if (uriReady.load(std::memory_order_acquire))
        return uri.klass ? &uri : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!uriReady.load(std::memory_order_relaxed)) {
        uri.klass = getClass(env, OBFUSCATE("android/net/Uri"));
        uri.method = getStaticMethod(env, uri.klass, OBFUSCATE("parse"),
                                       OBFUSCATE("(Ljava/lang/String;)Landroid/net/Uri;"));
        uriReady.store(true, std::memory_order_release);
    }
    return uri.klass ? &uri : nullptr;
}

const BuildClass* cacheBuild(JNIEnv* env) {
    if (buildReady.load(std::memory_order_acquire))
        return build.klass ? &build : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!buildReady.load(std::memory_order_relaxed)) {
        build.klass = getClass(env, OBFUSCATE("android/os/Build$VERSION"));
        build.sdk = getStaticField(env, build.klass, OBFUSCATE("SDK_INT"), OBFUSCATE("I"));
        buildReady.store(true, std::memory_order_release);
    }
    return build.klass ? &build : nullptr;
}

const ClassMethod* cacheSettings(JNIEnv* env) {
    if (settingsReady.load(std::memory_order_acquire))
        return settings.klass ? &settings : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!settingsReady.load(std::memory_order_relaxed)) {
        settings.klass = getClass(env, OBFUSCATE("android/provider/Settings"));
        settings.method = getStaticMethod(env, settings.klass, OBFUSCATE("canDrawOverlays"),
                                            OBFUSCATE("(Landroid/content/Context;)Z"));
        settingsReady.store(true, std::memory_order_release);
    }
    return settings.klass ? &settings : nullptr;
}

const ContextClass* cacheContext(JNIEnv* env) {
    if (contextReady.load(std::memory_order_acquire))
        return context.klass ? &context : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!contextReady.load(std::memory_order_relaxed)) {
        context.klass = getClass(env, OBFUSCATE("android/content/Context"));
        context.startService =
            getMethod(env, context.klass, OBFUSCATE("startService"),
                        OBFUSCATE("(Landroid/content/Intent;)Landroid/content/ComponentName;"));
        context.startActivity = getMethod(env, context.klass, OBFUSCATE("startActivity"),
                                             OBFUSCATE("(Landroid/content/Intent;)V"));
        context.packetName = getMethod(env, context.klass, OBFUSCATE("getPackageName"),
                                        OBFUSCATE("()Ljava/lang/String;"));
        contextReady.store(true, std::memory_order_release);
    }
    return context.klass ? &context : nullptr;
}

const ActivityClass* cacheActivity(JNIEnv* env) {
    if (activityReady.load(std::memory_order_acquire))
        return activity.klass ? &activity : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!activityReady.load(std::memory_order_relaxed)) {
        activity.klass = getClass(env, OBFUSCATE("android/app/Activity"));
        activity.managerPacket = getMethod(env, activity.klass, OBFUSCATE("getPackageManager"),
                                             OBFUSCATE("()Landroid/content/pm/PackageManager;"));
        activity.packetName = getMethod(env, activity.klass, OBFUSCATE("getPackageName"),
                                          OBFUSCATE("()Ljava/lang/String;"));
        activityReady.store(true, std::memory_order_release);
    }
    return activity.klass ? &activity : nullptr;
}

const PacketManagerClass* cacheManagerPacket(JNIEnv* env) {
    if (managerPacketReady.load(std::memory_order_acquire))
        return managerPacket.klass ? &managerPacket : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!managerPacketReady.load(std::memory_order_relaxed)) {
        managerPacket.klass = getClass(env, OBFUSCATE("android/content/pm/PackageManager"));
        managerPacket.infoPacket =
            getMethod(env, managerPacket.klass, OBFUSCATE("getPackageInfo"),
                        OBFUSCATE("(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"));
        managerPacketReady.store(true, std::memory_order_release);
    }
    return managerPacket.klass ? &managerPacket : nullptr;
}

const PacketInfoClass* cacheInfoPacket(JNIEnv* env) {
    if (infoPacketReady.load(std::memory_order_acquire))
        return infoPacket.klass ? &infoPacket : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> hold(cacheKey);
    if (!infoPacketReady.load(std::memory_order_relaxed)) {
        infoPacket.klass = getClass(env, OBFUSCATE("android/content/pm/PackageInfo"));
        infoPacket.versionName = getField(env, infoPacket.klass, OBFUSCATE("versionName"),
                                         OBFUSCATE("Ljava/lang/String;"));
        infoPacket.codeVersion =
            getField(env, infoPacket.klass, OBFUSCATE("versionCode"), OBFUSCATE("I"));
        infoPacketReady.store(true, std::memory_order_release);
    }
    return infoPacket.klass ? &infoPacket : nullptr;
}

void prepareJniCache(JNIEnv* env) {
    if (!env)
        return;
    klassString(env);
    cacheDialog(env);
    cacheToast(env);
    cacheHtml(env);
    cacheTextView(env);
    cacheIntent(env);
    cacheLauncher(env);
    cacheUri(env);
    cacheBuild(env);
    cacheSettings(env);
    cacheContext(env);
    cacheActivity(env);
    cacheManagerPacket(env);
    cacheInfoPacket(env);
}
