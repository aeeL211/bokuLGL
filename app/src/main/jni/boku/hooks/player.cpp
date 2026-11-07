#include "hooks.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "player_service.h"
#include "targets.h"

namespace {

// Vector2 hanya dua float berurutan, ditulis langsung ke field struct milik game.
struct Vektor2 {
    float x;
    float y;
};

using FnDetect = void (*)(void*, void**, int32_t, float, float, void*);
using FnPosCheck = void (*)(void*, void**, void*);

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kLimitFree = 1.0e9f;
constexpr int32_t kLimitTeleport = 1024;

Field fieldActorPlayer = {};
Field localPlayerField = {};
Field fieldActorPhysics = {};
Field fieldDetectActor = {};

// Batas langkah yang dipakai Speed_Limit game. Nilainya sama dengan
// _Tile.pixel_Size - 1 dan juga dibaca kamera sebagai jarak maksimum sebelum
// bergerak, jadi hasil skala kecepatan harus tetap berada di dalamnya.
Field fieldLimitSpeed = {};
Field fieldLimitSpeedValue = {};

// Tinggi dunia dipakai untuk menerjemahkan Y menu ke Y UI game.
// Tile__Manager.Display__Y menghitung Y UI = Y tile - h/2, jadi
// Y tile = Y UI + h/2.
Field fieldActorTile = {};
Field worldSizeField = {};
Field fieldHeightWorld = {};

// Posisi tile milik game disimpan di Physics.pos, terpisah dari Transform Unity.
// Transform hanya gambar yang ditulis ulang tiap frame, jadi teleport menulis
// posisi fisika beserta seluruh titik jejaknya.
Field fieldPhysics = {};
Field fieldPosition = {};
Field fieldWorld = {};
Field fieldTile = {};
Field fieldTrail = {};
Field fieldTrailPrev = {};
Field fieldTrailCheck = {};
Field fieldTrailNew = {};
Field fieldTrailEnd = {};
Field fieldSegar = {};
Field fieldSpeed = {};
Field fieldSpeedX = {};
Field fieldSpeedY = {};
Field fieldSpeedAdd = {};
Field fieldSpeedAddX = {};
Field fieldSpeedAddY = {};

// Skala awal teleport diambil dari ukuran tile game, dengan collider pemain
// sebagai cadangan: size adalah ukuran dalam satuan dunia, sedangkan tile_Size
// adalah jumlah tile yang ditempati.
Field fieldCollider = {};
Field fieldSizeCollider = {};
Field fieldSizeTile = {};
Field fieldSizeTileW = {};
Field fieldSizeTileH = {};
Field fieldSizePixel = {};
// Nama _Tile dipakai dua kelas berbeda, jadi kelas yang punya pixel_Size
// diikat sekali saat hook dipasang. Pemindaian penuh tidak diulang tiap frame.
void* klassTile = nullptr;

// Tile bisa berupa Vector2 pecahan atau Vector2Int. Tipe field dibaca sekali
// saat hook dipasang supaya penulisan tidak menebak lebar tiap field.
bool tileRound = false;

// Margin adalah kotak gerak yang dihitung ulang tiap frame dari tile di sekitar
// badan. Sisi kiri, kanan, dan atas dibuka, sedangkan bawah dibiarkan supaya
// lantai tetap dipijak.
Field fieldMargin = {};
Field fieldMarginTop = {};
Field fieldMarginLeft = {};
Field fieldMarginRight = {};

// Pos.tile dan Pos.world memakai ruang koordinat berbeda. Selisih keduanya
// memberi satu skala yang berlaku untuk kedua sumbu, jadi teleport tidak perlu
// menebak titik nol Y dari rasio dunia/tile yang bisa meleset.
struct Kalibrasi {
    bool ready;
    bool pasti;
    bool fromMotion;
    float scale;
    int sample;
    float offsetX;
    float offsetY;
    bool hasReference;
    float referenceWorldX;
    float referenceWorldY;
    float referenceTileX;
    float referenceTileY;
};
Kalibrasi kalibrasi = {};

// Kecepatan dan gravitasi hanya mengubah fisika pemain lokal. Nilai asli game
// tidak pernah ditimpa permanen: pengali dipasang selama satu panggilan method
// game lalu nilainya dikembalikan seperti semula.
int logSpeed = -1;
int logGravity = -1;
int logHeightWorld = -1;

struct VerifikasiTeleport {
    int remaining;
    float tileX;
    float tileY;
    Vektor2 world;
};
VerifikasiTeleport verifikasi = {};

bool localPlayer(void* manager) {
    void* global = readPtr(manager, fieldActorPlayer);
    return global && readPtr(global, localPlayerField) == manager;
}

bool readVector(void* objek, const Field& field, Vektor2& value) {
    if (!objek || !field.field)
        return false;
    Field y = field;
    y.offset += sizeof(float);
    value.x = readFloat(objek, field);
    value.y = readFloat(objek, y);
    return std::isfinite(value.x) && std::isfinite(value.y);
}

void writeVector(void* objek, Field field, const Vektor2& value) {
    if (!objek || !field.field)
        return;
    writeFloat(objek, field, value.x);
    field.offset += sizeof(float);
    writeFloat(objek, field, value.y);
}

bool readTile(void* position, Vektor2& value) {
    if (!position || !fieldTile.field)
        return false;
    if (!tileRound)
        return readVector(position, fieldTile, value);
    Field y = fieldTile;
    y.offset += sizeof(int32_t);
    value.x = static_cast<float>(readInt(position, fieldTile));
    value.y = static_cast<float>(readInt(position, y));
    return std::isfinite(value.x) && std::isfinite(value.y);
}

// Tile bulat tidak boleh memakai pembulatan pustaka: nilainya ditulis ke field
// int32, jadi cukup geser setengah ke arah yang benar sebelum dipotong.
int32_t bulatkanTile(float value) {
    return static_cast<int32_t>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

void writeTile(void* position, float x, float y) {
    if (!position || !fieldTile.field)
        return;
    if (!tileRound) {
        Vektor2 value = {x, y};
        writeVector(position, fieldTile, value);
        return;
    }
    writeInt(position, fieldTile, bulatkanTile(x));
    Field fieldY = fieldTile;
    fieldY.offset += sizeof(int32_t);
    writeInt(position, fieldY, bulatkanTile(y));
}

// Tinggi dunia dalam tile. UI game menaruh Y 0 di tengah dunia, jadi
// Y UI = Y tile - h/2. Bila field belum terbaca, 0 berarti tanpa geseran dan
// Y tile dipakai apa adanya.
int32_t heightWorldTile(void* managerPlayer) {
    if (!managerPlayer || !fieldActorPlayer.field || !fieldActorTile.field ||
        !worldSizeField.field || !fieldHeightWorld.field)
        return 0;
    void* manager = readPtr(managerPlayer, fieldActorPlayer);
    void* tile = manager ? readPtr(manager, fieldActorTile) : nullptr;
    void* size = tile ? readPtr(tile, worldSizeField) : nullptr;
    int32_t height = size ? readInt(size, fieldHeightWorld) : 0;
    return height > 0 && height <= 100000 ? height : 0;
}

void storePosition(bool hasTile, const Vektor2& tile, const Vektor2& world) {
    player::storePosition(hasTile, tile.x, tile.y, world.x, world.y);
}

// Perpindahan pemain memberi pasangan (tile, dunia). Rasio keduanya adalah
// satuan dunia per tile, dan skalanya sama untuk kedua sumbu. Sampel baru
// dipakai setelah perpindahan minimal empat tile supaya pembulatan angka tile
// tidak mendominasi hasil.
bool addSample(float deltaTile, float deltaWorld, const char* axis) {
    if (fabsf(deltaTile) < 4.0f)
        return false;
    float kandidat = deltaWorld / deltaTile;
    if (!std::isfinite(kandidat) || kandidat < 0.05f || kandidat > 4096.0f)
        return false;
    if (!kalibrasi.fromMotion) {
        kalibrasi.scale = kandidat;
        kalibrasi.fromMotion = true;
        kalibrasi.sample = 1;
        LOGI("Teleport: skala %.2f dari gerakan %s", kandidat, axis);
        return true;
    }
    kalibrasi.scale = kalibrasi.scale * 0.8f + kandidat * 0.2f;
    kalibrasi.sample++;
    if (kalibrasi.sample % 8 == 0)
        LOGI("Teleport: skala %.2f dari %d sampel", kalibrasi.scale, kalibrasi.sample);
    return true;
}

// Skala awal teleport diambil dari ukuran tile game. Bila field itu belum
// terbaca, collider pemain menjadi cadangan: size adalah ukuran badan dalam
// satuan dunia, sedangkan tile_Size adalah jumlah tile yang ditempati. Gerakan
// pemain menggantikan tebakan collider pada sampel pertama.
void tryScaleInitial(void* physics) {
    if (kalibrasi.pasti || !physics)
        return;

    // _Tile.pixel_Size adalah ukuran satu tile dalam satuan dunia dan berlaku
    // untuk kedua sumbu, jadi nilainya tidak bergantung pada model pemain.
    if (klassTile && fieldSizePixel.field) {
        void* staticFlag = dataFieldStatic(klassTile);
        int32_t piksel = staticFlag ? readInt(staticFlag, fieldSizePixel) : 0;
        if (piksel > 0) {
            kalibrasi.scale = static_cast<float>(piksel);
            kalibrasi.ready = true;
            kalibrasi.pasti = true;
            LOGI("Teleport: skala awal %.2f dari pixel_Size", kalibrasi.scale);
            return;
        }
    }

    if (kalibrasi.ready)
        return;

    if (!fieldCollider.field || !fieldSizeCollider.field || !fieldSizeTile.field ||
        !fieldSizeTileW.field || !fieldSizeTileH.field)
        return;
    void* collider = readPtr(physics, fieldCollider);
    if (!collider)
        return;
    void* tileSize = readPtr(collider, fieldSizeTile);
    if (!tileSize)
        return;

    Vektor2 size = {};
    if (!readVector(collider, fieldSizeCollider, size))
        return;
    float tileW = readFloat(tileSize, fieldSizeTileW);
    float tileH = readFloat(tileSize, fieldSizeTileH);

    float kandidat = 0.0f;
    if (std::isfinite(tileW) && tileW > 0.01f && size.x > 0.01f)
        kandidat = size.x / tileW;
    else if (std::isfinite(tileH) && tileH > 0.01f && size.y > 0.01f)
        kandidat = size.y / tileH;
    if (!std::isfinite(kandidat) || kandidat < 0.05f || kandidat > 4096.0f)
        return;

    kalibrasi.scale = kandidat;
    kalibrasi.ready = true;
    LOGI("Teleport: skala awal %.2f dari collider", kandidat);
}

void perbaruiKalibrasi(const Vektor2& world, const Vektor2& tile) {
    if (!kalibrasi.hasReference) {
        kalibrasi.referenceWorldX = world.x;
        kalibrasi.referenceWorldY = world.y;
        kalibrasi.referenceTileX = tile.x;
        kalibrasi.referenceTileY = tile.y;
        float scale = kalibrasi.ready ? kalibrasi.scale : 1.0f;
        kalibrasi.offsetX = world.x - tile.x * scale;
        kalibrasi.offsetY = world.y - tile.y * scale;
        kalibrasi.hasReference = true;
        return;
    }

    // Skala dari pixel_Size sudah pasti, jadi sampel gerakan tidak boleh
    // menggantinya. Titik nol tetap diperbarui untuk jalur tanpa field tile.
    if (kalibrasi.pasti) {
        kalibrasi.offsetX = world.x - tile.x * kalibrasi.scale;
        kalibrasi.offsetY = world.y - tile.y * kalibrasi.scale;
        return;
    }

    float deltaTileX = tile.x - kalibrasi.referenceTileX;
    float deltaTileY = tile.y - kalibrasi.referenceTileY;
    if (fabsf(deltaTileX) < 0.5f && fabsf(deltaTileY) < 0.5f)
        return;

    bool accepted = false;
    if (fabsf(deltaTileX) >= fabsf(deltaTileY)) {
        accepted = addSample(deltaTileX, world.x - kalibrasi.referenceWorldX, "X");
    } else {
        accepted = addSample(deltaTileY, world.y - kalibrasi.referenceWorldY, "Y");
    }
    if (!accepted)
        return;

    kalibrasi.ready = true;
    kalibrasi.referenceWorldX = world.x;
    kalibrasi.referenceWorldY = world.y;
    kalibrasi.referenceTileX = tile.x;
    kalibrasi.referenceTileY = tile.y;
    kalibrasi.offsetX = world.x - tile.x * kalibrasi.scale;
    kalibrasi.offsetY = world.y - tile.y * kalibrasi.scale;
}

Vektor2 toWorld(float tileX, float tileY, const Vektor2& currentWorld, const Vektor2& tileNow,
                bool hasTileNow) {
    float scale = kalibrasi.ready ? kalibrasi.scale : 1.0f;
    // Titik acuan sekarang membatalkan selisih tetap antara tile dan dunia,
    // jadi rumus ini tetap benar walau titik nol sumbu Y belum pernah dihitung.
    if (hasTileNow) {
        Vektor2 result = {currentWorld.x + (tileX - tileNow.x) * scale,
                         currentWorld.y + (tileY - tileNow.y) * scale};
        return result;
    }
    Vektor2 result = {tileX * scale, tileY * scale};
    if (kalibrasi.hasReference) {
        result.x = kalibrasi.offsetX + tileX * scale;
        result.y = kalibrasi.offsetY + tileY * scale;
    }
    return result;
}

bool writePositionPhysics(void* position, float tileX, float tileY, const Vektor2& world) {
    if (!position || !fieldWorld.field)
        return false;

    writeVector(position, fieldWorld, world);
    writeTile(position, tileX, tileY);

    void* trail = readPtr(position, fieldTrail);
    if (trail) {
        writeVector(trail, fieldTrailPrev, world);
        writeVector(trail, fieldTrailCheck, world);
        writeVector(trail, fieldTrailNew, world);
        writeVector(trail, fieldTrailEnd, world);
    }
    // Titik acuan berikutnya adalah tujuan teleport, bukan titik lama yang
    // jaraknya bisa ribuan tile, supaya kalibrasi tidak memakai lompatan ini
    // sebagai sampel gerakan.
    kalibrasi.hasReference = true;
    kalibrasi.referenceWorldX = world.x;
    kalibrasi.referenceWorldY = world.y;
    kalibrasi.referenceTileX = tileX;
    kalibrasi.referenceTileY = tileY;
    float scale = kalibrasi.ready ? kalibrasi.scale : 1.0f;
    kalibrasi.offsetX = world.x - tileX * scale;
    kalibrasi.offsetY = world.y - tileY * scale;
    return true;
}

void clearSpeed(void* physics) {
    void* speed = readPtr(physics, fieldSpeed);
    if (!speed)
        return;
    writeFloat(speed, fieldSpeedX, 0.0f);
    writeFloat(speed, fieldSpeedY, 0.0f);
    // Tambahan kecepatan ikut dipakai Pos__Check pada frame berikutnya, jadi
    // sisa langkah sebelum teleport juga harus hilang.
    void* extra = readPtr(speed, fieldSpeedAdd);
    if (!extra)
        return;
    writeFloat(extra, fieldSpeedAddX, 0.0f);
    writeFloat(extra, fieldSpeedAddY, 0.0f);
}

// Game memakai flag ini untuk membangun ulang tile di sekitar badan, jadi tile
// tujuan langsung dipakai pada frame berikutnya.
void refreshTile(void* physics) {
    if (fieldSegar.field)
        writeBool(physics, fieldSegar, true);
}

void verifikasiTeleport(void* position) {
    if (verifikasi.remaining <= 0)
        return;
    verifikasi.remaining--;
    if (verifikasi.remaining > 0)
        return;

    Vektor2 world = {};
    Vektor2 tile = {};
    bool hasWorld = readVector(position, fieldWorld, world);
    bool hasTile = readTile(position, tile);
    if (!hasWorld) {
        LOGE("Teleport: posisi sesudah tidak terbaca");
        return;
    }
    if (hasTile) {
        LOGI("Teleport: sesudah tile=(%.2f, %.2f) dunia=(%.1f, %.1f) selisih=(%.2f, %.2f)/"
             "(%.1f, %.1f)",
             tile.x, tile.y, world.x, world.y, tile.x - verifikasi.tileX, tile.y - verifikasi.tileY,
             world.x - verifikasi.world.x, world.y - verifikasi.world.y);
    } else {
        LOGI("Teleport: sesudah dunia=(%.1f, %.1f) selisih=(%.1f, %.1f)", world.x, world.y,
             world.x - verifikasi.world.x, world.y - verifikasi.world.y);
    }
}

void applyTeleport(void* manager) {
    TeleportRequest& permintaan = teleportRequest();
    if (!permintaan.menunggu.exchange(false))
        return;

    void* physics = readPtr(manager, fieldPhysics);
    if (!physics) {
        LOGE("Teleport: fisika pemain tidak ditemukan");
        return;
    }
    void* position = readPtr(physics, fieldPosition);
    if (!position) {
        LOGE("Teleport: posisi fisika tidak terbaca");
        return;
    }

    int32_t tileX = permintaan.x.load();
    int32_t uiY = permintaan.y.load();
    if (tileX < -kLimitTeleport || tileX > kLimitTeleport || uiY < -kLimitTeleport ||
        uiY > kLimitTeleport) {
        LOGE("Teleport ditolak: X %d, Y UI %d di luar batas -%d..%d", tileX, uiY, kLimitTeleport,
             kLimitTeleport);
        return;
    }
    // Menu memakai Y UI game: 0 adalah tengah dunia, sama dengan
    // Tile__Manager.Display__Y (Y tile - h/2). Fisika memakai Y tile, jadi
    // Y UI digeser setengah tinggi dunia.
    int32_t tileY = uiY + heightWorldTile(manager) / 2;

    Vektor2 oldWorld = {};
    Vektor2 tileOld = {};
    bool hasWorld = readVector(position, fieldWorld, oldWorld);
    bool hasTile = readTile(position, tileOld);
    if (!hasWorld) {
        LOGE("Teleport ditolak: posisi dunia tidak terbaca");
        return;
    }
    tryScaleInitial(physics);
    if (hasTile) {
        LOGI("Teleport: sebelum tile=(%.2f, %.2f) dunia=(%.1f, %.1f)", tileOld.x, tileOld.y,
             oldWorld.x, oldWorld.y);
    } else {
        LOGI("Teleport: sebelum dunia=(%.1f, %.1f), tile tidak terbaca", oldWorld.x, oldWorld.y);
    }

    Vektor2 newWorld =
        toWorld(static_cast<float>(tileX), static_cast<float>(tileY), oldWorld, tileOld, hasTile);
    if (!std::isfinite(newWorld.x) || !std::isfinite(newWorld.y)) {
        LOGE("Teleport ditolak: hasil konversi bukan angka");
        return;
    }
    if (!kalibrasi.ready) {
        LOGI("Teleport: skala belum terkalibrasi, satuan tile dipakai apa adanya");
    }

    LOGI("Teleport: target UI=(%d, %d) tile=(%d, %d) dunia=(%.1f, %.1f) skala=%.2f "
         "offset=(%.2f, %.2f)",
         tileX, uiY, tileX, tileY, newWorld.x, newWorld.y,
         kalibrasi.ready ? kalibrasi.scale : 1.0f, kalibrasi.offsetX, kalibrasi.offsetY);
    if (!writePositionPhysics(position, static_cast<float>(tileX), static_cast<float>(tileY),
                           newWorld)) {
        LOGE("Teleport: posisi fisika tidak bisa ditulis");
        return;
    }
    clearSpeed(physics);
    refreshTile(physics);

    verifikasi.remaining = 3;
    verifikasi.tileX = static_cast<float>(tileX);
    verifikasi.tileY = static_cast<float>(tileY);
    verifikasi.world = newWorld;
}

float getMultiple(int32_t value, float minimum, float maksimum) {
    float result = static_cast<float>(value) / 10.0f;
    if (!std::isfinite(result))
        return 1.0f;
    if (result < minimum)
        return minimum;
    if (result > maksimum)
        return maksimum;
    return result;
}

// Speed_Limit game membatasi speed.add.x dan speed.add.y ke ±_speed.limit
// sebelum Pos__Check. Hasil skala mod memakai batas yang sama supaya langkah
// tidak pernah melewati satu tile; kalau lewat, border dan tile di antaranya
// tidak sempat diperbarui dan render ikut rusak.
float stepLimit(void* managerPhysics) {
    return readFloat(readPtr(managerPhysics, fieldLimitSpeed), fieldLimitSpeedValue);
}

float clampStep(float value, float limit) {
    if (!std::isfinite(limit) || limit <= 0.0f)
        return value;
    if (value > limit)
        return limit;
    if (value < -limit)
        return -limit;
    return value;
}

// Physics__Manager.a menunjuk Manager, dan Manager.player adalah pemain lokal.
// Pos__Check dipanggil untuk setiap badan fisika, jadi hook hanya menyentuh
// badan yang sama dengan Player__Manager.phy milik pemain lokal.
bool physicsOwnedPlayer(void* managerPhysics, void* physics) {
    void* global = readPtr(managerPhysics, fieldActorPhysics);
    void* player = readPtr(global, localPlayerField);
    return player && readPtr(player, fieldPhysics) == physics;
}

// Pos__Check hanya menghitung _world.check = pos.world + speed.add. Semua
// gerakan pemain, termasuk terbang, berenang, dan meluncur, lewat titik ini,
// jadi kecepatan X dan komponen turun Y diskalakan di sini lalu dikembalikan.
// Komponen naik tidak disentuh supaya lompatan tetap sama.
void hookPosCheck(void* own, void** physics, void* method) {
    auto original = reinterpret_cast<FnPosCheck>(target(T_POS_CHECK).orig);
    if (!original)
        return;

    float multiple = getMultiple(features().speedPlayer.load(), 0.5f, 5.0f);
    float rasio = getMultiple(features().gravityPlayer.load(), 0.0f, 3.0f);
    void* objek = physics ? *physics : nullptr;
    if ((multiple == 1.0f && rasio == 1.0f) || !objek || !physicsOwnedPlayer(own, objek)) {
        original(own, physics, method);
        return;
    }

    void* speed = readPtr(objek, fieldSpeed);
    void* extra = speed ? readPtr(speed, fieldSpeedAdd) : nullptr;
    if (!extra) {
        original(own, physics, method);
        return;
    }
    float originalX = readFloat(extra, fieldSpeedAddX);
    float originalY = readFloat(extra, fieldSpeedAddY);
    if (!std::isfinite(originalX) || !std::isfinite(originalY)) {
        original(own, physics, method);
        return;
    }

    float limit = stepLimit(own);
    if (multiple != 1.0f)
        writeFloat(extra, fieldSpeedAddX, clampStep(originalX * multiple, limit));
    // Hanya gerak turun yang mengikuti slider gravitasi. Dorongan naik dari
    // lompatan dibiarkan, jadi 0,0x membuat pemain melayang.
    if (rasio != 1.0f && originalY < 0.0f)
        writeFloat(extra, fieldSpeedAddY, clampStep(originalY * rasio, limit));

    original(own, physics, method);

    writeFloat(extra, fieldSpeedAddX, originalX);
    writeFloat(extra, fieldSpeedAddY, originalY);
}

// Kotak gerak dari tile dipakai pemain saat melangkah, jadi sisi kiri, kanan,
// dan atas dibuka selebar mungkin. Sisi bawah dibiarkan supaya lantai tetap
// dipijak. Hanya entri milik `waktu` yang diubah: setiap panggilan _Set
// menghitung ulang satu entri itu, sedangkan entri lain dipakai fase fisika
// lain dan harus tetap berisi kotak gerak aslinya. Mengubah semua entri membuat
// tembus dinding tetap aktif setelah sakelar dimatikan karena entri yang tidak
// pernah dihitung ulang menyimpan nilai bebas.
void freeWall(void* physics, int32_t clock) {
    void* margin = physics ? readPtr(physics, fieldMargin) : nullptr;
    if (!margin)
        return;
    int32_t count = lengthArray(margin);
    if (clock < 0 || clock >= count)
        return;
    char* data = arrayData(margin);
    void* entry = *reinterpret_cast<void**>(data + static_cast<size_t>(clock) * sizeof(void*));
    if (!entry)
        return;
    writeFloat(entry, fieldMarginTop, kLimitFree);
    writeFloat(entry, fieldMarginLeft, -kLimitFree);
    writeFloat(entry, fieldMarginRight, kLimitFree);
}

// Deteksi fisika berjalan untuk setiap badan di dunia, jadi hanya badan yang
// sama dengan fisika pemain lokal yang dibebaskan.
bool physicsPlayerOwn(void* detect, void** physics) {
    void* global = readPtr(detect, fieldDetectActor);
    void* manager = readPtr(global, localPlayerField);
    void* objek = physics ? *physics : nullptr;
    return manager && objek && readPtr(manager, fieldPhysics) == objek;
}

// Dipasang tepat setelah kotak gerak dihitung dari tile dan sebelum pemain
// memakainya untuk melangkah, jadi dinding hilang pada frame yang sama.
void hookDetectPhysics(void* own, void** physics, int32_t clock, float posX, float posY,
                       void* method) {
    auto original = reinterpret_cast<FnDetect>(target(T_DETECT_SET).orig);
    if (!original)
        return;
    original(own, physics, clock, posX, posY, method);
    if (!features().noClip || !physicsPlayerOwn(own, physics))
        return;
    freeWall(physics ? *physics : nullptr, clock);
}

void checkTileType() {
    if (!fieldTile.field)
        return;
    void* klass = fieldClass(fieldTile);
    const char* name = klass ? apiIl2cpp().className(klass) : nullptr;
    tileRound = name && strstr(name, "Int") != nullptr;
    LOGI("Teleport: field tile %s (%s)", name ? name : "tanpa nama",
         tileRound ? "bilangan bulat" : "pecahan");
}

bool findFieldPhysics(void* managerClass) {
    if (!findField(managerClass, "phy", fieldPhysics))
        return false;
    void* klassPhysics = fieldClass(fieldPhysics);
    if (!findField(klassPhysics, "pos", fieldPosition) ||
        !findField(klassPhysics, "speed", fieldSpeed) ||
        !findField(klassPhysics, "refresh", fieldSegar) ||
        !findField(klassPhysics, "margin", fieldMargin))
        return false;

    void* klassPosition = fieldClass(fieldPosition);
    if (!findField(klassPosition, "world", fieldWorld) ||
        !findField(klassPosition, "_world", fieldTrail))
        return false;
    // Tile adalah posisi yang dipakai fisika untuk mencari tabrakan. Tanpa field
    // ini teleport kembali menulis world saja dan bisa ditarik ke tile lama.
    if (!findField(klassPosition, "tile", fieldTile))
        LOGE("Field tile tidak ditemukan, teleport memakai world saja");
    checkTileType();

    void* klassTrail = fieldClass(fieldTrail);
    if (!findField(klassTrail, "prev", fieldTrailPrev) ||
        !findField(klassTrail, "check", fieldTrailCheck) ||
        !findField(klassTrail, "new_", fieldTrailNew) ||
        !findField(klassTrail, "end", fieldTrailEnd))
        return false;

    void* klassSpeed = fieldClass(fieldSpeed);
    if (!findField(klassSpeed, "x", fieldSpeedX) ||
        !findField(klassSpeed, "y", fieldSpeedY))
        return false;

    // add dipakai Pos__Check untuk menghitung langkah berikutnya. Bila field
    // ini hilang, hook kecepatan dilewati tetapi teleport dan No Clip tetap
    // berjalan.
    if (!findField(klassSpeed, "add", fieldSpeedAdd) ||
        !findField(fieldClass(fieldSpeedAdd), "x", fieldSpeedAddX) ||
        !findField(fieldClass(fieldSpeedAdd), "y", fieldSpeedAddY)) {
        fieldSpeedAdd = {};
        fieldSpeedAddX = {};
        fieldSpeedAddY = {};
        LOGE("Field speed.add tidak ditemukan; Kecepatan dan Gravitasi pemain nonaktif");
    } else {
        LOGI("Field kecepatan tambahan siap");
    }

    // Collider hanya dipakai untuk menebak skala awal teleport. Bila salah satu
    // field tidak ada, teleport tetap memakai skala dari gerakan pemain.
    if (findField(klassPhysics, "collider", fieldCollider) &&
        findField(fieldClass(fieldCollider), "size", fieldSizeCollider) &&
        findField(fieldClass(fieldCollider), "tile_Size", fieldSizeTile) &&
        findField(fieldClass(fieldSizeTile), "w", fieldSizeTileW) &&
        findField(fieldClass(fieldSizeTile), "h", fieldSizeTileH)) {
        LOGI("Field collider siap");
    } else {
        fieldSizeCollider = {};
        fieldSizeTile = {};
        fieldSizeTileW = {};
        fieldSizeTileH = {};
        LOGE("Field collider tidak lengkap; skala awal teleport dilewati");
    }
    return true;
}

bool findFieldMargin() {
    void* klassMargin = fieldElementClass(fieldMargin);
    return findField(klassMargin, "top", fieldMarginTop) &&
           findField(klassMargin, "left", fieldMarginLeft) &&
           findField(klassMargin, "right", fieldMarginRight);
}

// Manager.tile._world_Size.h adalah tinggi dunia dalam tile, sumber yang sama
// dengan Tile__Manager.Display__Y saat daftar zona belum siap. Field ini dipakai
// untuk menerjemahkan Y menu ke Y UI game; bila hilang, Y tile dipakai apa
// adanya dan hanya muncul peringatan di log.
bool findFieldWorld() {
    if (!findField(fieldClass(fieldActorPlayer), "tile", fieldActorTile) ||
        !findField(fieldClass(fieldActorTile), "_world_Size", worldSizeField) ||
        !findField(fieldClass(worldSizeField), "h", fieldHeightWorld)) {
        fieldActorTile = {};
        worldSizeField = {};
        fieldHeightWorld = {};
        LOGE("Field tinggi dunia tidak lengkap; Titik Y memakai tile apa adanya");
        return false;
    }
    LOGI("Field tinggi dunia siap");
    return true;
}

} // namespace

// Field pembanding pemain lokal dicari sebelum hook fitur dipasang, supaya hook
// yang berjalan untuk semua pemain sudah bisa membedakan badan tamu sejak
// panggilan pertama. Dipanggil juga oleh pasangHookPemain.
void findLocalPlayerFields() {
    void* managerClass = classOf("Player__Manager");
    findField(managerClass, "a", fieldActorPlayer);
    findField(fieldClass(fieldActorPlayer), "player", localPlayerField);
}

// Pembanding untuk hook yang berjalan pada state milik semua pemain. Selama
// field pembanding belum siap, fitur dibiarkan berjalan seperti sebelumnya agar
// tidak mati diam-diam ketika nama field berubah.
bool isLocalPlayerHook(void* manager) {
    if (!manager)
        return false;
    if (!fieldActorPlayer.field || !localPlayerField.field)
        return true;
    return localPlayer(manager);
}

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// fisika game tidak boleh disentuh dari thread menu.
void onPlayerUpdate(void* manager) {
    if (!fieldActorPlayer.field || !localPlayerField.field || !fieldPhysics.field)
        return;
    if (!localPlayer(manager))
        return;

    void* physics = readPtr(manager, fieldPhysics);
    void* position = physics ? readPtr(physics, fieldPosition) : nullptr;
    int32_t height = heightWorldTile(manager);
    if (height != logHeightWorld) {
        logHeightWorld = height;
        if (height > 0)
            LOGI("Titik Y UI: tinggi dunia %d tile, Y UI 0 di tile %d", height, height / 2);
        else
            LOGI("Titik Y UI: tinggi dunia tidak terbaca, Y tile dipakai apa adanya");
    }
    if (position) {
        Vektor2 world = {};
        Vektor2 tile = {};
        bool hasWorld = readVector(position, fieldWorld, world);
        bool hasTile = readTile(position, tile);
        if (hasWorld && player::servePosition()) {
            Vektor2 tileUi = tile;
            tileUi.y -= static_cast<float>(height) * 0.5f;
            storePosition(hasTile, tileUi, world);
        }
        if (hasWorld && hasTile) {
            tryScaleInitial(physics);
            perbaruiKalibrasi(world, tile);
        }
        verifikasiTeleport(position);
    }

    applyTeleport(manager);

    int speedValue = features().speedPlayer.load();
    if (speedValue != logSpeed) {
        logSpeed = speedValue;
        LOGI("Kecepatan pemain: %.1fx", getMultiple(speedValue, 0.5f, 5.0f));
    }
    int gravityValue = features().gravityPlayer.load();
    if (gravityValue != logGravity) {
        logGravity = gravityValue;
        LOGI("Gravitasi pemain: %.1fx", getMultiple(gravityValue, 0.0f, 3.0f));
    }
}

void installHookMotion() {
    // Pos__Check dipanggil untuk semua badan fisika, jadi butuh Physics__Manager.a
    // untuk mengenali pemain lokal. Kecepatan dan gravitasi memakai titik yang sama,
    // dan batas langkahnya dibaca dari Physics__Manager._speed.limit milik game.
    void* klassPhysicsManager = findClass("Physics__Manager");
    if (!findField(klassPhysicsManager, "a", fieldActorPhysics))
        LOGE("Field fisika manajer tidak ditemukan; Kecepatan dan Gravitasi pemain nonaktif");
    else if (!fieldSpeedAdd.field || !fieldSpeedAddX.field || !fieldSpeedAddY.field)
        LOGE("Field speed.add tidak lengkap; Kecepatan dan Gravitasi pemain nonaktif");
    else {
        if (findField(klassPhysicsManager, "_speed", fieldLimitSpeed) &&
            findField(fieldClass(fieldLimitSpeed), "limit", fieldLimitSpeedValue))
            LOGI("Field batas kecepatan siap");
        else {
            fieldLimitSpeed = {};
            fieldLimitSpeedValue = {};
            LOGE("Field batas kecepatan tidak ditemukan; langkah mengikuti batas game saja");
        }
        installHook(target(T_POS_CHECK), reinterpret_cast<void*>(hookPosCheck));
    }
}

void installPlayerHook() {
    findLocalPlayerFields();
    void* managerClass = classOf("Player__Manager");
    if (!fieldActorPlayer.field || !localPlayerField.field) {
        LOGE("Field pemain tidak lengkap, Teleport, Kecepatan, Gravitasi, dan No Clip nonaktif");
        return;
    }
    if (!findFieldPhysics(managerClass)) {
        LOGE("Field fisika pemain tidak lengkap, Teleport, Kecepatan, dan No Clip nonaktif");
        return;
    }
    LOGI("Field pemain OK");

    findFieldWorld();

    klassTile = findKlassField("_Tile", "pixel_Size");
    if (klassTile && findField(klassTile, "pixel_Size", fieldSizePixel))
        LOGI("Field ukuran tile siap");
    else
        LOGE("Field pixel_Size tidak ditemukan; skala awal teleport memakai collider");

    installHookMotion();

    void* klassDetect = findClass("Physics__Detect");
    bool readyWall = findField(klassDetect, "a", fieldDetectActor) && findFieldMargin();
    if (!readyWall || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    installHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDetectPhysics));
}
