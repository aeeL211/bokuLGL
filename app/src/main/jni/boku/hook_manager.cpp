#include "hook_manager.h"

#include <shadowhook.h>

#include <atomic>
#include <mutex>

#include "log.h"

namespace {

std::atomic<int> countInstalled{0};
std::atomic<bool> hookReady{false};
// ShadowHook keeps its own init result, so a failure here is permanent. It is
// remembered so later hooks do not try again and do not flood logcat with the
// same message.
std::atomic<bool> hookFail{false};

// Only one thread may call shadowhook_init. The flags above handle the fast
// path; this lock closes the race when the first two callers arrive together.
std::mutex keyInit;

#ifdef BOKU_HOOK_DEBUG
constexpr bool kDebugHook = true;
#else
constexpr bool kDebugHook = false;
#endif

} // namespace

// ShadowHook must be initialized once before the first hook. Multi mode is used
// because every proxy here always calls its original through Target::orig: the
// cost equals unique mode, but two installs on one address are still accepted and
// chained instead of failing. Shared mode is not used because its proxies must
// use SHADOWHOOK_CALL_PREV and SHADOWHOOK_STACK_SCOPE.
bool initializeHooks() {
    if (hookReady.load(std::memory_order_acquire))
        return true;
    if (hookFail.load(std::memory_order_acquire))
        return false;

    std::lock_guard<std::mutex> hold(keyInit);
    if (hookReady.load(std::memory_order_relaxed))
        return true;
    if (hookFail.load(std::memory_order_relaxed))
        return false;

    int code = shadowhook_init(SHADOWHOOK_MODE_MULTI, kDebugHook);
    if (code != SHADOWHOOK_ERRNO_OK) {
        hookFail.store(true, std::memory_order_release);
        LOGE("ShadowHook failed to initialize: %d %s", code, shadowhook_to_errmsg(code));
        return false;
    }
    hookReady.store(true, std::memory_order_release);
    LOGI("ShadowHook ready (%s, multi mode)", shadowhook_get_version());
    // The sh_linker.c patch skips the dlopen monitor because
    // libshadowhook_nothing.so is not bundled. All targets live in libraries that
    // are already loaded and never unloaded, so hooks do not depend on it.
    LOGI("dlopen monitor not used");
    return true;
}

bool installHook(Target& entry, void* replacement) {
    if (!entry.addr) {
        LOGE("Target not found: %s.%s", entry.cls, entry.name);
        return false;
    }
    // Two files can name the same method. The target remembers its first install
    // so the multi chain does not call the same proxy twice.
    if (entry.installed)
        return true;
    if (!initializeHooks())
        return false;

    void* stub = shadowhook_hook_func_addr_2(entry.addr, replacement, &entry.orig,
                                             SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int code = shadowhook_get_errno();
        LOGE("Hook failed: %s.%s (ShadowHook %d: %s)", entry.cls, entry.name, code,
             shadowhook_to_errmsg(code));
        return false;
    }
    entry.stub = stub;
    entry.installed = true;
    countInstalled.fetch_add(1, std::memory_order_relaxed);
    return true;
}

int countHookInstalled() { return countInstalled.load(std::memory_order_relaxed); }

bool installProbeHook(void* address, void* replacement, void** original, const char* name) {
    if (!address) {
        LOGE("Probe not found: %s", name);
        return false;
    }
    if (!replacement || !original) {
        LOGE("Probe invalid: %s", name);
        return false;
    }
    // The probe is installed only once, so do not chain the same proxy twice.
    if (*original)
        return true;
    if (!initializeHooks())
        return false;

    void* stub = shadowhook_hook_func_addr_2(address, replacement, original,
                                             SHADOWHOOK_HOOK_WITH_MULTI_MODE);
    if (!stub) {
        int code = shadowhook_get_errno();
        LOGE("Probe failed: %s (ShadowHook %d: %s)", name, code, shadowhook_to_errmsg(code));
        return false;
    }
    return true;
}
