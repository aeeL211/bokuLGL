#pragma once

#include <jni.h>

// Kelas dan method Java yang dipakai jembatan menu diikat sekali sebagai
// referensi global. Pencarian nama karena itu tidak diulang pada setiap toast
// atau dialog, dan kelas yang hilang hanya dicatat sekali.

struct ClassMethod {
    jclass klass;
    jmethodID method;
};

struct DialogClass {
    jclass klass;
    jmethodID show;
};

struct ToastClass {
    jclass klass;
    jmethodID create;
    jmethodID show;
};

struct IntentClass {
    jclass klass;
    jmethodID fromContext;
    jmethodID fromUri;
};

struct BuildClass {
    jclass klass;
    jfieldID sdk;
};

struct ContextClass {
    jclass klass;
    jmethodID startService;
    jmethodID startActivity;
    jmethodID packetName;
};

struct ActivityClass {
    jclass klass;
    jmethodID managerPacket;
    jmethodID packetName;
};

struct PacketManagerClass {
    jclass klass;
    jmethodID infoPacket;
};

struct PacketInfoClass {
    jclass klass;
    jfieldID versionName;
    jfieldID codeVersion;
};

// Mengisi cache lebih awal dari JNI_OnLoad. Kegagalan satu kelas tidak
// menghentikan kelas lain; pemanggil memeriksa hasilnya sendiri.
void prepareJniCache(JNIEnv* env);

jclass klassString(JNIEnv* env);

const DialogClass* cacheDialog(JNIEnv* env);
const ToastClass* cacheToast(JNIEnv* env);
const ClassMethod* cacheHtml(JNIEnv* env);
const ClassMethod* cacheTextView(JNIEnv* env);
const IntentClass* cacheIntent(JNIEnv* env);
jclass cacheLauncher(JNIEnv* env);
const ClassMethod* cacheUri(JNIEnv* env);
const BuildClass* cacheBuild(JNIEnv* env);
const ClassMethod* cacheSettings(JNIEnv* env);
const ContextClass* cacheContext(JNIEnv* env);
const ActivityClass* cacheActivity(JNIEnv* env);
const PacketManagerClass* cacheManagerPacket(JNIEnv* env);
const PacketInfoClass* cacheInfoPacket(JNIEnv* env);
