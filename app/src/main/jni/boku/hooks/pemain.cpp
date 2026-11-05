#include "hooks.h"

#include <cstdint>

#include "fitur.h"
#include "hook.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// Vector2 hanya dua float berurutan, ditulis langsung ke field struct milik game.
struct Vektor2 {
    float x;
    float y;
};

using FnDeteksi = void (*)(void*, void**, int32_t, float, float, void*);

// Batas yang dipakai No Clip. Nilainya jauh di luar ukuran dunia mana pun, jadi
// pemeriksaan tabrakan tidak pernah menyempitkannya lagi.
constexpr float kBatasBebas = 1.0e9f;

Field fieldAktorPemain = {};
Field fieldPemainLokal = {};
Field fieldDeteksiAktor = {};

// Posisi tile milik game disimpan di Physics.pos, terpisah dari Transform Unity.
// Transform hanya gambar yang ditulis ulang tiap frame, jadi teleport menulis
// posisi fisika beserta seluruh titik jejaknya.
Field fieldFisika = {};
Field fieldPosisi = {};
Field fieldDunia = {};
Field fieldJejak = {};
Field fieldJejakPrev = {};
Field fieldJejakCheck = {};
Field fieldJejakBaru = {};
Field fieldJejakAkhir = {};
Field fieldSegar = {};
Field fieldKecepatan = {};
Field fieldKecepatanX = {};
Field fieldKecepatanY = {};

// Margin adalah kotak gerak yang dihitung ulang tiap frame dari tile di sekitar
// badan. Sisi kiri, kanan, dan atas dibuka, sedangkan bawah dibiarkan supaya
// lantai tetap dipijak.
Field fieldMargin = {};
Field fieldMarginAtas = {};
Field fieldMarginKiri = {};
Field fieldMarginKanan = {};

bool pemainLokal(void* manajer) {
    void* global = bacaPtr(manajer, fieldAktorPemain);
    return global && bacaPtr(global, fieldPemainLokal) == manajer;
}

void tulisVektor(void* objek, Field bidang, const Vektor2& nilai) {
    if (!objek || !bidang.field)
        return;
    tulisFloat(objek, bidang, nilai.x);
    bidang.offset += sizeof(float);
    tulisFloat(objek, bidang, nilai.y);
}

// Posisi fisika ditulis langsung ke titik tujuan. Game menyalin
// Physics.pos._world.end ke Physics.pos.world di akhir tiap update, jadi semua
// titik jejak ikut dipindah agar teleport tidak ditarik kembali ke posisi lama.
bool tulisPosisiFisika(void* fisika, float x, float y, Vektor2& akhir) {
    void* posisi = bacaPtr(fisika, fieldPosisi);
    if (!posisi || !fieldDunia.field)
        return false;

    akhir.x = x;
    akhir.y = y;
    tulisVektor(posisi, fieldDunia, akhir);

    void* jejak = bacaPtr(posisi, fieldJejak);
    if (jejak) {
        tulisVektor(jejak, fieldJejakPrev, akhir);
        tulisVektor(jejak, fieldJejakCheck, akhir);
        tulisVektor(jejak, fieldJejakBaru, akhir);
        tulisVektor(jejak, fieldJejakAkhir, akhir);
    }

    void* kecepatan = bacaPtr(fisika, fieldKecepatan);
    if (kecepatan) {
        tulisFloat(kecepatan, fieldKecepatanX, 0.0f);
        tulisFloat(kecepatan, fieldKecepatanY, 0.0f);
    }
    // Game memakai flag ini untuk membangun ulang tile di sekitar badan, jadi
    // tile tujuan langsung dipakai pada frame berikutnya.
    if (fieldSegar.field)
        tulisBool(fisika, fieldSegar, true);
    return true;
}

void terapkanTeleport(void* manajer) {
    PermintaanTeleport& permintaan = permintaanTeleport();
    if (!permintaan.menunggu.exchange(false))
        return;

    void* fisika = bacaPtr(manajer, fieldFisika);
    if (!fisika) {
        LOGE("Teleport: fisika pemain tidak ditemukan");
        return;
    }

    float tujuanX = static_cast<float>(permintaan.x.load());
    float tujuanY = static_cast<float>(permintaan.y.load());
    Vektor2 akhir = {};
    if (!tulisPosisiFisika(fisika, tujuanX, tujuanY, akhir)) {
        LOGE("Teleport: posisi fisika tidak terbaca");
        return;
    }
    LOGI("Teleport ke %.0f, %.0f (fisika %.0f, %.0f)", tujuanX, tujuanY, akhir.x, akhir.y);
}

// Kotak gerak dari tile dipakai pemain saat melangkah, jadi sisi kiri, kanan,
// dan atas dibuka selebar mungkin. Sisi bawah dibiarkan supaya lantai tetap
// dipijak.
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
        tulisFloat(entri, fieldMarginAtas, kBatasBebas);
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
    if (!cariField(kelasManajer, "phy", fieldFisika))
        return false;
    void* kelasFisika = kelasField(fieldFisika);
    if (!cariField(kelasFisika, "pos", fieldPosisi) ||
        !cariField(kelasFisika, "speed", fieldKecepatan) ||
        !cariField(kelasFisika, "refresh", fieldSegar) ||
        !cariField(kelasFisika, "margin", fieldMargin))
        return false;

    void* kelasPosisi = kelasField(fieldPosisi);
    if (!cariField(kelasPosisi, "world", fieldDunia) ||
        !cariField(kelasPosisi, "_world", fieldJejak))
        return false;

    void* kelasJejak = kelasField(fieldJejak);
    if (!cariField(kelasJejak, "prev", fieldJejakPrev) ||
        !cariField(kelasJejak, "check", fieldJejakCheck) ||
        !cariField(kelasJejak, "new_", fieldJejakBaru) ||
        !cariField(kelasJejak, "end", fieldJejakAkhir))
        return false;

    void* kelasKecepatan = kelasField(fieldKecepatan);
    return cariField(kelasKecepatan, "x", fieldKecepatanX) &&
           cariField(kelasKecepatan, "y", fieldKecepatanY);
}

bool cariFieldMargin() {
    void* kelasMargin = kelasElemenField(fieldMargin);
    return cariField(kelasMargin, "top", fieldMarginAtas) &&
           cariField(kelasMargin, "left", fieldMarginKiri) &&
           cariField(kelasMargin, "right", fieldMarginKanan);
}

} // namespace

// Dipanggil di akhir update pemain lokal, di thread game, karena posisi dan
// fisika game tidak boleh disentuh dari thread menu.
void saatUpdatePemain(void* manajer) {
    if (!fieldAktorPemain.field || !fieldPemainLokal.field || !fieldFisika.field)
        return;
    if (!pemainLokal(manajer))
        return;
    terapkanTeleport(manajer);
}

void pasangHookPemain() {
    void* kelasManajer = kelasDari("Player__Manager");
    if (!cariField(kelasManajer, "a", fieldAktorPemain) ||
        !cariField(kelasField(fieldAktorPemain), "player", fieldPemainLokal)) {
        LOGE("Field pemain tidak lengkap, Teleport dan No Clip nonaktif");
        return;
    }
    if (!cariFieldFisika(kelasManajer)) {
        LOGE("Field fisika pemain tidak lengkap, Teleport dan No Clip nonaktif");
        return;
    }
    LOGI("Field pemain OK");

    void* kelasDeteksi = cariKelas("Physics__Detect");
    bool siapDinding = cariField(kelasDeteksi, "a", fieldDeteksiAktor) && cariFieldMargin();
    if (!siapDinding || !target(T_DETECT_SET).addr) {
        LOGE("Field tembus dinding tidak lengkap; No Clip nonaktif");
        return;
    }
    LOGI("Field tembus dinding siap");
    pasangHook(target(T_DETECT_SET), reinterpret_cast<void*>(hookDeteksiFisika));
}
