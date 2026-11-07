#pragma once

#include "targets.h"

// Pembungkus tipis ShadowHook. Setiap hook lewat sini supaya inisialisasi, mode,
// dan kegagalan tercatat di satu tempat; pemanggil bisa membiarkan fitur itu
// pada perilaku vanilla.
bool initializeHooks();
bool installHook(Target& entry, void* pengganti);

// Memasang hook pada alamat mentah, bukan pada entri target. Probe crash memakainya
// untuk fungsi libc yang tidak dinamai target mana pun, dan tidak ikut dihitung
// sebagai hook.
bool installProbeHook(void* address, void* pengganti, void** original, const char* name);

// Jumlah hook yang saat ini terpasang.
int countHookInstalled();
