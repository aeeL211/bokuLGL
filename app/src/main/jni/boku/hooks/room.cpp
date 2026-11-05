#include "hooks.h"

#include <pthread.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"
#include "toast.h"

namespace {

using FnTendang = void (*)(void*, void*, void*);
using FnBersihkan = void (*)(void*, void*, void*);

constexpr int kMaksPemain = 16;
constexpr int kBatasAksi = 8;
constexpr size_t kKarakterKunci = 40;
constexpr size_t kKarakterNama = 96;
constexpr double kJedaAksi = 0.25;
constexpr double kJedaDaftar = 0.4;

// Daftar hanya disusun selama menu memintanya. Menu membaca sekali per detik,
// jadi salinan tetap hidup dua detik sesudah pembacaan terakhir.
constexpr double kMasaDaftar = 2.0;

Field fieldTamuManajer = {};
Field fieldMultiManajer = {};
Field fieldPemainManajer = {};
Field fieldItemTamu = {};
Field fieldUkuranTamu = {};
Field fieldKunciTamu = {};
Field fieldAkunTamu = {};
Field fieldNamaTamu = {};
Field fieldTransferMulti = {};
Field fieldPipeTransfer = {};
Field fieldManajerTamu = {};
std::atomic<bool> siapMenu{false};

// Daftar pemain untuk menu. Diisi di thread game tiap kJedaDaftar dan dibaca
// thread UI; kuncinya hanya teks, jadi room yang berganti di antaranya tidak
// meninggalkan pointer yang menggantung.
struct PemainRingkas {
    char nama[kKarakterNama];
    char kunci[kKarakterKunci];
    char peran[8];
};

pthread_mutex_t kunciDaftar = PTHREAD_MUTEX_INITIALIZER;
PemainRingkas daftarPemain[kMaksPemain] = {};
int jumlahPemain = 0;
double berikutDaftar = 0.0;
std::atomic<double> daftarDiminta{0.0};

enum class JenisAksi { Tendang, Salin };

// Aksi dari menu hanya menyimpan teks kunci dan nama yang diambil saat tombol
// ditekan, jadi antrean sisa pergantian room tidak menunjuk objek yang sudah
// dibebaskan.
struct Aksi {
    JenisAksi jenis;
    char kunci[kKarakterKunci];
    char nama[kKarakterNama];
};

pthread_mutex_t kunciAksi = PTHREAD_MUTEX_INITIALIZER;
Aksi antrean[kBatasAksi] = {};
int jumlahAntrean = 0;
double berikutAksi = 0.0;

double sekarangDetik() {
    timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

void teksDari(void* teks, char* keluaran, size_t ukuran, const char* cadangan) {
    keluaran[0] = '\0';
    if (teks && keUtf8(teks, keluaran, ukuran) && keluaran[0])
        return;
    snprintf(keluaran, ukuran, "%s", cadangan);
}

void* pipeDari(void* multi) {
    return bacaPtr(bacaPtr(multi, fieldTransferMulti), fieldPipeTransfer);
}

// Tombol tendang bawaan mengirim dua paket: tendangan membuat tamu menampilkan
// pemberitahuan, dan penghapusan tamu mengeluarkannya dari setiap daftar.
// Keduanya membawa kunci pengirim, jadi penggantian yang sama membuat klien
// menerimanya sebagai paket host.
bool tendangKunci(void* multi, void* kunci) {
    void* pipe = pipeDari(multi);
    auto tendang = reinterpret_cast<FnTendang>(target(T_KICK_SEND).addr);
    auto hapus = reinterpret_cast<FnBersihkan>(target(T_GUEST_REMOVE_SEND).addr);
    if (!kunci || !pipe || !tendang)
        return false;
    mulaiSpoof(multi);
    tendang(pipe, kunci, target(T_KICK_SEND).method);
    if (hapus)
        hapus(pipe, kunci, target(T_GUEST_REMOVE_SEND).method);
    hentiSpoof();
    return true;
}

// Nama tamu dibaca dari akun guest, sumber yang sama dengan daftar pemain di
// room. Bila nama tidak terbaca, barisnya memakai kunci tamu supaya tetap bisa
// dipakai.
void salinNama(void* tamu, char* keluaran, size_t ukuran, const char* cadangan) {
    void* akun = fieldAkunTamu.field ? bacaPtr(tamu, fieldAkunTamu) : nullptr;
    void* nama = akun && fieldNamaTamu.field ? bacaPtr(akun, fieldNamaTamu) : nullptr;
    teksDari(nama, keluaran, ukuran, cadangan);
    // Baris menu memakai tab dan newline sebagai pemisah, jadi karakter kendali
    // di nama pemain diganti spasi.
    for (char* p = keluaran; *p; p++) {
        if (static_cast<unsigned char>(*p) < 0x20)
            *p = ' ';
    }
}

// Jumlah tamu di daftar hidup. Nol berarti belum ada room, dan pada keadaan itu
// kunci host serta kunci pemain tidak boleh ditanyakan ke game: kedua getter
// melempar NullReferenceException saat room belum terbentuk, dan eksepsi yang
// lolos dari hook mematikan proses.
int32_t tamuDiRoom(void* manajerTamu) {
    void* manajer = bacaPtr(manajerTamu, fieldManajerTamu);
    void* daftar = bacaPtr(manajer, fieldTamuManajer);
    void* item = bacaPtr(daftar, fieldItemTamu);
    int32_t jumlah = item ? bacaInt(daftar, fieldUkuranTamu) : 0;
    int32_t kapasitas = item ? panjangArray(item) : 0;
    return jumlah > 0 && jumlah <= kapasitas ? jumlah : 0;
}

// Salinan daftar pemain untuk menu, diperbarui di thread game. Host dan pemain
// sendiri ikut ditampilkan dengan perannya, tetapi menu hanya menawarkan tendang
// untuk tamu: menendang keduanya mengakhiri room.
void segarkanDaftar(void* manajerTamu) {
    double sekarang = sekarangDetik();
    if (sekarang < berikutDaftar || sekarang > daftarDiminta.load())
        return;
    berikutDaftar = sekarang + kJedaDaftar;

    void* manajer = bacaPtr(manajerTamu, fieldManajerTamu);
    void* daftar = bacaPtr(manajer, fieldTamuManajer);
    int32_t jumlah = tamuDiRoom(manajerTamu);

    // Di luar room daftar kosong, dan kedua kunci tidak ditanyakan sama sekali.
    void* multi = jumlah > 0 ? bacaPtr(manajer, fieldMultiManajer) : nullptr;
    void* host = multi ? kunciHostDari(multi) : nullptr;
    void* sendiri = jumlah > 0 ? kunciPemainAman(bacaPtr(manajer, fieldPemainManajer)) : nullptr;

    PemainRingkas baru[kMaksPemain] = {};
    void* item = bacaPtr(daftar, fieldItemTamu);
    char* data = item ? dataArray(item) : nullptr;
    int terisi = 0;
    for (int32_t i = 0; data && i < jumlah && terisi < kMaksPemain; i++) {
        void* tamu = *reinterpret_cast<void**>(data + i * sizeof(void*));
        if (!tamu)
            continue;
        void* kunci = bacaPtr(tamu, fieldKunciTamu);
        teksDari(kunci, baru[terisi].kunci, kKarakterKunci, "");
        if (!baru[terisi].kunci[0])
            continue;
        salinNama(tamu, baru[terisi].nama, kKarakterNama, baru[terisi].kunci);
        if (samaString(kunci, host))
            snprintf(baru[terisi].peran, sizeof(baru[terisi].peran), "host");
        else if (samaString(kunci, sendiri))
            snprintf(baru[terisi].peran, sizeof(baru[terisi].peran), "kamu");
        else
            snprintf(baru[terisi].peran, sizeof(baru[terisi].peran), "tamu");
        terisi++;
    }

    pthread_mutex_lock(&kunciDaftar);
    memcpy(daftarPemain, baru, sizeof(PemainRingkas) * static_cast<size_t>(terisi));
    jumlahPemain = terisi;
    pthread_mutex_unlock(&kunciDaftar);
}

void jalankanAksi(void* manajerTamu, const Aksi& aksi) {
    char pesan[kKarakterNama + 64];
    if (aksi.jenis == JenisAksi::Tendang) {
        // Aksi sisa dari room sebelumnya tidak boleh mengirim paket: kunci host
        // hanya ada selama room berjalan.
        if (tamuDiRoom(manajerTamu) <= 0) {
            LOGE("Tendang menu dilewati: room sudah tidak ada");
            return;
        }
        void* multi = bacaPtr(bacaPtr(manajerTamu, fieldManajerTamu), fieldMultiManajer);
        bool terkirim = multi && tendangKunci(multi, stringDariUtf8(aksi.kunci));
        if (terkirim) {
            snprintf(pesan, sizeof(pesan), "Tendang terkirim: %s", aksi.nama);
            LOGI("Tendang menu: %s", aksi.kunci);
        } else {
            snprintf(pesan, sizeof(pesan), "Tendang gagal: %s", aksi.nama);
            LOGE("Tendang menu gagal: %s", aksi.kunci);
        }
        tampilkanToast(pesan);
        return;
    }

    int slot = 0;
    HasilSalin hasil = salinModelTamu(manajerTamu, aksi.kunci, aksi.nama, slot);
    switch (hasil) {
    case HasilSalin::Tersimpan:
        snprintf(pesan, sizeof(pesan), "Skin %s disalin ke slot %d", aksi.nama, slot);
        break;
    case HasilSalin::SudahAda:
        snprintf(pesan, sizeof(pesan), "Skin %s sudah ada di slot %d", aksi.nama, slot);
        break;
    case HasilSalin::TidakAda:
        snprintf(pesan, sizeof(pesan), "Skin %s belum tercatat, coba lagi", aksi.nama);
        break;
    default:
        snprintf(pesan, sizeof(pesan), "Salin skin %s gagal", aksi.nama);
        break;
    }
    tampilkanToast(pesan);
}

// Satu aksi per detak. tendang mengirim dua paket, jadi dua tendangan dalam satu
// frame hanya akan meloloskan yang pertama.
void detakAksi(void* manajerTamu) {
    double sekarang = sekarangDetik();
    if (sekarang < berikutAksi)
        return;

    Aksi aksi = {};
    bool ada = false;
    pthread_mutex_lock(&kunciAksi);
    if (jumlahAntrean > 0) {
        aksi = antrean[0];
        for (int i = 1; i < jumlahAntrean; i++)
            antrean[i - 1] = antrean[i];
        jumlahAntrean--;
        ada = true;
    }
    pthread_mutex_unlock(&kunciAksi);
    if (!ada)
        return;

    berikutAksi = sekarang + kJedaAksi;
    jalankanAksi(manajerTamu, aksi);
}

void hookUpdate(void* sendiri, void* metode) {
    auto asli = reinterpret_cast<void (*)(void*, void*)>(target(T_GUEST_UPDATE).orig);
    if (asli)
        asli(sendiri, metode);
    // Panggilan game yang keluar lewat jalur kirim akan melewati henti yang
    // berpasangan, dan penggantian yang bocor membuat setiap pencarian kunci
    // berikutnya dijawab dengan kunci host.
    hentiSpoof();
    detakAksi(sendiri);
    segarkanDaftar(sendiri);
}

bool antrekan(JenisAksi jenis, const char* kunci, const char* nama) {
    if (!siapMenu || !kunci || !*kunci || strlen(kunci) >= kKarakterKunci)
        return false;

    pthread_mutex_lock(&kunciAksi);
    bool diterima = jumlahAntrean < kBatasAksi;
    if (diterima) {
        Aksi& aksi = antrean[jumlahAntrean++];
        aksi.jenis = jenis;
        snprintf(aksi.kunci, sizeof(aksi.kunci), "%s", kunci);
        snprintf(aksi.nama, sizeof(aksi.nama), "%s", nama && *nama ? nama : kunci);
    }
    pthread_mutex_unlock(&kunciAksi);
    return diterima;
}

// Field daftar tamu dipakai section Room dan tombol tendang, jadi dipisahkan
// dari field chat: perubahan pada salah satunya tidak mematikan yang lain.
bool cariFieldMenu() {
    if (!cariField(kelasDari("Guest__Manager"), "a", fieldManajerTamu))
        return false;
    void* kelasManajer = kelasField(fieldManajerTamu);
    if (!cariField(kelasManajer, "guests", fieldTamuManajer) ||
        !cariField(kelasManajer, "multi", fieldMultiManajer) ||
        !cariField(kelasManajer, "player", fieldPemainManajer))
        return false;

    void* kelasDaftar = kelasField(fieldTamuManajer);
    if (!cariField(kelasDaftar, "_items", fieldItemTamu) ||
        !cariField(kelasDaftar, "_size", fieldUkuranTamu))
        return false;
    void* kelasTamu = kelasElemenField(fieldItemTamu);
    if (!cariField(kelasTamu, "key", fieldKunciTamu))
        return false;
    // Nama hanya dipakai daftar pemain di menu. Tanpa field ini daftar tetap
    // berjalan dengan kunci tamu sebagai label.
    if (cariField(kelasTamu, "account", fieldAkunTamu) &&
        cariField(kelasField(fieldAkunTamu), "user_Name", fieldNamaTamu))
        LOGI("Field nama tamu siap");
    else
        LOGE("Field nama tamu tidak ditemukan, memakai kunci tamu");
    if (!cariField(kelasField(fieldMultiManajer), "transfer", fieldTransferMulti))
        return false;
    return cariField(kelasField(fieldTransferMulti), "pipe", fieldPipeTransfer);
}

} // namespace

// Dipanggil dari menu di thread UI lewat JNI. Salinan diambil sekaligus supaya
// room yang berubah di tengah penulisan tidak membuat barisnya bercampur.
void salinDaftarPemain(char* keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0)
        return;
    keluaran[0] = '\0';
    daftarDiminta.store(sekarangDetik() + kMasaDaftar);

    PemainRingkas salinan[kMaksPemain] = {};
    int jumlah = 0;
    pthread_mutex_lock(&kunciDaftar);
    jumlah = jumlahPemain;
    memcpy(salinan, daftarPemain, sizeof(PemainRingkas) * static_cast<size_t>(jumlah));
    pthread_mutex_unlock(&kunciDaftar);

    size_t posisi = 0;
    for (int i = 0; i < jumlah; i++) {
        int ditulis = snprintf(keluaran + posisi, ukuran - posisi, "%s%s\t%s\t%s", i ? "\n" : "",
                               salinan[i].nama, salinan[i].kunci, salinan[i].peran);
        if (ditulis < 0 || static_cast<size_t>(ditulis) >= ukuran - posisi)
            break;
        posisi += static_cast<size_t>(ditulis);
    }
}

bool antrekanTendangPemain(const char* kunci, const char* nama) {
    return antrekan(JenisAksi::Tendang, kunci, nama);
}

bool antrekanSalinPemain(const char* kunci, const char* nama) {
    return antrekan(JenisAksi::Salin, kunci, nama);
}

void pasangHookRoom() {
    siapMenu = cariFieldMenu() && target(T_GUEST_UPDATE).addr && target(T_KICK_SEND).addr &&
               target(T_HOST_KEY).addr && target(T_PLAYER_KEY).orig;
    if (!siapMenu) {
        LOGE("Field daftar pemain tidak ditemukan; section Room tidak aktif");
        return;
    }
    LOGI("Field daftar pemain siap");
    // Satu hook update tamu menggerakkan antrean aksi dan salinan daftar, jadi
    // hanya ada satu hook untuk seluruh section.
    pasangHook(target(T_GUEST_UPDATE), reinterpret_cast<void*>(hookUpdate));
}
