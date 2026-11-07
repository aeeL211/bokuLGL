#pragma once

#include <ctime>

// Jam monotonik dipakai untuk irama antrean, masa berlaku daftar pemain, dan
// pembaruan posisi. Perubahan jam dinding karena zona waktu atau penyetelan
// pengguna karena itu tidak pernah mengubah jeda fitur.
inline double nowSeconds() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}
