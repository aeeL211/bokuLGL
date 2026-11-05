#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Vector3 adalah tiga float berurutan, jadi arm64 mengembalikannya di s0-s2 dan
// menerimanya di s0-s2, sama seperti method IL2CPP yang dibangkitkan game.
struct Vektor3 {
    float x;
    float y;
    float z;
};

using FnTransform = void* (*)(void*, void*);
using FnAmbilPosisi = Vektor3 (*)(void*, void*);
using FnSetPosisi = void (*)(void*, Vektor3, void*);
using FnGetComponent = void* (*)(void*, void*, void*);
using FnAmbilBool = bool (*)(void*, void*);
using FnSetBool = void (*)(void*, bool, void*);

Field fieldManajerGlobal = {};
Field fieldPemainLokal = {};
Field fieldRoot = {};

// Komponen Unity pada objek akar pemain, dicari sekali. Game ini memakai fisika
// tile sendiri, jadi bila pemain tidak membawa Collider2D maupun Rigidbody2D,
// No Clip hanya mencatatnya dan tidak menyentuh apa pun.
enum class Komponen { TidakAda, Collider, Rigidbody };

Komponen jenisKomponen = Komponen::TidakAda;
void* komponenNoClip = nullptr;
// Objek akar tempat komponen itu ditemukan. Pindah dunia membangun ulang objek
// pemain, jadi komponen lama tidak boleh dipakai lagi.
void* rootKomponen = nullptr;
bool komponenDicari = false;
bool nilaiAwal = true;
bool sedangNonaktif = false;

// System.Type Collider2D dan Rigidbody2D disiapkan saat start, di thread pekerja,
// supaya frame pertama No Clip tidak perlu memindai seluruh assembly.
void* tipeCollider = nullptr;
void* tipeRigidbody = nullptr;

bool pemainLokal(void* manajer) {
    void* global = bacaPtr(manajer, fieldManajerGlobal);
    return global && bacaPtr(global, fieldPemainLokal) == manajer;
}

void terapkanTeleport(void* manajer) {
    PermintaanTeleport& permintaan = permintaanTeleport();
    if (!permintaan.menunggu.exchange(false))
        return;

    void* root = bacaPtr(manajer, fieldRoot);
    auto ambilTransform = reinterpret_cast<FnTransform>(target(T_GAMEOBJECT_TRANSFORM).addr);
    auto ambilPosisi = reinterpret_cast<FnAmbilPosisi>(target(T_TRANSFORM_GET_POSITION).addr);
    auto setPosisi = reinterpret_cast<FnSetPosisi>(target(T_TRANSFORM_SET_POSITION).addr);
    if (!root || !ambilTransform || !ambilPosisi || !setPosisi) {
        LOGE("Teleport: Transform pemain tidak siap");
        return;
    }

    void* transform = ambilTransform(root, target(T_GAMEOBJECT_TRANSFORM).method);
    if (!transform) {
        LOGE("Teleport: Transform pemain tidak ditemukan");
        return;
    }

    Vektor3 posisi = ambilPosisi(transform, target(T_TRANSFORM_GET_POSITION).method);
    posisi.x = static_cast<float>(permintaan.x.load());
    posisi.y = static_cast<float>(permintaan.y.load());
    setPosisi(transform, posisi, target(T_TRANSFORM_SET_POSITION).method);
    LOGI("Teleport ke %.0f, %.0f", posisi.x, posisi.y);
}

void* cariKomponen(void* root, void* tipe) {
    auto getComponent = reinterpret_cast<FnGetComponent>(target(T_GAMEOBJECT_GET_COMPONENT).addr);
    if (!root || !tipe || !getComponent)
        return nullptr;
    return getComponent(root, tipe, target(T_GAMEOBJECT_GET_COMPONENT).method);
}

void cariKomponenNoClip(void* root) {
    komponenDicari = true;

    void* collider = tipeCollider ? cariKomponen(root, tipeCollider) : nullptr;
    if (collider) {
        auto ambilBool = reinterpret_cast<FnAmbilBool>(target(T_BEHAVIOUR_GET_ENABLED).addr);
        if (ambilBool) {
            jenisKomponen = Komponen::Collider;
            komponenNoClip = collider;
            nilaiAwal = ambilBool(collider, target(T_BEHAVIOUR_GET_ENABLED).method);
            LOGI("No Clip: Collider2D pemain siap");
            return;
        }
    }

    void* rigidbody = tipeRigidbody ? cariKomponen(root, tipeRigidbody) : nullptr;
    if (rigidbody) {
        auto ambilBool = reinterpret_cast<FnAmbilBool>(target(T_RIGIDBODY2D_GET_KINEMATIC).addr);
        if (ambilBool) {
            jenisKomponen = Komponen::Rigidbody;
            komponenNoClip = rigidbody;
            nilaiAwal = ambilBool(rigidbody, target(T_RIGIDBODY2D_GET_KINEMATIC).method);
            LOGI("No Clip: Rigidbody2D pemain siap");
            return;
        }
    }

    LOGE("No Clip: pemain tidak membawa Collider2D atau Rigidbody2D Unity");
}

void terapkanNoClip(void* manajer) {
    void* root = bacaPtr(manajer, fieldRoot);
    if (!root)
        return;

    // Komponen milik objek pemain sebelumnya sudah dibebaskan bersama objeknya,
    // jadi pencarian diulang dan sakelar diterapkan lagi ke objek yang baru.
    if (root != rootKomponen) {
        rootKomponen = root;
        komponenDicari = false;
        komponenNoClip = nullptr;
        jenisKomponen = Komponen::TidakAda;
        sedangNonaktif = false;
        nilaiAwal = true;
    }
    if (!komponenDicari)
        cariKomponenNoClip(root);
    if (!komponenNoClip)
        return;

    bool mauNonaktif = fitur().noClip;
    if (mauNonaktif == sedangNonaktif)
        return;

    if (jenisKomponen == Komponen::Collider) {
        auto setBool = reinterpret_cast<FnSetBool>(target(T_BEHAVIOUR_SET_ENABLED).addr);
        if (!setBool)
            return;
        setBool(komponenNoClip, mauNonaktif ? false : nilaiAwal,
                target(T_BEHAVIOUR_SET_ENABLED).method);
    } else {
        auto setBool = reinterpret_cast<FnSetBool>(target(T_RIGIDBODY2D_SET_KINEMATIC).addr);
        if (!setBool)
            return;
        setBool(komponenNoClip, mauNonaktif ? true : nilaiAwal,
                target(T_RIGIDBODY2D_SET_KINEMATIC).method);
    }
    sedangNonaktif = mauNonaktif;
}

} // namespace

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// komponen Unity tidak boleh disentuh dari thread menu.
void saatUpdatePemain(void* manajer) {
    if (!fieldManajerGlobal.field || !fieldPemainLokal.field || !fieldRoot.field)
        return;
    if (!pemainLokal(manajer))
        return;
    terapkanNoClip(manajer);
    terapkanTeleport(manajer);
}

void pasangHookPemain() {
    void* kelasManajer = kelasDari("Player__Manager");
    bool ok = cariField(kelasManajer, "a", fieldManajerGlobal) &&
              cariField(kelasField(fieldManajerGlobal), "player", fieldPemainLokal) &&
              cariField(kelasManajer, "root", fieldRoot);
    if (!ok) {
        LOGE("Field pemain tidak lengkap, Teleport dan No Clip nonaktif");
        return;
    }
    tipeCollider = tipeKelas(cariKelas("Collider2D"));
    tipeRigidbody = tipeKelas(cariKelas("Rigidbody2D"));
    if (!tipeCollider && !tipeRigidbody)
        LOGE("Kelas Collider2D dan Rigidbody2D tidak ditemukan");
    LOGI("Field pemain OK");
}
