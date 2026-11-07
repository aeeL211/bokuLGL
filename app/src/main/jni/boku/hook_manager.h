#pragma once

#include "targets.h"

// Thin ShadowHook wrapper. Every hook goes through here so initialization, mode
// and failures are logged in one place; callers can leave that feature on
// vanilla behavior.
bool initializeHooks();
bool installHook(Target& entry, void* replacement);

// Hooks a raw address instead of a target entry. The crash probes use it for
// libc functions that no target names, and it does not count as a hook.
bool installProbeHook(void* address, void* replacement, void** original, const char* name);

// Number of hooks currently installed.
int countHookInstalled();
