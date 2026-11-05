#include "Utils.hpp"

bool mainLibLoaded = false;

jboolean isGameLibLoaded(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    return mainLibLoaded;
}

// Dicari di /proc/self/maps, bukan dengan dlopen, supaya library yang sudah
// dipetakan game terdeteksi tanpa menaikkan refcount-nya.
bool isLibraryLoaded(const char* libraryName) {
    if (mainLibLoaded)
        return true;
    if (!libraryName || !*libraryName)
        return false;

    char line[512] = {};
    FILE* peta = fopen("/proc/self/maps", "rt");
    if (!peta)
        return false;
    while (fgets(line, sizeof(line), peta)) {
        if (strstr(line, libraryName)) {
            mainLibLoaded = true;
            break;
        }
    }
    fclose(peta);
    return mainLibLoaded;
}
