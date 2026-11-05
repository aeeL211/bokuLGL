#pragma once

#include <atomic>
#include <cstdint>

// Menu hanya menulis nilai di sini. Setiap hook membaca sakelar pada tiap
// panggilan, jadi menyalakan atau mematikan fitur tidak pernah memasang atau
// melepas hook saat game berjalan.
struct Fitur {
    bool iap = true;
    bool slot = true;
    bool inputTeks = true;
    bool rentangSkin = true;
    bool kataKasar = true;
    bool simpanDunia = true;
    bool ukuranRoom = true;
    bool sandiRoom = true;
    bool privasi = true;
    bool salinSkin = true;
    bool chatPribadi = true;
    bool tombolTendang = true;
    bool tembakOtomatis = true;
    bool busterPenuh = true;
    bool kotoran = true;
    bool makanan = true;
    bool efek = true;
    bool terpental = true;
    bool labelVersi = true;
    bool roomKit = true;
    bool noClip = false;
};

// Tombol menu berjalan di thread UI, sedangkan posisi dibaca hook update pemain
// di thread game. Permintaan hanya dititipkan, lalu dipakai satu kali.
struct PermintaanTeleport {
    std::atomic<int32_t> x{0};
    std::atomic<int32_t> y{0};
    std::atomic<bool> menunggu{false};
};

Fitur& fitur();
PermintaanTeleport& permintaanTeleport();
