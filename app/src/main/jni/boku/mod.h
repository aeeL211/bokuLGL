#pragma once

#include <jni.h>

// Dipanggil sekali dari JNI_OnLoad milik menu, karena library ini hanya boleh
// mengekspor satu JNI_OnLoad. Library dimuat di proses game, jadi activity
// Unity sudah ada saat pemanggilan ini.
void setupMod(JavaVM* vm, JNIEnv* env);
