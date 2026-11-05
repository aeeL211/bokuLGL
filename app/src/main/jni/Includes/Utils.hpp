#ifndef UTILS
#define UTILS

#include <jni.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Menu menunggu libil2cpp.so sebelum menyusun daftar fitur, sehingga hasil
// pemindaian /proc/self/maps disimpan sekali.
extern bool mainLibLoaded;

jboolean isGameLibLoaded(JNIEnv* env, jobject thiz);

bool isLibraryLoaded(const char* libraryName);

#endif
