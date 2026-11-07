#pragma once

#include <jni.h>

// Java classes and methods used by the menu bridge are bound once as global
// references. Name lookups are therefore not repeated on every toast or dialog,
// and a missing class is only logged once.

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
    jmethodID packageName;
};

struct ActivityClass {
    jclass klass;
    jmethodID managerPackage;
    jmethodID packageName;
};

struct PackageManagerClass {
    jclass klass;
    jmethodID infoPackage;
};

struct PackageInfoClass {
    jclass klass;
    jfieldID versionName;
    jfieldID versionCode;
};

// Fills the cache early from JNI_OnLoad. One class failing does not stop the
// others; callers check their own result.
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
const PackageManagerClass* cacheManagerPackage(JNIEnv* env);
const PackageInfoClass* cacheInfoPackage(JNIEnv* env);
