#include "Jni.hpp"

#include <pthread.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "boku/jni_cache.h"
#include "boku/jni_util.h"

void Dialog(JNIEnv *env, jobject context, const char *title, const char *message, const char *openBtn,
            const char *closeBtn, int sec, const char *url) {
    const DialogClass *dialog = cacheDialog(env);
    if (!dialog || !dialog->show || !context)
        return;

    LocalRef<jstring> jTitle(env, env->NewStringUTF(title));
    LocalRef<jstring> jMessage(env, env->NewStringUTF(message));
    LocalRef<jstring> jOpen(env, env->NewStringUTF(openBtn));
    LocalRef<jstring> jClose(env, env->NewStringUTF(closeBtn));
    LocalRef<jstring> jUrl(env, env->NewStringUTF(url));
    if (!jTitle || !jMessage || !jOpen || !jClose || !jUrl) {
        clearException(env, "DialogHelper string");
        return;
    }

    env->CallStaticVoidMethod(dialog->klass, dialog->show, context, jTitle.get(), jMessage.get(),
                              jOpen.get(), jClose.get(), static_cast<jint>(sec), jUrl.get());
    clearException(env, "DialogHelper.showDialogWithLink");
}

void Toast(JNIEnv *env, jobject thiz, const char *text, int length) {
    const ToastClass *toast = cacheToast(env);
    if (!toast || !toast->create || !toast->show || !thiz)
        return;

    LocalRef<jstring> message(env, env->NewStringUTF(text));
    if (!message) {
        clearException(env, "Toast.makeText string");
        return;
    }
    LocalRef<jobject> objekToast(env, env->CallStaticObjectMethod(toast->klass, toast->create, thiz,
                                                                  message.get(),
                                                                  static_cast<jint>(length)));
    if (!objekToast) {
        clearException(env, "Toast.makeText");
        return;
    }
    env->CallVoidMethod(objekToast.get(), toast->show);
    clearException(env, "Toast.show");
}

// Big letter cause crash
void setText(JNIEnv *env, jobject obj, const char *text) {
    const ClassMethod *html = cacheHtml(env);
    const ClassMethod *textView = cacheTextView(env);
    if (!obj || !html || !html->method || !textView || !textView->method)
        return;

    LocalRef<jstring> jstr(env, env->NewStringUTF(text));
    if (!jstr) {
        clearException(env, "Html.fromHtml string");
        return;
    }
    LocalRef<jobject> spanned(env,
                              env->CallStaticObjectMethod(html->klass, html->method, jstr.get()));
    if (!spanned) {
        clearException(env, "Html.fromHtml");
        return;
    }
    env->CallVoidMethod(obj, textView->method, spanned.get());
    clearException(env, "TextView.setText");
}

void startService(JNIEnv *env, jobject ctx) {
    const ContextClass *context = cacheContext(env);
    const IntentClass *intent = cacheIntent(env);
    jclass launcher = cacheLauncher(env);
    if (!ctx || !context || !context->startService || !intent || !intent->fromContext || !launcher)
        return;

    LocalRef<jobject> objekIntent(env, env->NewObject(intent->klass, intent->fromContext, ctx,
                                                      launcher));
    if (!objekIntent) {
        clearException(env, "Intent Launcher");
        return;
    }
    LocalRef<jobject> component(
            env, env->CallObjectMethod(ctx, context->startService, objekIntent.get()));
    if (!component)
        clearException(env, "Context.startService");
}

void *exit_thread(void *) {
    sleep(5);
    exit(0);
}

int get_api_sdk(JNIEnv *env) {
    const BuildClass *build = cacheBuild(env);
    if (!build || !build->sdk)
        return 0;
    return env->GetStaticIntField(build->klass, build->sdk);
}

void startActivityPermisson(JNIEnv *env, jobject ctx) {
    const ContextClass *context = cacheContext(env);
    const ClassMethod *uri = cacheUri(env);
    const IntentClass *intent = cacheIntent(env);
    if (!ctx || !context || !context->packetName || !context->startActivity || !uri ||
        !uri->method || !intent || !intent->fromUri)
        return;

    LocalRef<jstring> packetName(
            env, static_cast<jstring>(env->CallObjectMethod(ctx, context->packetName)));
    if (!packetName) {
        clearException(env, "Context.getPackageName");
        return;
    }
    StringUtf packet(env, packetName.get());
    if (!packet) {
        clearException(env, "getPackageName UTF-8");
        return;
    }

    const char *awalan = OBFUSCATE("package:");
    char addressPacket[512];
    snprintf(addressPacket, sizeof(addressPacket), "%s%s", awalan, packet.get());

    LocalRef<jstring> address(env, env->NewStringUTF(addressPacket));
    if (!address) {
        clearException(env, "Uri string");
        return;
    }
    LocalRef<jobject> uriObjek(
            env, env->CallStaticObjectMethod(uri->klass, uri->method, address.get()));
    if (!uriObjek) {
        clearException(env, "Uri.parse");
        return;
    }

    LocalRef<jstring> action(
            env, env->NewStringUTF(OBFUSCATE("android.settings.action.MANAGE_OVERLAY_PERMISSION")));
    if (!action) {
        clearException(env, "Intent action");
        return;
    }
    LocalRef<jobject> objekIntent(env, env->NewObject(intent->klass, intent->fromUri, action.get(),
                                                      uriObjek.get()));
    if (!objekIntent) {
        clearException(env, "Intent permission");
        return;
    }
    env->CallVoidMethod(ctx, context->startActivity, objekIntent.get());
    clearException(env, "Context.startActivity");
}

// Needed jclass parameter because this is a static java method
void CheckOverlayPermission(JNIEnv *env, jclass thiz, jobject ctx) {
    (void)thiz;
    // If overlay permission option is greyed out, make sure to add
    // android.permission.SYSTEM_ALERT_WINDOW in manifest

    LOGI(OBFUSCATE("Check overlay permission"));

    int sdkVer = get_api_sdk(env);
    if (sdkVer >= 23) { // Android 6.0
        const ClassMethod *settings = cacheSettings(env);
        bool allowed = true;
        if (settings && settings->method)
            allowed = env->CallStaticBooleanMethod(settings->klass, settings->method, ctx) == JNI_TRUE;
        clearException(env, "Settings.canDrawOverlays");
        if (!allowed) {
            Toast(env, ctx, OBFUSCATE("Overlay permission is required in order to show mod menu."),
                  1);
            Toast(env, ctx, OBFUSCATE("Overlay permission is required in order to show mod menu."),
                  1);
            startActivityPermisson(env, ctx);

            pthread_t ptid;
            pthread_create(&ptid, nullptr, exit_thread, nullptr);
            return;
        }
    }

    LOGI(OBFUSCATE("Start service"));
    startService(env, ctx);
}
