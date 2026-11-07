#pragma once

#include <cstddef>

#include "targets_generated.h"

struct Target {
    const char* cls;
    const char* name;
    const char* paramType;
    int argc;
    void* addr;
    void* method;
    void* orig;
    // ShadowHook's stub for unhooking. It is never released while the mod runs,
    // but it is kept so the installation can be traced and unhooked correctly
    // if that is ever needed.
    void* stub;
    bool installed;
};

Target& target(TargetId id);

// Decrypts the target name table with the build key. Target names are never
// stored in plain text in the library, and every build uses a different key.
bool initializeTargets();

// Finds every target by class name, method name, and parameter count. Returns
// how many were found, which equals T_COUNT when everything is healthy.
int scan();
