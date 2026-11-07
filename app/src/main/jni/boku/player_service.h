#pragma once

#include <cstddef>

// Data yang dibagi antara thread game dan thread menu. Thread game menulis
// salinan kecil di sini, thread menu hanya membaca salinan itu; tidak ada objek
// IL2CPP yang pernah menyeberang.
namespace pemain {

constexpr int kMaksPemain = 16;
constexpr int kBatasAksi = 8;
constexpr std::size_t kKarakterKunci = 40;
constexpr std::size_t kKarakterNama = 96;

struct Ringkas {
    char nama[kKarakterNama];
    char kunci[kKarakterKunci];
    char peran[8];
};

struct Aksi {
    enum class Jenis { Tendang, TendangPermanen, TendangHost, TendangSemua, Salin };

    Jenis jenis;
    char kunci[kKarakterKunci];
    char nama[kKarakterNama];
};

// Ditulis sekali oleh hook room setelah field daftar tamu ditemukan.
void setelSiap(bool siap);

// Jendela pembaruan daftar pemain. Mengembalikan true bila thread game boleh
// menyusun salinan baru sekarang; pemanggil yang menentukan isinya.
bool layaniDaftar();
void simpanDaftar(const Ringkas* data, int jumlah);

// Baris "nama<TAB>kunci<TAB>peran", dipisah newline. Aman dari thread UI.
void salinDaftar(char* keluaran, std::size_t ukuran);

// Antrean aksi menu. Antrean berbatas dan tidak memblokir thread UI.
bool antrekanTendang(const char* kunci, const char* nama, bool permanen);
bool antrekanTendangHost(const char* nama);
bool antrekanTendangSemua();
bool antrekanSalin(const char* kunci, const char* nama);
bool ambilAksi(Aksi& keluar);

// Jendela pembaruan posisi pemain lokal.
bool layaniPosisi();
void simpanPosisi(bool adaTile, float tileX, float tileY, float duniaX, float duniaY);

// Baris "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY"; bagian tile kosong bila game
// tidak menyediakan field tile.
void salinPosisi(char* keluaran, std::size_t ukuran);

} // namespace pemain
