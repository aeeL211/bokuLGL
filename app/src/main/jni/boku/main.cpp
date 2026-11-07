#include <jni.h>
#include <pthread.h>
#include <unistd.h>

#include "Utils.hpp"
#include "hook_manager.h"
#include "hooks/hooks.h"
#include "il2cpp_api.h"
#include "info.h"
#include "jni_bridge.h"
#include "log.h"
#include "mod.h"
#include "targets.h"
#include "toast.h"

#if !defined(__aarch64__)
#error "Hanya arm64-v8a yang didukung"
#endif

namespace {

void* worker(void*) {
    LOGI("Worker started");
    // JNI_OnLoad runs after the constructor returns, so the activity class may
    // not be cached yet when this thread starts.
    for (int i = 0; i < 10 && !klassUnityPlayer(); i++)
        usleep(100000);
    recordEnvironment();
    if (!initializeTargets())
        return nullptr;

    // The menu asks IsGameLibLoaded() before showing the feature list, so this
    // wait also marks that libil2cpp.so is available.
    for (int i = 0; i < 120 && !isLibraryLoaded("libil2cpp.so"); i++) {
        sleep(1);
    }
    sleep(2);

    if (!bindApi()) {
        LOGE("IL2CPP API not found");
        return nullptr;
    }
    // ShadowHook installs its own SIGSEGV/SIGBUS handler for write protection.
    // The mod crash handler is installed first, then ShadowHook is initialized
    // after Unity has installed its handler, so the chain is
    // bytesig -> mod log -> Unity handler. The probes are installed after the
    // module table used to resolve them is captured.
    installCrashLog();
    // Initialization failure does not stop the mod: each hook logs its own
    // failure, read-only field features keep working, and the game is left on
    // vanilla behavior.
    if (initializeHooks())
        installCrashProbe();
    // libil2cpp and its companion libraries finish loading before this point,
    // so the module table is captured again so later crash frames can be named.
    refreshModules();
    LOGI("IL2CPP API bound");

    void* domain = nullptr;
    for (int i = 0; i < 60 && !(domain = apiIl2cpp().domain()); i++) {
        sleep(1);
    }
    if (!domain) {
        LOGE("IL2CPP domain not ready");
        return nullptr;
    }

    LOGI("Domain ready, scan starting");
    apiIl2cpp().attach(domain);
    int found = scan();
    LOGI("Scan complete: %d of %d methods found", found, T_COUNT);

    installIapHook();
    installMultiplayerHook();
    installChatHook();
    installSensitiveHook();
    installTextInputHook();
    installProfileHook();
    installPasswordHook();
    installSkinRangeHook();
    installAboutHook();
    findLocalPlayerFields();
    installHandHook();
    installFoodHook();
    installPrivacyHook();
    findActionFields();
    installWeaponHook();
    installEffectsHook();
    installKnockbackHook();
    installFecesHook();
    installSkinHook();
    installSlotHook();
    installSpoofHook();
    installRoomHook();
    installWorldHook();
    installPlayerHook();
    refreshModules();
    LOGI("Hooks installed: %d", countHookInstalled());
    toastEnter();
    return nullptr;
}

} // namespace

void setupMod(JavaVM* vm, JNIEnv* env) {
    setVmJava(vm);
    if (env)
        saveKlassUnityPlayer(env);
    openLogFile();
}

__attribute__((constructor)) static void onLoad() {
    openLogFile();
    LOGI("Library loaded");
    pthread_t t;
    if (pthread_create(&t, nullptr, worker, nullptr) == 0) {
        pthread_detach(t);
    } else {
        LOGE("Failed to create thread");
    }
}
