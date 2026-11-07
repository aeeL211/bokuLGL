#pragma once

#include <jni.h>

// Daftar fitur dan setelan yang dibaca menu. Isinya adalah kontrak dengan
// Java: setiap baris dibaca sebagai nomor, tipe, dan label, jadi teksnya tidak
// boleh berubah saat refaktor.
jobjectArray daftarFitur(JNIEnv* env);
jobjectArray daftarSetelan(JNIEnv* env);
