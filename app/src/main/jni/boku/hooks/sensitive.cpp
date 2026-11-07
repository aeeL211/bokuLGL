#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "targets.h"

namespace {

// Setiap pemeriksaan teks di game diputuskan di sini: daftar kata ditanyakan
// lebih dulu, dan bila pemanggil juga meminta catatan, catatan itu dimunculkan.
// Menjawab false melewati keduanya, jadi baris chat, nama, dan teks profil lolos.
bool hookSensitive(void* self, void* text, int32_t bahasa, bool record, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*, int32_t, bool, void*)>(
        target(T_SENSITIVE_WORD).orig);
    if (!features().wordRude)
        return original ? original(self, text, bahasa, record, method) : false;
    return false;
}

} // namespace

void installSensitiveHook() {
    installHook(target(T_SENSITIVE_WORD), reinterpret_cast<void*>(hookSensitive));
}
