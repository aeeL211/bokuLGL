#pragma once

#include <jni.h>

#include <cstddef>

// JNI is used by the log path, toasts, and the version suffix, so the VM handle
// and the UnityPlayer class live outside the logging guard.
JavaVM* vmJava();
void setVmJava(JavaVM* vm);

jclass klassUnityPlayer();
void saveKlassUnityPlayer(JNIEnv* env);

JavaVM* findVmJava();
bool attachJni(JNIEnv** env, bool* release);

// getExternalStorageDirectory is the only root that also works for paths owned
// by another app, so the log backup and name files both use it.
bool rootStorageExternal(char* out, size_t size);

// The package name is read from /proc/self/cmdline instead of being written into
// the source, so a renamed or cloned game still finds its own files.
bool packageApplication(char* out, size_t size);
