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
    const KelasDialog *dialog = cacheDialog(env);
    if (!dialog || !dialog->tampil || !context)
        return;

    LocalRef<jstring> jTitle(env, env->NewStringUTF(title));
    LocalRef<jstring> jMessage(env, env->NewStringUTF(message));
    LocalRef<jstring> jOpen(env, env->NewStringUTF(openBtn));
    LocalRef<jstring> jClose(env, env->NewStringUTF(closeBtn));
    LocalRef<jstring> jUrl(env, env->NewStringUTF(url));
    if (!jTitle || !jMessage || !jOpen || !jClose || !jUrl) {
        bersihkanEksepsi(env, "DialogHelper string");
        return;
    }

    env->CallStaticVoidMethod(dialog->kelas, dialog->tampil, context, jTitle.get(), jMessage.get(),
                              jOpen.get(), jClose.get(), static_cast<jint>(sec), jUrl.get());
    bersihkanEksepsi(env, "DialogHelper.showDialogWithLink");
}

void Toast(JNIEnv *env, jobject thiz, const char *text, int length) {
    const KelasToast *toast = cacheToast(env);
    if (!toast || !toast->buat || !toast->tampil || !thiz)
        return;

    LocalRef<jstring> pesan(env, env->NewStringUTF(text));
    if (!pesan) {
        bersihkanEksepsi(env, "Toast.makeText string");
        return;
    }
    LocalRef<jobject> objekToast(env, env->CallStaticObjectMethod(toast->kelas, toast->buat, thiz,
                                                                  pesan.get(),
                                                                  static_cast<jint>(length)));
    if (!objekToast) {
        bersihkanEksepsi(env, "Toast.makeText");
        return;
    }
    env->CallVoidMethod(objekToast.get(), toast->tampil);
    bersihkanEksepsi(env, "Toast.show");
}

// Big letter cause crash
void setText(JNIEnv *env, jobject obj, const char *text) {
    const KelasMetode *html = cacheHtml(env);
    const KelasMetode *textView = cacheTextView(env);
    if (!obj || !html || !html->metode || !textView || !textView->metode)
        return;

    LocalRef<jstring> jstr(env, env->NewStringUTF(text));
    if (!jstr) {
        bersihkanEksepsi(env, "Html.fromHtml string");
        return;
    }
    LocalRef<jobject> spanned(env,
                              env->CallStaticObjectMethod(html->kelas, html->metode, jstr.get()));
    if (!spanned) {
        bersihkanEksepsi(env, "Html.fromHtml");
        return;
    }
    env->CallVoidMethod(obj, textView->metode, spanned.get());
    bersihkanEksepsi(env, "TextView.setText");
}

void startService(JNIEnv *env, jobject ctx) {
    const KelasKonteks *konteks = cacheKonteks(env);
    const KelasIntent *intent = cacheIntent(env);
    jclass launcher = cacheLauncher(env);
    if (!ctx || !konteks || !konteks->mulaiLayanan || !intent || !intent->dariKonteks || !launcher)
        return;

    LocalRef<jobject> objekIntent(env, env->NewObject(intent->kelas, intent->dariKonteks, ctx,
                                                      launcher));
    if (!objekIntent) {
        bersihkanEksepsi(env, "Intent Launcher");
        return;
    }
    LocalRef<jobject> komponen(
            env, env->CallObjectMethod(ctx, konteks->mulaiLayanan, objekIntent.get()));
    if (!komponen)
        bersihkanEksepsi(env, "Context.startService");
}

void *exit_thread(void *) {
    sleep(5);
    exit(0);
}

int get_api_sdk(JNIEnv *env) {
    const KelasBuild *build = cacheBuild(env);
    if (!build || !build->sdk)
        return 0;
    return env->GetStaticIntField(build->kelas, build->sdk);
}

void startActivityPermisson(JNIEnv *env, jobject ctx) {
    const KelasKonteks *konteks = cacheKonteks(env);
    const KelasMetode *uri = cacheUri(env);
    const KelasIntent *intent = cacheIntent(env);
    if (!ctx || !konteks || !konteks->namaPaket || !konteks->mulaiAktivitas || !uri ||
        !uri->metode || !intent || !intent->dariUri)
        return;

    LocalRef<jstring> namaPaket(
            env, static_cast<jstring>(env->CallObjectMethod(ctx, konteks->namaPaket)));
    if (!namaPaket) {
        bersihkanEksepsi(env, "Context.getPackageName");
        return;
    }
    StringUtf paket(env, namaPaket.get());
    if (!paket) {
        bersihkanEksepsi(env, "getPackageName UTF-8");
        return;
    }

    const char *awalan = OBFUSCATE("package:");
    char alamatPaket[512];
    snprintf(alamatPaket, sizeof(alamatPaket), "%s%s", awalan, paket.get());

    LocalRef<jstring> alamat(env, env->NewStringUTF(alamatPaket));
    if (!alamat) {
        bersihkanEksepsi(env, "Uri string");
        return;
    }
    LocalRef<jobject> uriObjek(
            env, env->CallStaticObjectMethod(uri->kelas, uri->metode, alamat.get()));
    if (!uriObjek) {
        bersihkanEksepsi(env, "Uri.parse");
        return;
    }

    LocalRef<jstring> aksi(
            env, env->NewStringUTF(OBFUSCATE("android.settings.action.MANAGE_OVERLAY_PERMISSION")));
    if (!aksi) {
        bersihkanEksepsi(env, "Intent action");
        return;
    }
    LocalRef<jobject> objekIntent(env, env->NewObject(intent->kelas, intent->dariUri, aksi.get(),
                                                      uriObjek.get()));
    if (!objekIntent) {
        bersihkanEksepsi(env, "Intent permission");
        return;
    }
    env->CallVoidMethod(ctx, konteks->mulaiAktivitas, objekIntent.get());
    bersihkanEksepsi(env, "Context.startActivity");
}

// Needed jclass parameter because this is a static java method
void CheckOverlayPermission(JNIEnv *env, jclass thiz, jobject ctx) {
    (void)thiz;
    // If overlay permission option is greyed out, make sure to add
    // android.permission.SYSTEM_ALERT_WINDOW in manifest

    LOGI(OBFUSCATE("Check overlay permission"));

    int sdkVer = get_api_sdk(env);
    if (sdkVer >= 23) { // Android 6.0
        const KelasMetode *settings = cacheSettings(env);
        bool boleh = true;
        if (settings && settings->metode)
            boleh = env->CallStaticBooleanMethod(settings->kelas, settings->metode, ctx) == JNI_TRUE;
        bersihkanEksepsi(env, "Settings.canDrawOverlays");
        if (!boleh) {
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
