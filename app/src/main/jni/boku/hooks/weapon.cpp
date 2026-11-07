#include "hooks.h"

#include <cstdint>
#include <ctime>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldManagerWeapon = {};
Field fieldWeaponCmn = {};
Field fieldManagerState = {};
Field fieldManagerCmn = {};
Field fieldCmnHh = {};
Field fieldState = {};
Field fieldCombo = {};
Field fieldRock = {};
Field fieldCharge = {};
// Penghitung charge runtime adalah jumlah step 32-bit yang dibandingkan
// pencarian id buster dengan step penuh dari konfigurasi.
Field fieldStep = {};
Field fieldHhRock = {};
Field fieldConfigCharge = {};
Field fieldStepFull = {};

// Tombol yang ditahan menembak sekali per jendela. Pistol dan buster bawaan
// sama-sama menunggu timer aksi yang panjang, dan buster bisa menghabiskan
// interval tembaknya sebelum state akhir mulai, setelah itu tombol yang ditahan
// berhenti menembak.
constexpr long long kRhythmPistolMs = 80;
constexpr long long kActionBusterMs = 60;
constexpr long long kRhythmBusterMs = 100;

// Setiap pemain menjalankan state machine yang sama, jadi iramanya mengikuti
// manajer pemain. Satu jam bersama akan membuat tamu mereset tepi tombol lokal.
struct Rhythm {
    void* manager;
    long long shootPistolMs;
    long long shootBusterMs;
    bool ditahan;
};
constexpr int kCountRhythm = 32;
Rhythm rhythmList[kCountRhythm] = {};
int rhythmNext = 0;

long long nowMs() {
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<long long>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
}

Rhythm& rhythmFor(void* manager) {
    Rhythm* slotKosong = nullptr;
    for (Rhythm& rhythm : rhythmList) {
        if (rhythm.manager == manager)
            return rhythm;
        if (!rhythm.manager && !slotKosong)
            slotKosong = &rhythm;
    }

    Rhythm& slot = slotKosong ? *slotKosong : rhythmList[rhythmNext];
    rhythmNext = (rhythmNext + 1) % kCountRhythm;
    slot = {};
    slot.manager = manager;
    return slot;
}

// Bernilai benar paling sering sekali per jendela, jadi tombol yang ditahan
// menembak dengan laju tetap.
bool rhythmReady(long long& lastMs, long long rhythmMs) {
    long long now = nowMs();
    if (now - lastMs < rhythmMs)
        return false;
    lastMs = now;
    return true;
}

void* statePlayer(void* own, const Field& managerField) {
    void* manager = readPtr(own, managerField);
    return readPtr(manager, fieldState);
}

// State machine senjata yang sama juga dijalankan untuk tamu di dalam room.
// Semua hook di bawah hanya boleh mempercepat pemain lokal, jadi manajer pemain
// diperiksa lebih dulu dan jalur bawaan dipakai untuk badan tamu.
bool weaponLocal(void* manager) { return isLocalPlayerHook(manager); }

// Update state mengulang tembakan hanya selama flag combo-nya menyala, dan flag
// itu mengikuti tombol yang ditahan. Menulisnya sebelum update bawaan membuat
// semua gaya berdiri maupun bergerak ikut mengulang.
void selaraskanCombo(void* manager) {
    void* state = readPtr(manager, fieldState);
    if (state)
        writeBool(state, fieldCombo, isActionButtonPressed(manager));
}

// Tekanan tombol memulai tembakan bawaan, jadi irama dihitung dari sini dan
// pengulangan paksa pertama jatuh satu jendela kemudian, bukan seketika.
void hookUpdateState(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_STATE_UPDATE).orig);
    if (!original)
        return;

    if (!features().shootAutomatic) {
        original(own, method);
        return;
    }

    void* manager = readPtr(own, fieldManagerState);
    if (!weaponLocal(manager)) {
        original(own, method);
        return;
    }
    Rhythm& rhythm = rhythmFor(manager);
    bool ditahan = isActionButtonPressed(manager);
    if (ditahan && !rhythm.ditahan) {
        rhythm.shootPistolMs = nowMs();
        rhythm.shootBusterMs = rhythm.shootPistolMs;
    }
    rhythm.ditahan = ditahan;
    original(own, method);
}

// State akhir pistol adalah timer tetap. Mengakhirinya pada update pertama
// menghilangkan jeda antar tembakan; fase aksi tetap memutar muzzle dan peluru.
bool hookEndPistol(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_GUN_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic || !weaponLocal(readPtr(own, fieldManagerCmn)))
        return original(own, method);
    return true;
}

// Aksi pistol bawaan hanya berakhir setelah frame kedua plus timer aksi.
// Menjawab ya pada irama pendek selama tombol ditahan menembak lagi sebelum
// timer itu membiarkan jalur bawaan lewat.
bool hookEndActionPistol(void* own, int image, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, int, void*)>(target(T_GUN_ACTION_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, image, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, image, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, image, method)) {
        if (isActionButtonPressed(manager))
            rhythm.shootPistolMs = nowMs();
        return true;
    }
    if (!isActionButtonPressed(manager))
        return false;
    return rhythmReady(rhythm.shootPistolMs, kRhythmPistolMs);
}

// Jaga flag combo tetap mutakhir selama aksi pistol, lalu jalankan update bawaan.
void hookUpdatePistol(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_GUN_UPDATE).orig);
    if (!original)
        return;

    void* manager = readPtr(own, fieldManagerWeapon);
    if (features().shootAutomatic && weaponLocal(manager))
        selaraskanCombo(manager);
    original(own, method);
}

// Penyelerasan yang sama selama aksi buster.
void hookUpdateBuster(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_ROCK_UPDATE).orig);
    if (!original)
        return;

    void* manager = readPtr(own, fieldManagerWeapon);
    if (features().shootAutomatic && weaponLocal(manager))
        selaraskanCombo(manager);
    original(own, method);
}

// Setiap gaya mengakhiri aksi buster lewat pengecekan ini, yang menunggu timer
// aksi panjang. Mengakhirinya satu jendela aksi setelah tembakan mulai
// mempertahankan frame muzzle dan efek sambil membuang sisa penantiannya.
bool hookEndActionBuster(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_ROCK_ACTION_END).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, method))
        return true;
    if (!isActionButtonPressed(manager))
        return false;
    return nowMs() - rhythm.shootBusterMs >= kActionBusterMs;
}

// Gaya bergerak meninggalkan state akhir buster lewat pengecekan ini, yang
// menyerah begitu interval tembakan lewat. Menjawab ya pada irama membuat
// tembakan terus berjalan selama tombol ditahan.
bool hookEndToActionBuster(void* own, void* method) {
    auto original = reinterpret_cast<bool (*)(void*, void*)>(target(T_ROCK_END_TO_ACTION).orig);
    if (!original)
        return false;
    if (!features().shootAutomatic)
        return original(own, method);

    void* manager = readPtr(own, fieldManagerCmn);
    if (!weaponLocal(manager))
        return original(own, method);
    Rhythm& rhythm = rhythmFor(manager);
    if (original(own, method)) {
        if (isActionButtonPressed(manager))
            rhythm.shootBusterMs = nowMs();
        return true;
    }
    if (!isActionButtonPressed(manager))
        return false;
    return rhythmReady(rhythm.shootBusterMs, kRhythmBusterMs);
}

// Buster berdiri menembak ulang di dalam update akhirnya sendiri. Menahan update
// itu sampai iramanya jatuh tempo lalu memulai aksinya menjaga laju yang sama,
// dan jalur bawaan tetap berjalan setiap kali tombol dilepas.
void hookUpdateEndBuster(void* own, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_ROCK_END_UPDATE).orig);
    if (!original)
        return;
    if (!features().shootAutomatic) {
        original(own, method);
        return;
    }

    auto directionEnd = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_ROCK_END_DIR).addr);
    auto action = reinterpret_cast<void (*)(void*, int32_t, void*)>(target(T_ROCK_ACTION).addr);
    void* manager = readPtr(own, fieldManagerWeapon);
    void* cmn = readPtr(own, fieldWeaponCmn);
    if (!directionEnd || !action || !cmn || !weaponLocal(manager) || !isActionButtonPressed(manager)) {
        original(own, method);
        return;
    }

    Rhythm& rhythm = rhythmFor(manager);
    if (!rhythmReady(rhythm.shootBusterMs, kRhythmBusterMs))
        return;

    action(own, directionEnd(cmn, target(T_ROCK_END_DIR).method), target(T_ROCK_ACTION).method);
}

// Pencarian id hanya mengembalikan proyektil terisi bila step charge runtime
// sama dengan step penuh dari konfigurasi. Menulis step itu sebelum pencarian
// membuat setiap tembakan memakai amunisi terkuat.
int32_t hookIdBuster(void* own, void* method) {
    auto original = reinterpret_cast<int32_t (*)(void*, void*)>(target(T_ROCK_ID).orig);
    if (!original)
        return 0;
    if (!features().busterFull)
        return original(own, method);
    if (!weaponLocal(readPtr(own, fieldManagerCmn)))
        return original(own, method);

    void* state = statePlayer(own, fieldManagerCmn);
    void* rock = readPtr(state, fieldRock);
    void* charge = readPtr(rock, fieldCharge);
    void* hh = readPtr(own, fieldCmnHh);
    void* config = readPtr(hh, fieldHhRock);
    void* chargeConfig = readPtr(config, fieldConfigCharge);
    if (charge && chargeConfig)
        writeInt(charge, fieldStep, readInt(chargeConfig, fieldStepFull));

    return original(own, method);
}

bool findFieldWeapon() {
    void* cmn = classOf("Player__State__Cmn");
    void* weapon = classOf("Player__State__Std__Action__Hh__Weapon");
    void* managerState = classOf("Player__State");
    return findField(managerState, "m", fieldManagerState) &&
           findField(weapon, "m", fieldManagerWeapon) &&
           findField(weapon, "cmn", fieldWeaponCmn) && findField(cmn, "m", fieldManagerCmn) &&
           findField(cmn, "chr_Hh", fieldCmnHh) &&
           findField(fieldClass(fieldManagerWeapon), "__state", fieldState) &&
           findField(fieldClass(fieldState), "combo", fieldCombo) &&
           findField(fieldClass(fieldState), "_rockman__Rock_Buster", fieldRock) &&
           findField(fieldClass(fieldRock), "charge", fieldCharge) &&
           findField(fieldClass(fieldCharge), "step", fieldStep) &&
           findField(fieldClass(fieldCmnHh), "_rockman__Rock_Buster", fieldHhRock) &&
           findField(fieldClass(fieldHhRock), "charge", fieldConfigCharge) &&
           findField(fieldClass(fieldConfigCharge), "step__Full", fieldStepFull);
}

} // namespace

void installWeaponHook() {
    if (!findFieldWeapon()) {
        LOGE("Field senjata tidak ditemukan; senjata tetap vanilla");
        return;
    }
    LOGI("Field senjata siap");

    if (!target(T_ROCK_ACTION).addr || !target(T_ROCK_END_DIR).addr)
        LOGE("Method tembak ulang buster tidak ditemukan; buster berdiri tetap vanilla");

    installHook(target(T_STATE_UPDATE), reinterpret_cast<void*>(hookUpdateState));
    installHook(target(T_GUN_END), reinterpret_cast<void*>(hookEndPistol));
    installHook(target(T_GUN_ACTION_END), reinterpret_cast<void*>(hookEndActionPistol));
    installHook(target(T_GUN_UPDATE), reinterpret_cast<void*>(hookUpdatePistol));
    installHook(target(T_ROCK_UPDATE), reinterpret_cast<void*>(hookUpdateBuster));
    installHook(target(T_ROCK_ACTION_END), reinterpret_cast<void*>(hookEndActionBuster));
    installHook(target(T_ROCK_END_UPDATE), reinterpret_cast<void*>(hookUpdateEndBuster));
    installHook(target(T_ROCK_END_TO_ACTION), reinterpret_cast<void*>(hookEndToActionBuster));
    installHook(target(T_ROCK_ID), reinterpret_cast<void*>(hookIdBuster));
}
