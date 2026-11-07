#pragma once

#include <cstddef>

void installIapHook();
void installMultiplayerHook();
void installChatHook();
void installSensitiveHook();
void installTextInputHook();
void installProfileHook();
void installPasswordHook();
void installSkinRangeHook();
void installAboutHook();
void installFoodHook();
void installSkinHook();
void installPrivacyHook();
void installWeaponHook();
void installEffectsHook();
void installKnockbackHook();
void installFecesHook();
void installHandHook();
void installSlotHook();
void installSpoofHook();
void installRoomHook();
void installPlayerHook();

// Bernilai benar bila manajer pemain ini adalah pemain lokal. Dipakai hook yang
// berjalan untuk semua pemain supaya fitur tidak menyentuh badan tamu. Selama
// field pembanding belum siap, hasilnya benar agar fitur tidak mati diam-diam.
bool isLocalPlayerHook(void* manager);

// Mencari dua field pembanding pemain lokal. Dipanggil sebelum hook fitur
// dipasang, lalu diulang oleh pasangHookPemain.
void findLocalPlayerFields();

// Dipanggil dari hook update pemain, di thread game. Menangani Teleport dan
// No Clip hanya untuk pemain lokal.
void onPlayerUpdate(void* manager);

// Paket yang hanya diterima host dikirim dengan kunci host milik room, dan
// penggantiannya hanya hidup selama satu panggilan kirim.
void startSpoof(void* multi);
void stopSpoof();
void* hostKeyFrom(void* multi);

// Kunci pemain sendiri, dibaca hanya setelah room punya objek photon. Di luar
// room hasilnya nullptr, karena getter game melempar NullReferenceException.
void* safePlayerKey(void* player);

// Bernilai benar selama tombol interaksi ditahan untuk pemain ini.
bool isActionButtonPressed(void* manager);
void findActionFields();

// Dinaikkan oleh fitur yang menahan itemnya di tangan padahal panggilan stok
// akan mengosongkan slot tangan.
void setHoldHand(bool hold);

// Mengembalikan id yang terakhir dipegang ke tangan setelah animasi makan
// menyembunyikan sprite-nya, jadi makanan yang ditahan tetap terlihat.
void refreshHand();

// Hasil salin skin satu tamu. Slot hanya diisi untuk Tersimpan dan SudahAda.
enum class CopyResult { Failed, Saved, Exists, Missing };

// Menulis model tamu ke slot kosong pertama, beserta nama pemiliknya. Model
// diambil dari cache Guest__Manager.Add/Init__Model, lalu dari model hidup tamu
// bila cache belum terisi. Pemanggilnya hanya menyebut kunci dan nama tamu.
// Dipanggil di thread game.
CopyResult copyGuestModel(void* managerGuest, const char* key, const char* name, int& slot);

// Salinan daftar pemain, antrean aksi, dan posisi pemain lokal berada di
// boku/player_service.h. Hook room hanya mengisi salinan itu, dan menu hanya
// membacanya.
