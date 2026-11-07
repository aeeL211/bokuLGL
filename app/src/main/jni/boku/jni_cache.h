#pragma once

#include <jni.h>

// Kelas dan method Java yang dipakai jembatan menu diikat sekali sebagai
// referensi global. Pencarian nama karena itu tidak diulang pada setiap toast
// atau dialog, dan kelas yang hilang hanya dicatat sekali.

struct KelasMetode {
    jclass kelas;
    jmethodID metode;
};

struct KelasDialog {
    jclass kelas;
    jmethodID tampil;
};

struct KelasToast {
    jclass kelas;
    jmethodID buat;
    jmethodID tampil;
};

struct KelasIntent {
    jclass kelas;
    jmethodID dariKonteks;
    jmethodID dariUri;
};

struct KelasBuild {
    jclass kelas;
    jfieldID sdk;
};

struct KelasKonteks {
    jclass kelas;
    jmethodID mulaiLayanan;
    jmethodID mulaiAktivitas;
    jmethodID namaPaket;
};

struct KelasAktivitas {
    jclass kelas;
    jmethodID manajerPaket;
    jmethodID namaPaket;
};

struct KelasManajerPaket {
    jclass kelas;
    jmethodID infoPaket;
};

struct KelasInfoPaket {
    jclass kelas;
    jfieldID namaVersi;
    jfieldID kodeVersi;
};

// Mengisi cache lebih awal dari JNI_OnLoad. Kegagalan satu kelas tidak
// menghentikan kelas lain; pemanggil memeriksa hasilnya sendiri.
void siapkanCacheJni(JNIEnv* env);

jclass kelasString(JNIEnv* env);

const KelasDialog* cacheDialog(JNIEnv* env);
const KelasToast* cacheToast(JNIEnv* env);
const KelasMetode* cacheHtml(JNIEnv* env);
const KelasMetode* cacheTextView(JNIEnv* env);
const KelasIntent* cacheIntent(JNIEnv* env);
jclass cacheLauncher(JNIEnv* env);
const KelasMetode* cacheUri(JNIEnv* env);
const KelasBuild* cacheBuild(JNIEnv* env);
const KelasMetode* cacheSettings(JNIEnv* env);
const KelasKonteks* cacheKonteks(JNIEnv* env);
const KelasAktivitas* cacheAktivitas(JNIEnv* env);
const KelasManajerPaket* cacheManajerPaket(JNIEnv* env);
const KelasInfoPaket* cacheInfoPaket(JNIEnv* env);
