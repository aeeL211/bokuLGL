#pragma once

#include <cstddef>

// Data yang dibagi antara thread game dan thread menu. Thread game menulis
// salinan kecil di sini, thread menu hanya membaca salinan itu; tidak ada objek
// IL2CPP yang pernah menyeberang.
namespace player {

constexpr int kMaxPlayer = 16;
constexpr int kLimitAction = 8;
constexpr std::size_t kCharacterKey = 40;
constexpr std::size_t kCharacterName = 96;

struct Summary {
    char name[kCharacterName];
    char key[kCharacterKey];
    char role[8];
};

struct Action {
    enum class Kind { Kick, KickPermanent, KickHost, KickAll, Copy };

    Kind jenis;
    char key[kCharacterKey];
    char name[kCharacterName];
};

// Ditulis sekali oleh hook room setelah field daftar tamu ditemukan.
void setReady(bool ready);

// Jendela pembaruan daftar pemain. Mengembalikan true bila thread game boleh
// menyusun salinan baru sekarang; pemanggil yang menentukan isinya.
bool serveList();
void storeList(const Summary* data, int count);

// Baris "nama<TAB>kunci<TAB>peran", dipisah newline. Aman dari thread UI.
void copyList(char* keluaran, std::size_t size);

// Antrean aksi menu. Antrean berbatas dan tidak memblokir thread UI.
bool enqueueKick(const char* key, const char* name, bool permanent);
bool enqueueKickHost(const char* name);
bool enqueueKickAll();
bool enqueueCopy(const char* key, const char* name);
bool takeAction(Action& exit);

// Jendela pembaruan posisi pemain lokal.
bool servePosition();
void storePosition(bool hasTile, float tileX, float tileY, float worldX, float worldY);

// Baris "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY"; bagian tile kosong bila game
// tidak menyediakan field tile.
void copyPosition(char* keluaran, std::size_t size);

} // namespace pemain
