#ifndef MENU_HPP
#define MENU_HPP

#include "Jni.hpp"

void Init(JNIEnv *env, jobject thiz, jobject ctx, jobject title, jobject subtitle);

void setText(JNIEnv *env, jobject obj, const char* text);

jstring Icon(JNIEnv *env, jobject thiz);

jstring IconWebViewData(JNIEnv *env, jobject thiz);

jobjectArray SettingsList(JNIEnv *env, jobject activityObject);

jobjectArray GetFeatureList(JNIEnv *env, jobject context);

// Section Room: teks "nama<TAB>kunci<TAB>peran" per tamu, dipisah newline.
jstring PlayerList(JNIEnv *env, jclass clazz);

// Baris posisi pemain lokal: "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY".
jstring PlayerPosition(JNIEnv *env, jclass clazz);

jboolean KickPlayer(JNIEnv *env, jclass clazz, jstring key, jstring name,
                       jboolean permanent);

jboolean KickHost(JNIEnv *env, jclass clazz, jstring name);

jboolean KickAll(JNIEnv *env, jclass clazz);

jboolean CopyPlayerSkin(JNIEnv *env, jclass clazz, jstring key, jstring name);

void Changes(JNIEnv *env, jclass clazz, jobject obj,
             jint featNum, jstring featName, jint value, jlong Lvalue,
             jboolean boolean, jstring str);

#endif