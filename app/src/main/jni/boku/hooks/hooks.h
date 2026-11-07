#pragma once

#include <cstddef>

void pasangHookIap();
void pasangHookMultiplayer();
void pasangHookChat();
void pasangHookSensitif();
void pasangHookInputTeks();
void pasangHookProfil();
void pasangHookSandi();
void pasangHookRentangSkin();
void pasangHookTentang();
void pasangHookMakanan();
void pasangHookSkin();
void pasangHookPrivasi();
void pasangHookSenjata();
void pasangHookEfek();
void pasangHookTerpental();
void pasangHookKotoran();
void pasangHookTangan();
void pasangHookSlot();
void pasangHookSpoof();
void pasangHookRoom();
void pasangHookPemain();

// Bernilai benar bila manajer pemain ini adalah pemain lokal. Dipakai hook yang
// berjalan untuk semua pemain supaya fitur tidak menyentuh badan tamu. Selama
// field pembanding belum siap, hasilnya benar agar fitur tidak mati diam-diam.
bool pemainLokalHook(void* manajer);

// Mencari dua field pembanding pemain lokal. Dipanggil sebelum hook fitur
// dipasang, lalu diulang oleh pasangHookPemain.
void cariFieldPemainLokal();

// Dipanggil dari hook update pemain, di thread game. Menangani Teleport dan
// No Clip hanya untuk pemain lokal.
void saatUpdatePemain(void* manajer);

// Paket yang hanya diterima host dikirim dengan kunci host milik room, dan
// penggantiannya hanya hidup selama satu panggilan kirim.
void mulaiSpoof(void* multi);
void hentiSpoof();
void* kunciHostDari(void* multi);

// Kunci pemain sendiri, dibaca hanya setelah room punya objek photon. Di luar
// room hasilnya nullptr, karena getter game melempar NullReferenceException.
void* kunciPemainAman(void* pemain);

// Bernilai benar selama tombol interaksi ditahan untuk pemain ini.
bool tombolAksiDitekan(void* manajer);
void cariFieldAksi();

// Dinaikkan oleh fitur yang menahan itemnya di tangan padahal panggilan stok
// akan mengosongkan slot tangan.
void setelTahanTangan(bool tahan);

// Mengembalikan id yang terakhir dipegang ke tangan setelah animasi makan
// menyembunyikan sprite-nya, jadi makanan yang ditahan tetap terlihat.
void segarkanTangan();

// Hasil salin skin satu tamu. Slot hanya diisi untuk Tersimpan dan SudahAda.
enum class HasilSalin { Gagal, Tersimpan, SudahAda, TidakAda };

// Menulis model tamu ke slot kosong pertama, beserta nama pemiliknya. Model
// diambil dari cache Guest__Manager.Add/Init__Model, lalu dari model hidup tamu
// bila cache belum terisi. Pemanggilnya hanya menyebut kunci dan nama tamu.
// Dipanggil di thread game.
HasilSalin salinModelTamu(void* manajerTamu, const char* kunci, const char* nama, int& slot);

// Daftar pemain untuk section Room di menu, ditulis sebagai baris
// "nama<TAB>kunci<TAB>peran" yang dipisah newline. Perannya host, kamu, atau
// tamu. Aman dipanggil dari thread UI.
void salinDaftarPemain(char* keluaran, size_t ukuran);

// Posisi pemain lokal untuk baris menu, ditulis sebagai
// "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY". Bagian tile kosong bila game tidak
// punya field tile. Aman dipanggil dari thread UI.
void salinPosisiPemain(char* keluaran, size_t ukuran);

// Mengantrekan satu aksi dari menu. Aksi dijalankan dari hook update tamu, di
// thread game, satu per detak supaya paket tidak bertabrakan.
bool antrekanTendangPemain(const char* kunci, const char* nama);
bool antrekanSalinPemain(const char* kunci, const char* nama);
