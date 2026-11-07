#include "jni_cache.h"

#include <atomic>
#include <mutex>

#include "Includes/Logger.h"
#include "Includes/obfuscate.h"

namespace {

std::mutex kunciCache;

jclass ambilKelas(JNIEnv* env, const char* nama) {
    jclass lokal = env->FindClass(nama);
    if (!lokal) {
        env->ExceptionClear();
        return nullptr;
    }
    auto global = static_cast<jclass>(env->NewGlobalRef(lokal));
    env->DeleteLocalRef(lokal);
    if (!global)
        env->ExceptionClear();
    return global;
}

jmethodID ambilMetodeStatis(JNIEnv* env, jclass kelas, const char* nama, const char* tanda) {
    if (!kelas)
        return nullptr;
    jmethodID metode = env->GetStaticMethodID(kelas, nama, tanda);
    if (!metode)
        env->ExceptionClear();
    return metode;
}

jmethodID ambilMetode(JNIEnv* env, jclass kelas, const char* nama, const char* tanda) {
    if (!kelas)
        return nullptr;
    jmethodID metode = env->GetMethodID(kelas, nama, tanda);
    if (!metode)
        env->ExceptionClear();
    return metode;
}

jfieldID ambilFieldStatis(JNIEnv* env, jclass kelas, const char* nama, const char* tanda) {
    if (!kelas)
        return nullptr;
    jfieldID field = env->GetStaticFieldID(kelas, nama, tanda);
    if (!field)
        env->ExceptionClear();
    return field;
}

jfieldID ambilField(JNIEnv* env, jclass kelas, const char* nama, const char* tanda) {
    if (!kelas)
        return nullptr;
    jfieldID field = env->GetFieldID(kelas, nama, tanda);
    if (!field)
        env->ExceptionClear();
    return field;
}

KelasDialog dialog = {};
std::atomic<bool> dialogSiap{false};

KelasToast toast = {};
std::atomic<bool> toastSiap{false};

KelasMetode html = {};
std::atomic<bool> htmlSiap{false};

KelasMetode textView = {};
std::atomic<bool> textViewSiap{false};

KelasIntent intent = {};
std::atomic<bool> intentSiap{false};

jclass launcher = nullptr;
std::atomic<bool> launcherSiap{false};

KelasMetode uri = {};
std::atomic<bool> uriSiap{false};

KelasBuild build = {};
std::atomic<bool> buildSiap{false};

KelasMetode settings = {};
std::atomic<bool> settingsSiap{false};

KelasKonteks konteks = {};
std::atomic<bool> konteksSiap{false};

KelasAktivitas aktivitas = {};
std::atomic<bool> aktivitasSiap{false};

KelasManajerPaket manajerPaket = {};
std::atomic<bool> manajerPaketSiap{false};

KelasInfoPaket infoPaket = {};
std::atomic<bool> infoPaketSiap{false};

jclass kelasStringGlobal = nullptr;
std::atomic<bool> stringSiap{false};

} // namespace

jclass kelasString(JNIEnv* env) {
    if (stringSiap.load(std::memory_order_acquire))
        return kelasStringGlobal;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!stringSiap.load(std::memory_order_relaxed)) {
        kelasStringGlobal = ambilKelas(env, OBFUSCATE("java/lang/String"));
        stringSiap.store(true, std::memory_order_release);
    }
    return kelasStringGlobal;
}

const KelasDialog* cacheDialog(JNIEnv* env) {
    if (dialogSiap.load(std::memory_order_acquire))
        return dialog.kelas ? &dialog : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!dialogSiap.load(std::memory_order_relaxed)) {
        dialog.kelas = ambilKelas(env, OBFUSCATE("com/android/support/DialogHelper"));
        dialog.tampil = ambilMetodeStatis(
                env, dialog.kelas, OBFUSCATE("showDialogWithLink"),
                OBFUSCATE("(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;"
                          "Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V"));
        dialogSiap.store(true, std::memory_order_release);
    }
    return dialog.kelas ? &dialog : nullptr;
}

const KelasToast* cacheToast(JNIEnv* env) {
    if (toastSiap.load(std::memory_order_acquire))
        return toast.kelas ? &toast : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!toastSiap.load(std::memory_order_relaxed)) {
        toast.kelas = ambilKelas(env, OBFUSCATE("android/widget/Toast"));
        toast.buat = ambilMetodeStatis(
                env, toast.kelas, OBFUSCATE("makeText"),
                OBFUSCATE("(Landroid/content/Context;Ljava/lang/CharSequence;I)"
                          "Landroid/widget/Toast;"));
        toast.tampil = ambilMetode(env, toast.kelas, OBFUSCATE("show"), OBFUSCATE("()V"));
        toastSiap.store(true, std::memory_order_release);
    }
    return toast.kelas ? &toast : nullptr;
}

const KelasMetode* cacheHtml(JNIEnv* env) {
    if (htmlSiap.load(std::memory_order_acquire))
        return html.kelas ? &html : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!htmlSiap.load(std::memory_order_relaxed)) {
        html.kelas = ambilKelas(env, OBFUSCATE("android/text/Html"));
        html.metode =
                ambilMetodeStatis(env, html.kelas, OBFUSCATE("fromHtml"),
                                  OBFUSCATE("(Ljava/lang/String;)Landroid/text/Spanned;"));
        htmlSiap.store(true, std::memory_order_release);
    }
    return html.kelas ? &html : nullptr;
}

const KelasMetode* cacheTextView(JNIEnv* env) {
    if (textViewSiap.load(std::memory_order_acquire))
        return textView.kelas ? &textView : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!textViewSiap.load(std::memory_order_relaxed)) {
        textView.kelas = ambilKelas(env, OBFUSCATE("android/widget/TextView"));
        textView.metode = ambilMetode(env, textView.kelas, OBFUSCATE("setText"),
                                      OBFUSCATE("(Ljava/lang/CharSequence;)V"));
        textViewSiap.store(true, std::memory_order_release);
    }
    return textView.kelas ? &textView : nullptr;
}

const KelasIntent* cacheIntent(JNIEnv* env) {
    if (intentSiap.load(std::memory_order_acquire))
        return intent.kelas ? &intent : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!intentSiap.load(std::memory_order_relaxed)) {
        intent.kelas = ambilKelas(env, OBFUSCATE("android/content/Intent"));
        intent.dariKonteks =
                ambilMetode(env, intent.kelas, OBFUSCATE("<init>"),
                            OBFUSCATE("(Landroid/content/Context;Ljava/lang/Class;)V"));
        intent.dariUri =
                ambilMetode(env, intent.kelas, OBFUSCATE("<init>"),
                            OBFUSCATE("(Ljava/lang/String;Landroid/net/Uri;)V"));
        intentSiap.store(true, std::memory_order_release);
    }
    return intent.kelas ? &intent : nullptr;
}

jclass cacheLauncher(JNIEnv* env) {
    if (launcherSiap.load(std::memory_order_acquire))
        return launcher;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!launcherSiap.load(std::memory_order_relaxed)) {
        launcher = ambilKelas(env, OBFUSCATE("com/android/support/Launcher"));
        launcherSiap.store(true, std::memory_order_release);
    }
    return launcher;
}

const KelasMetode* cacheUri(JNIEnv* env) {
    if (uriSiap.load(std::memory_order_acquire))
        return uri.kelas ? &uri : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!uriSiap.load(std::memory_order_relaxed)) {
        uri.kelas = ambilKelas(env, OBFUSCATE("android/net/Uri"));
        uri.metode = ambilMetodeStatis(env, uri.kelas, OBFUSCATE("parse"),
                                       OBFUSCATE("(Ljava/lang/String;)Landroid/net/Uri;"));
        uriSiap.store(true, std::memory_order_release);
    }
    return uri.kelas ? &uri : nullptr;
}

const KelasBuild* cacheBuild(JNIEnv* env) {
    if (buildSiap.load(std::memory_order_acquire))
        return build.kelas ? &build : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!buildSiap.load(std::memory_order_relaxed)) {
        build.kelas = ambilKelas(env, OBFUSCATE("android/os/Build$VERSION"));
        build.sdk = ambilFieldStatis(env, build.kelas, OBFUSCATE("SDK_INT"), OBFUSCATE("I"));
        buildSiap.store(true, std::memory_order_release);
    }
    return build.kelas ? &build : nullptr;
}

const KelasMetode* cacheSettings(JNIEnv* env) {
    if (settingsSiap.load(std::memory_order_acquire))
        return settings.kelas ? &settings : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!settingsSiap.load(std::memory_order_relaxed)) {
        settings.kelas = ambilKelas(env, OBFUSCATE("android/provider/Settings"));
        settings.metode =
                ambilMetodeStatis(env, settings.kelas, OBFUSCATE("canDrawOverlays"),
                                  OBFUSCATE("(Landroid/content/Context;)Z"));
        settingsSiap.store(true, std::memory_order_release);
    }
    return settings.kelas ? &settings : nullptr;
}

const KelasKonteks* cacheKonteks(JNIEnv* env) {
    if (konteksSiap.load(std::memory_order_acquire))
        return konteks.kelas ? &konteks : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!konteksSiap.load(std::memory_order_relaxed)) {
        konteks.kelas = ambilKelas(env, OBFUSCATE("android/content/Context"));
        konteks.mulaiLayanan =
                ambilMetode(env, konteks.kelas, OBFUSCATE("startService"),
                            OBFUSCATE("(Landroid/content/Intent;)Landroid/content/ComponentName;"));
        konteks.mulaiAktivitas =
                ambilMetode(env, konteks.kelas, OBFUSCATE("startActivity"),
                            OBFUSCATE("(Landroid/content/Intent;)V"));
        konteks.namaPaket = ambilMetode(env, konteks.kelas, OBFUSCATE("getPackageName"),
                                        OBFUSCATE("()Ljava/lang/String;"));
        konteksSiap.store(true, std::memory_order_release);
    }
    return konteks.kelas ? &konteks : nullptr;
}

const KelasAktivitas* cacheAktivitas(JNIEnv* env) {
    if (aktivitasSiap.load(std::memory_order_acquire))
        return aktivitas.kelas ? &aktivitas : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!aktivitasSiap.load(std::memory_order_relaxed)) {
        aktivitas.kelas = ambilKelas(env, OBFUSCATE("android/app/Activity"));
        aktivitas.manajerPaket = ambilMetode(
                env, aktivitas.kelas, OBFUSCATE("getPackageManager"),
                OBFUSCATE("()Landroid/content/pm/PackageManager;"));
        aktivitas.namaPaket = ambilMetode(env, aktivitas.kelas, OBFUSCATE("getPackageName"),
                                          OBFUSCATE("()Ljava/lang/String;"));
        aktivitasSiap.store(true, std::memory_order_release);
    }
    return aktivitas.kelas ? &aktivitas : nullptr;
}

const KelasManajerPaket* cacheManajerPaket(JNIEnv* env) {
    if (manajerPaketSiap.load(std::memory_order_acquire))
        return manajerPaket.kelas ? &manajerPaket : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!manajerPaketSiap.load(std::memory_order_relaxed)) {
        manajerPaket.kelas = ambilKelas(env, OBFUSCATE("android/content/pm/PackageManager"));
        manajerPaket.infoPaket = ambilMetode(
                env, manajerPaket.kelas, OBFUSCATE("getPackageInfo"),
                OBFUSCATE("(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"));
        manajerPaketSiap.store(true, std::memory_order_release);
    }
    return manajerPaket.kelas ? &manajerPaket : nullptr;
}

const KelasInfoPaket* cacheInfoPaket(JNIEnv* env) {
    if (infoPaketSiap.load(std::memory_order_acquire))
        return infoPaket.kelas ? &infoPaket : nullptr;
    if (!env)
        return nullptr;
    std::lock_guard<std::mutex> tahan(kunciCache);
    if (!infoPaketSiap.load(std::memory_order_relaxed)) {
        infoPaket.kelas = ambilKelas(env, OBFUSCATE("android/content/pm/PackageInfo"));
        infoPaket.namaVersi = ambilField(env, infoPaket.kelas, OBFUSCATE("versionName"),
                                         OBFUSCATE("Ljava/lang/String;"));
        infoPaket.kodeVersi =
                ambilField(env, infoPaket.kelas, OBFUSCATE("versionCode"), OBFUSCATE("I"));
        infoPaketSiap.store(true, std::memory_order_release);
    }
    return infoPaket.kelas ? &infoPaket : nullptr;
}

void siapkanCacheJni(JNIEnv* env) {
    if (!env)
        return;
    kelasString(env);
    cacheDialog(env);
    cacheToast(env);
    cacheHtml(env);
    cacheTextView(env);
    cacheIntent(env);
    cacheLauncher(env);
    cacheUri(env);
    cacheBuild(env);
    cacheSettings(env);
    cacheKonteks(env);
    cacheAktivitas(env);
    cacheManajerPaket(env);
    cacheInfoPaket(env);
}
