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

bool versiAplikasi(char* nama, size_t size, jint& kode) {
    EnvScope lingkup;
    if (!lingkup)
        return false;
    JNIEnv* env = lingkup.env();

    bool berhasil = false;
    kode = 0;

    LocalRef<jobject> activity;
    if (kelasUnityPlayer()) {
        jfieldID fieldAktivitas =
            env->GetStaticFieldID(kelasUnityPlayer(), "currentActivity", "Landroid/app/Activity;");
        if (fieldAktivitas)
            activity = LocalRef<jobject>(
                env, env->GetStaticObjectField(kelasUnityPlayer(), fieldAktivitas));
        else
            bersihkanEksepsi(env, "UnityPlayer.currentActivity");
    }

    const KelasAktivitas* aktivitas = cacheAktivitas(env);
    LocalRef<jstring> paket;
    LocalRef<jobject> manajer;
    if (activity && aktivitas && aktivitas->namaPaket && aktivitas->manajerPaket) {
        paket = LocalRef<jstring>(
            env, static_cast<jstring>(env->CallObjectMethod(activity.get(), aktivitas->namaPaket)));
        manajer =
            LocalRef<jobject>(env, env->CallObjectMethod(activity.get(), aktivitas->manajerPaket));
    }

    const KelasManajerPaket* infoManajer = cacheManajerPaket(env);
    LocalRef<jobject> informasi;
    if (manajer && paket && infoManajer && infoManajer->infoPaket) {
        informasi = LocalRef<jobject>(
            env, env->CallObjectMethod(manajer.get(), infoManajer->infoPaket, paket.get(), 0));
    }

    const KelasInfoPaket* info = cacheInfoPaket(env);
    LocalRef<jstring> versi;
    if (informasi && info) {
        if (info->namaVersi)
            versi = LocalRef<jstring>(
                env, static_cast<jstring>(env->GetObjectField(informasi.get(), info->namaVersi)));
        if (info->kodeVersi)
            kode = env->GetIntField(informasi.get(), info->kodeVersi);
        if (versi) {
            StringUtf karakter(env, versi.get());
            if (karakter) {
                snprintf(nama, size, "%s", karakter.get());
                berhasil = true;
            }
        }
    }

    bersihkanEksepsi(env, "versi aplikasi");
    return berhasil;
}

} // namespace

void catatLingkungan() {
    char versi[64] = {};
    jint kode = 0;
    if (versiAplikasi(versi, sizeof(versi), kode))
        LOGI("Boku Boku %s (code %d)", versi, kode);
    else
        LOGI("Versi Boku Boku tidak terbaca");
    LOGI("Mod by Febrian/Ian (code aeeL), build %s", BOKU_BUILD_COMMIT);
}
