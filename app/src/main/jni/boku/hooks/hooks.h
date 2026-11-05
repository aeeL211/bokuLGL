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
void pasangHookAkun();
void pasangHookSenjata();
void pasangHookEfek();
void pasangHookTerpental();
void pasangHookKotoran();
void pasangHookTangan();
void pasangHookSlot();
void pasangHookSpoof();
void pasangHookTendang();
void pasangHookRoom();
void pasangHookPemain();

// Dipanggil dari hook update pemain, di thread game. Menangani Teleport dan
// No Clip hanya untuk pemain lokal.
void saatUpdatePemain(void* manajer);

// Paket yang hanya diterima host dikirim dengan kunci host milik room, dan
// penggantiannya hanya hidup selama satu panggilan kirim.
void mulaiSpoof(void* multi);
void hentiSpoof();
void* kunciHostDari(void* multi);

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
// berasal dari cache yang diisi hook Guest__Manager.Add, jadi pemanggilnya hanya
// menyebut kunci dan nama tamu. Dipanggil di thread game.
HasilSalin salinModelTamu(void* manajerTamu, const char* kunci, const char* nama, int& slot);

// Daftar tamu untuk section Room di menu, ditulis sebagai baris
// "nama<TAB>kunci" yang dipisah newline. Aman dipanggil dari thread UI.
void salinDaftarPemain(char* keluaran, size_t ukuran);

// Mengantrekan satu aksi dari menu. Aksi dijalankan dari hook update tamu, di
// thread game, satu per detak supaya paket tidak bertabrakan.
bool antrekanTendangPemain(const char* kunci, const char* nama);
bool antrekanSalinPemain(const char* kunci, const char* nama);
