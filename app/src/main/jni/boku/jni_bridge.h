#pragma once

#include <jni.h>

#include <cstddef>

// JNI dipakai oleh jalur log, toast dan sufiks versi, jadi pegangan VM dan kelas
// UnityPlayer berada di luar guard pencatatan.
JavaVM* vmJava();
void setVmJava(JavaVM* vm);

jclass klassUnityPlayer();
void saveKlassUnityPlayer(JNIEnv* env);

JavaVM* findVmJava();
bool lekatJni(JNIEnv** env, bool* release);

// getExternalStorageDirectory adalah satu-satunya akar yang juga bekerja untuk
// path milik aplikasi lain, jadi cadangan log dan berkas nama sama-sama memakainya.
bool rootStorageExternal(char* out, size_t size);

// Nama paket diambil dari /proc/self/cmdline, bukan ditulis ke dalam sumber, jadi
// game yang diganti nama atau dikloning tetap menemukan berkasnya sendiri.
bool packetApplication(char* out, size_t size);
