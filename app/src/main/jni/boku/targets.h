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
    bool terpasang;
};

Target& target(TargetId id);

// Mendekripsi tabel nama target dengan kunci build. Nama target tidak pernah
// tertulis polos di pustaka, dan setiap build memakai kunci berbeda.
bool inisialisasiTarget();

// Mencari setiap target berdasarkan nama kelas, nama method, dan jumlah parameter.
// Mengembalikan berapa yang ditemukan, yang bernilai T_COUNT bila semuanya berjalan
// sehat.
int scan();
