#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
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

// Vector2 hanya dua float berurutan, ditulis langsung ke field struct milik game.
struct Vektor2 {
    float x;
    float y;
};

using FnTransform = void* (*)(void*, void*);
using FnAmbilPosisi = Vektor3 (*)(void*, void*);
using FnSetPosisi = void (*)(void*, Vektor3, void*);
using FnDeteksi = void (*)(void*, void**, int32_t, float, float, void*);

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kBatasBebas = 1.0e9f;

Field fieldAktorPemain = {};
Field fieldPemainLokal = {};
Field fieldDeteksiAktor = {};
Field fieldRoot = {};

// Fisika tile milik game menyimpan posisi sendiri, terpisah dari Transform
// Unity. Transform hanya gambar yang ditulis ulang tiap frame, jadi teleport
// harus menggeser posisi fisika dengan selisih yang sama.
Field fieldFisika = {};
Field fieldPosisi = {};
Field fieldDunia = {};
Field fieldJejak = {};
Field fieldJejakPrev = {};
Field fieldJejakCheck = {};
Field fieldKecepatan = {};
Field fieldKecepatanX = {};
Field fieldKecepatanY = {};

// Margin adalah kotak gerak yang dihitung ulang tiap frame dari tile di sekitar
// badan. Sisi kiri dan kanannya yang dilonggarkan, sedangkan atas dan bawah
// dibiarkan supaya lantai tetap dipijak.
Field fieldMargin = {};
Field fieldMarginKiri = {};
Field fieldMarginKanan = {};

bool pemainLokal(void* manajer) {
    void* global = bacaPtr(manajer, fieldAktorPemain);
    return global && bacaPtr(global, fieldPemainLokal) == manajer;
}

Vektor2 bacaVektor(void* objek, Field bidang) {
    Vektor2 hasil = {};
    if (!objek || !bidang.field)
        return hasil;
    hasil.x = bacaFloat(objek, bidang);
    bidang.offset += sizeof(float);
    hasil.y = bacaFloat(objek, bidang);
    return hasil;
}

void tulisVektor(void* objek, Field bidang, const Vektor2& nilai) {
    if (!objek || !bidang.field)
        return;
    tulisFloat(objek, bidang, nilai.x);
    bidang.offset += sizeof(float);
    tulisFloat(objek, bidang, nilai.y);
}

// Posisi fisika digeser sebesar selisih transform lama dan titik tujuan, lalu
// kecepatan serta jejak posisi dibersihkan supaya pemain tidak langsung
// terlempar kembali.
bool geserFisika(void* manajer, float geserX, float geserY, Vektor2& akhir) {
    void* fisika = bacaPtr(manajer, fieldFisika);
    void* posisi = fisika ? bacaPtr(fisika, fieldPosisi) : nullptr;
    if (!posisi || !fieldDunia.field)
        return false;

    akhir = bacaVektor(posisi, fieldDunia);
    akhir.x += geserX;
    akhir.y += geserY;
    tulisVektor(posisi, fieldDunia, akhir);

    void* kecepatan = bacaPtr(fisika, fieldKecepatan);
    if (kecepatan) {
        tulisFloat(kecepatan, fieldKecepatanX, 0.0f);
        tulisFloat(kecepatan, fieldKecepatanY, 0.0f);
    }

    void* jejak = bacaPtr(posisi, fieldJejak);
    if (jejak) {
        tulisVektor(jejak, fieldJejakPrev, akhir);
        tulisVektor(jejak, fieldJejakCheck, akhir);
    }
    return true;
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

    Vektor3 sekarang = ambilPosisi(transform, target(T_TRANSFORM_GET_POSITION).method);
    Vektor3 tujuan = {static_cast<float>(permintaan.x.load()),
                      static_cast<float>(permintaan.y.load()), sekarang.z};
    Vektor2 fisika = {};
    bool ikut = geserFisika(manajer, tujuan.x - sekarang.x, tujuan.y - sekarang.y, fisika);
    setPosisi(transform, tujuan, target(T_TRANSFORM_SET_POSITION).method);

    if (ikut)
        LOGI("Teleport ke %.0f, %.0f (fisika %.0f, %.0f)", tujuan.x, tujuan.y, fisika.x, fisika.y);
    else
        LOGI("Teleport ke %.0f, %.0f (posisi fisika tidak terbaca)", tujuan.x, tujuan.y);
}

// Kotak gerak dari tile dipakai pemain saat melangkah, jadi sisi kiri dan
// kanannya dibuka selebar mungkin. Lantai dan langit-langit tetap dihitung.
void bebaskanDinding(void* fisika) {
    void* margin = fisika ? bacaPtr(fisika, fieldMargin) : nullptr;
    if (!margin)
        return;
    int32_t jumlah = panjangArray(margin);
    char* data = dataArray(margin);
    for (int32_t i = 0; i < jumlah; i++) {
        void* entri = *reinterpret_cast<void**>(data + static_cast<size_t>(i) * sizeof(void*));
        if (!entri)
            continue;
        tulisFloat(entri, fieldMarginKiri, -kBatasBebas);
        tulisFloat(entri, fieldMarginKanan, kBatasBebas);
    }
}

// Deteksi fisika berjalan untuk setiap badan di dunia, jadi hanya badan yang
// sama dengan fisika pemain lokal yang dibebaskan.
bool fisikaPemainSendiri(void* deteksi, void** fisika) {
    void* global = bacaPtr(deteksi, fieldDeteksiAktor);
    void* manajer = bacaPtr(global, fieldPemainLokal);
    void* objek = fisika ? *fisika : nullptr;
    return manajer && objek && bacaPtr(manajer, fieldFisika) == objek;
}

// Dipasang tepat setelah kotak gerak dihitung dari tile dan sebelum pemain
// memakainya untuk melangkah, jadi dinding hilang pada frame yang sama.
void hookDeteksiFisika(void* sendiri, void** fisika, int32_t waktu, float posX, float posY,
                       void* metode) {
    auto asli = reinterpret_cast<FnDeteksi>(target(T_DETECT_SET).orig);
    if (!asli)
        return;
    asli(sendiri, fisika, waktu, posX, posY, metode);
    if (!fitur().noClip || !fisikaPemainSendiri(sendiri, fisika))
        return;
    bebaskanDinding(fisika ? *fisika : nullptr);
}

bool cariFieldFisika(void* kelasManajer) {
    return cariField(kelasManajer, "phy", fieldFisika) &&
           cariField(kelasField(fieldFisika), "pos", fieldPosisi) &&
           cariField(kelasField(fieldPosisi), "world", fieldDunia) &&
           cariField(kelasField(fieldPosisi), "_world", fieldJejak) &&
           cariField(kelasField(fieldJejak), "prev", fieldJejakPrev) &&
           cariField(kelasField(fieldJejak), "check", fieldJejakCheck) &&
           cariField(kelasField(fieldFisika), "speed", fieldKecepatan) &&
           cariField(kelasField(fieldKecepatan), "x", fieldKecepatanX) &&
           cariField(kelasField(fieldKecepatan), "y", fieldKecepatanY);
}

bool cariFieldMargin(void* kelasFisika) {
    return cariField(kelasFisika, "margin", fieldMargin) &&
           cariField(kelasElemenField(fieldMargin), "left", fieldMarginKiri) &&
           cariField(kelasElemenField(fieldMargin), "right", fieldMarginKanan);
}

} // namespace

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// fisika game tidak boleh disentuh dari thread menu.
void saatUpdatePemain(void* manajer) {
    if (!fieldAktorPemain.field || !fieldPemainLokal.field || !fieldRoot.field)
        return;
    if (!pemainLokal(manajer))
        return;
    terapkanTeleport(manajer);
}

void pasangHookPemain() {
    void* kelasManajer = kelasDari("Player__Manager");
    bool siap = cariField(kelasManajer, "a", fieldAktorPemain) &&
                cariField(kelasField(fieldAktorPemain), "player", fieldPemainLokal) &&
                cariField(kelasManajer, "root", fieldRoot);
    if (!siap) {
        LOGE("Field pemain tidak lengkap, Teleport nonaktif");
        return;
    }
    if (cariFieldFisika(kelasManajer))
        LOGI("Field pemain OK");
    else
        LOGE("Field fisika pemain tidak lengkap, Teleport hanya menggeser gambar");

    void* kelasDeteksi = cariKelas("Physics__Detect");
    void* kelasFisika = kelasField(fieldFisika);
    bool siapDinding = kelasFisika && cariField(kelasDeteksi, "a", fieldDeteksiAktor) &&
                       cariFieldMargin(kelasFisika);
    if (!siapDinding || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    pasangHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDeteksiFisika));
}
