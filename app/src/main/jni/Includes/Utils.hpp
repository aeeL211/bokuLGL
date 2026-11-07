#ifndef UTILS
#define UTILS

#include <jni.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// The menu waits for libil2cpp.so before building the feature list, so the
// /proc/self/maps scan result is stored once. The scan thread and the UI thread
// read it together, so the value is atomic.
extern std::atomic<bool> mainLibLoaded;

jboolean isGameLibLoaded(JNIEnv* env, jobject thiz);

bool isLibraryLoaded(const char* libraryName);

#endif
