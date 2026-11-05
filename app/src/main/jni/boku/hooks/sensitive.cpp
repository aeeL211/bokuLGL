#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "targets.h"

namespace {

// Setiap pemeriksaan teks di game diputuskan di sini: daftar kata ditanyakan
// lebih dulu, dan bila pemanggil juga meminta catatan, catatan itu dimunculkan.
// Menjawab false melewati keduanya, jadi baris chat, nama, dan teks profil lolos.
bool hookSensitif(void* self, void* teks, int32_t bahasa, bool catat, void* metode) {
    auto asli = reinterpret_cast<bool (*)(void*, void*, int32_t, bool, void*)>(
        target(T_SENSITIVE_WORD).orig);
    if (!fitur().kataKasar)
        return asli ? asli(self, teks, bahasa, catat, metode) : false;
    return false;
}

} // namespace

void pasangHookSensitif() {
    pasangHook(target(T_SENSITIVE_WORD), reinterpret_cast<void*>(hookSensitif));
}
