#pragma once

#include <atomic>
#include <cstdint>

// Menu hanya menulis nilai di sini. Setiap hook membaca sakelar pada tiap
// panggilan, jadi menyalakan atau mematikan fitur tidak pernah memasang atau
// melepas hook saat game berjalan. Tulisannya datang dari thread UI dan
// bacaannya dari thread game, jadi setiap sakelar atomik.
struct Fitur {
    std::atomic<bool> iap{true};
    std::atomic<bool> slot{true};
    std::atomic<bool> inputTeks{true};
    // Batas geser skin dalam satuan game: 5 berarti -5..+5 seperti bawaan,
    // 25 berarti -25..+25. Menu mengirim 5..100.
    std::atomic<int32_t> rentangSkin{25};
    std::atomic<bool> kataKasar{true};
    std::atomic<bool> simpanDunia{true};
    std::atomic<bool> ukuranRoom{true};
    std::atomic<bool> sandiRoom{true};
    std::atomic<bool> privasi{true};
    std::atomic<bool> chatPribadi{true};
    std::atomic<bool> tembakOtomatis{true};
    std::atomic<bool> busterPenuh{true};
    std::atomic<bool> kotoran{true};
    std::atomic<bool> makanan{true};
    // Getar kamera dan puing tile dipisah karena getar hanya mengganggu
    // tampilan, sedangkan puing ikut menentukan beban gambar di layar.
    std::atomic<bool> getar{true};
    std::atomic<bool> pecahan{true};
    std::atomic<bool> terpental{true};
    std::atomic<bool> noClip{false};
    // Nilai slider dikirim menu sebagai bilangan bulat: kecepatan 5..50 berarti
    // 0,5x..5,0x dan gravitasi 0..30 berarti 0,0x..3,0x. Nilai 10 adalah 1,0x.
    std::atomic<int32_t> kecepatanPemain{10};
    std::atomic<int32_t> gravitasiPemain{10};
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
