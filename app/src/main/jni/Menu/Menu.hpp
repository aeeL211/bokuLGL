#ifndef MENU_HPP
#define MENU_HPP

#include "Jni.hpp"

void Init(JNIEnv *env, jobject thiz, jobject ctx, jobject title, jobject subtitle);

void setText(JNIEnv *env, jobject obj, const char* text);

jstring Icon(JNIEnv *env, jobject thiz);

jstring IconWebViewData(JNIEnv *env, jobject thiz);

jobjectArray SettingsList(JNIEnv *env, jobject activityObject);

jobjectArray GetFeatureList(JNIEnv *env, jobject context);

// Section Room: teks "nama<TAB>kunci" per tamu, dipisah newline.
jstring DaftarPemain(JNIEnv *env, jclass clazz);

jboolean TendangPemain(JNIEnv *env, jclass clazz, jstring kunci, jstring nama);

jboolean SalinSkinPemain(JNIEnv *env, jclass clazz, jstring kunci, jstring nama);

void Changes(JNIEnv *env, jclass clazz, jobject obj,
             jint featNum, jstring featName, jint value, jlong Lvalue,
             jboolean boolean, jstring str);

#endif