#pragma once

#include <atomic>
#include <cstdint>

// Menu hanya menulis nilai di sini. Setiap hook membaca sakelar pada tiap
// panggilan, jadi menyalakan atau mematikan fitur tidak pernah memasang atau
// melepas hook saat game berjalan. Tulisannya datang dari thread UI dan
// bacaannya dari thread game, jadi setiap sakelar atomik.
struct Features {
    std::atomic<bool> iap{true};
    std::atomic<bool> slot{true};
    std::atomic<bool> inputText{true};
    // Batas geser skin dalam satuan game: 5 berarti -5..+5 seperti bawaan,
    // 25 berarti -25..+25. Menu mengirim 5..100.
    std::atomic<int32_t> skinRange{25};
    std::atomic<bool> wordRude{true};
    std::atomic<bool> saveWorld{true};
    std::atomic<bool> roomSize{true};
    // Izin build dan destroy di room orang lain ditembus dengan memaksa
    // Enforce pada paket tile dan Across pada paket objek.
    std::atomic<bool> permissionRoomOther{true};
    std::atomic<bool> roomPassword{true};
    std::atomic<bool> privacy{true};
    std::atomic<bool> privateChat{true};
    std::atomic<bool> shootAutomatic{true};
    std::atomic<bool> busterFull{true};
    std::atomic<bool> feces{true};
    std::atomic<bool> food{true};
    // Getar kamera dan puing tile dipisah karena getar hanya mengganggu
    // tampilan, sedangkan puing ikut menentukan beban gambar di layar.
    std::atomic<bool> shake{true};
    std::atomic<bool> pecahan{true};
    std::atomic<bool> knockback{true};
    std::atomic<bool> noClip{false};
    // Nilai slider dikirim menu sebagai bilangan bulat: kecepatan 5..50 berarti
    // 0,5x..5,0x dan gravitasi 0..30 berarti 0,0x..3,0x. Nilai 10 adalah 1,0x.
    std::atomic<int32_t> speedPlayer{10};
    std::atomic<int32_t> gravityPlayer{10};
};

// Tombol menu berjalan di thread UI, sedangkan posisi dibaca hook update pemain
// di thread game. Permintaan hanya dititipkan, lalu dipakai satu kali.
struct TeleportRequest {
    std::atomic<int32_t> x{0};
    std::atomic<int32_t> y{0};
    std::atomic<bool> menunggu{false};
};

Features& features();
TeleportRequest& teleportRequest();
