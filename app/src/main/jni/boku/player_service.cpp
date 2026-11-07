#include "player_service.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "waktu.h"

namespace {

constexpr double kJedaAksi = 0.25;
constexpr double kJedaDaftar = 0.4;

// Salinan tetap hidup dua detik sesudah pembacaan terakhir. Menu membaca sekali
// per detik, jadi menutup menu menghentikan pembaruan tanpa menghapus data yang
// masih ditampilkan.
constexpr double kMasaDaftar = 2.0;
constexpr double kMasaPosisi = 2.0;

std::atomic<bool> layananSiap{false};

std::mutex kunciDaftar;
pemain::Ringkas daftarPemain[pemain::kMaksPemain] = {};
int jumlahPemain = 0;
double berikutDaftar = 0.0;
std::atomic<double> daftarDiminta{0.0};

std::mutex kunciAksi;
pemain::Aksi antrean[pemain::kBatasAksi] = {};
int jumlahAntrean = 0;
double berikutAksi = 0.0;

struct PosisiRingkas {
    bool ada;
    bool adaTile;
    float tileX;
    float tileY;
    float duniaX;
    float duniaY;
};

std::mutex kunciPosisi;
PosisiRingkas posisiRingkas = {};
std::atomic<double> posisiDiminta{0.0};

bool antrekan(pemain::Aksi::Jenis jenis, const char* kunci, const char* nama) {
    bool butuhKunci = jenis == pemain::Aksi::Jenis::Tendang ||
                      jenis == pemain::Aksi::Jenis::TendangPermanen ||
                      jenis == pemain::Aksi::Jenis::Salin;
    if (!layananSiap.load() || (butuhKunci && (!kunci || !*kunci)) ||
        (kunci && std::strlen(kunci) >= pemain::kKarakterKunci))
        return false;

    std::lock_guard<std::mutex> tahan(kunciAksi);
    bool diterima = jumlahAntrean < pemain::kBatasAksi;
    if (diterima) {
        pemain::Aksi& aksi = antrean[jumlahAntrean++];
        aksi.jenis = jenis;
        std::snprintf(aksi.kunci, sizeof(aksi.kunci), "%s", kunci ? kunci : "");
        std::snprintf(aksi.nama, sizeof(aksi.nama), "%s", nama && *nama ? nama : kunci);
    }
    return diterima;
}

} // namespace

namespace pemain {

void setelSiap(bool siap) { layananSiap.store(siap); }

bool layaniDaftar() {
    double sekarang = sekarangDetik();
    if (sekarang < berikutDaftar || sekarang > daftarDiminta.load())
        return false;
    berikutDaftar = sekarang + kJedaDaftar;
    return true;
}

void simpanDaftar(const Ringkas* data, int jumlah) {
    if (!data || jumlah < 0 || jumlah > kMaksPemain)
        return;
    std::lock_guard<std::mutex> tahan(kunciDaftar);
    std::memcpy(daftarPemain, data, sizeof(Ringkas) * static_cast<std::size_t>(jumlah));
    jumlahPemain = jumlah;
}

void salinDaftar(char* keluaran, std::size_t ukuran) {
    if (!keluaran || ukuran == 0)
        return;
    keluaran[0] = '\0';
    daftarDiminta.store(sekarangDetik() + kMasaDaftar);

    Ringkas salinan[kMaksPemain] = {};
    int jumlah = 0;
    {
        std::lock_guard<std::mutex> tahan(kunciDaftar);
        jumlah = jumlahPemain;
        std::memcpy(salinan, daftarPemain, sizeof(Ringkas) * static_cast<std::size_t>(jumlah));
    }

    std::size_t posisi = 0;
    for (int i = 0; i < jumlah; i++) {
        int ditulis =
            std::snprintf(keluaran + posisi, ukuran - posisi, "%s%s\t%s\t%s", i ? "\n" : "",
                          salinan[i].nama, salinan[i].kunci, salinan[i].peran);
        if (ditulis < 0 || static_cast<std::size_t>(ditulis) >= ukuran - posisi)
            break;
        posisi += static_cast<std::size_t>(ditulis);
    }
}

bool antrekanTendang(const char* kunci, const char* nama, bool permanen) {
    return antrekan(permanen ? Aksi::Jenis::TendangPermanen : Aksi::Jenis::Tendang, kunci, nama);
}

bool antrekanTendangHost(const char* nama) {
    return antrekan(Aksi::Jenis::TendangHost, nullptr, nama);
}

bool antrekanTendangSemua() { return antrekan(Aksi::Jenis::TendangSemua, nullptr, "semua tamu"); }

bool antrekanSalin(const char* kunci, const char* nama) {
    return antrekan(Aksi::Jenis::Salin, kunci, nama);
}

bool ambilAksi(Aksi& keluar) {
    double sekarang = sekarangDetik();
    if (sekarang < berikutAksi)
        return false;

    bool ada = false;
    {
        std::lock_guard<std::mutex> tahan(kunciAksi);
        if (jumlahAntrean > 0) {
            keluar = antrean[0];
            for (int i = 1; i < jumlahAntrean; i++)
                antrean[i - 1] = antrean[i];
            jumlahAntrean--;
            ada = true;
        }
    }
    if (!ada)
        return false;

    berikutAksi = sekarang + kJedaAksi;
    return true;
}

bool layaniPosisi() { return sekarangDetik() < posisiDiminta.load(); }

void simpanPosisi(bool adaTile, float tileX, float tileY, float duniaX, float duniaY) {
    std::lock_guard<std::mutex> tahan(kunciPosisi);
    posisiRingkas.ada = true;
    posisiRingkas.adaTile = adaTile;
    posisiRingkas.tileX = adaTile ? tileX : 0.0f;
    posisiRingkas.tileY = adaTile ? tileY : 0.0f;
    posisiRingkas.duniaX = duniaX;
    posisiRingkas.duniaY = duniaY;
}

void salinPosisi(char* keluaran, std::size_t ukuran) {
    if (!keluaran || ukuran == 0)
        return;
    keluaran[0] = '\0';
    posisiDiminta.store(sekarangDetik() + kMasaPosisi);

    PosisiRingkas salinan = {};
    {
        std::lock_guard<std::mutex> tahan(kunciPosisi);
        salinan = posisiRingkas;
    }

    if (!salinan.ada)
        return;
    if (salinan.adaTile) {
        std::snprintf(keluaran, ukuran, "%.2f\t%.2f\t%.1f\t%.1f", salinan.tileX, salinan.tileY,
                      salinan.duniaX, salinan.duniaY);
    } else {
        std::snprintf(keluaran, ukuran, "\t\t%.1f\t%.1f", salinan.duniaX, salinan.duniaY);
    }
}

} // namespace pemain
