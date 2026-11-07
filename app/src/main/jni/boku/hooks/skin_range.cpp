#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

Field fieldMPos = {};
Field fieldPosMain = {};
Field fieldStepMax = {};
Field fieldStepHalf = {};
Field fieldWidthStep = {};
Field fieldWidthPos = {};
Field fieldValuePos = {};
Field fieldValueMin = {};
Field fieldValueMax = {};
Field fieldValuePerStep = {};
Field fieldMPortrait = {};
Field fieldPosPortrait = {};

constexpr int kMaxAxis = 8;
constexpr int kSlotRange = 2;
int32_t skinBase[kSlotRange][kMaxAxis] = {};
int32_t skinDiperlebar[kSlotRange][kMaxAxis] = {};
int32_t skinLimit[kSlotRange][kMaxAxis] = {};

// Refresh__Step mengatur ulang slider X/Y ke -5..+5 lalu menurunkan steps_Half,
// steps_W, dan per_Step dari steps_Max. Hanya sumbu yang masih -5..+5 yang
// dilebarkan (sumbu rotasi dibiarkan) dan nilai turunannya disusun ulang supaya
// Value__To__Step melihat rentang -batas..+batas pada lintasan Refresh() yang
// sama. Batas datang dari slider menu: 5 berarti tidak berubah, 25 adalah
// bawaan lama, dan 100 adalah geseran terjauh.
void widenRange(void* self, const Field& fieldOwner, const Field& fieldPos, int slot) {
    int32_t limit = features().skinRange.load();
    if (limit < 5)
        limit = 5;
    if (limit > 100)
        limit = 100;
    if (!fieldOwner.field || !fieldPos.field || !fieldStepMax.field ||
        !fieldStepHalf.field || !fieldWidthStep.field || !fieldWidthPos.field ||
        !fieldValuePos.field || !fieldValueMin.field || !fieldValueMax.field ||
        !fieldValuePerStep.field) {
        return;
    }

    void* owner = readPtr(self, fieldOwner);
    void* pos = readPtr(owner, fieldPos);
    if (!pos)
        return;

    void* valueArr = readPtr(pos, fieldValuePos);
    void* maxStep = readPtr(pos, fieldStepMax);
    void* halfStep = readPtr(pos, fieldStepHalf);
    void* stepWidth = readPtr(pos, fieldWidthStep);
    if (!valueArr || !maxStep || !halfStep || !stepWidth)
        return;

    int32_t countValue = lengthArray(valueArr);
    int32_t countMax = lengthArray(maxStep);
    int32_t countHalf = lengthArray(halfStep);
    int32_t countWidth = lengthArray(stepWidth);
    int32_t count = countValue < countMax ? countValue : countMax;
    if (count > kMaxAxis)
        count = kMaxAxis;
    if (count <= 0)
        return;

    float widthPos = readFloat(pos, fieldWidthPos);

    for (int32_t i = 0; i < count; i++) {
        void* value = *reinterpret_cast<void**>(arrayData(valueArr) + i * sizeof(void*));
        if (!value)
            continue;

        float minLimit = readFloat(value, fieldValueMin);
        float maxLimit = readFloat(value, fieldValueMax);
        bool stillDefault = nearValue(minLimit, -5.0f) && nearValue(maxLimit, 5.0f);
        // Sebagian jalur Refresh mengatur ulang min/max ke -5..+5, sebagian
        // lagi mempertahankan nilai yang sudah ditulis. Keduanya diterima supaya
        // batas baru tetap terpakai pada lintasan mana pun.
        bool bekasKita = skinLimit[slot][i] != 0 &&
                         nearValue(minLimit, -static_cast<float>(skinLimit[slot][i])) &&
                         nearValue(maxLimit, static_cast<float>(skinLimit[slot][i]));
        if (!stillDefault && !bekasKita)
            continue;

        char* addressMax = arrayData(maxStep) + i * sizeof(int32_t);
        int32_t* ptrMax = reinterpret_cast<int32_t*>(addressMax);
        int32_t now = *ptrMax;
        if (now <= 0) {
            LOGE("Rentang skin: sumbu %d punya steps_Max=%d, dilewati", i, now);
            continue;
        }

        // steps_Max bisa sedang bernilai dasar (sesudah game mengatur ulang)
        // atau nilai perluasan dari pembacaan sebelumnya. Keduanya menunjuk
        // dasar yang sama, jadi batas slider boleh berubah tanpa kehilangan
        // nilai asli game.
        int32_t base;
        if (skinBase[slot][i] != 0 &&
            (now == skinBase[slot][i] || now == skinDiperlebar[slot][i])) {
            base = skinBase[slot][i];
        } else {
            base = now;
        }

        int64_t widthNew = static_cast<int64_t>(base) * limit / 5;
        if (widthNew > INT32_MAX) {
            LOGE("Rentang skin: sumbu %d punya %d langkah, dilewati", i, base);
            continue;
        }
        int32_t diperlebar = static_cast<int32_t>(widthNew);
        if (diperlebar < base)
            diperlebar = base;

        if (skinBase[slot][i] != base || skinDiperlebar[slot][i] != diperlebar) {
            skinBase[slot][i] = base;
            skinDiperlebar[slot][i] = diperlebar;
            LOGI("Rentang skin: sumbu %d -%d..%d, %d -> %d langkah", i, limit, limit, base,
                 diperlebar);
        }
        skinLimit[slot][i] = limit;

        *ptrMax = diperlebar;
        if (i < countHalf) {
            char* addressHalf = arrayData(halfStep) + i * sizeof(int32_t);
            *reinterpret_cast<int32_t*>(addressHalf) = diperlebar / 2;
        }
        if (i < countWidth && widthPos > 0.0f) {
            char* addressWidth = arrayData(stepWidth) + i * sizeof(float);
            *reinterpret_cast<float*>(addressWidth) = widthPos / static_cast<float>(diperlebar);
        }

        writeFloat(value, fieldValueMin, -static_cast<float>(limit));
        writeFloat(value, fieldValueMax, static_cast<float>(limit));
        writeFloat(value, fieldValuePerStep, 10.0f / static_cast<float>(base));
    }
}

void hookStepPos(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_POS_REFRESH_STEP).orig);
    if (!original)
        return;
    original(self, method);
    widenRange(self, fieldMPos, fieldPosMain, 0);
}

// Editor potret punya instance _Pos dan array langkah sendiri, jadi ia memakai
// slot memo kedua alih-alih menimpa milik editor model.
void hookStepPortrait(void* self, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*)>(target(T_PORTRAIT_REFRESH_STEP).orig);
    if (!original)
        return;
    original(self, method);
    widenRange(self, fieldMPortrait, fieldPosPortrait, 1);
}

bool findFieldPos(void* klassPos) {
    return findField(klassPos, "steps_Max", fieldStepMax) &&
           findField(klassPos, "steps_Half", fieldStepHalf) &&
           findField(klassPos, "steps_W", fieldWidthStep) &&
           findField(klassPos, "w", fieldWidthPos) &&
           findField(klassPos, "values", fieldValuePos) &&
           findField(fieldElementClass(fieldValuePos), "min", fieldValueMin) &&
           findField(fieldElementClass(fieldValuePos), "max", fieldValueMax) &&
           findField(fieldElementClass(fieldValuePos), "per_Step", fieldValuePerStep);
}

} // namespace

void installSkinRangeHook() {
    void* componentPos = classOf("UI__Menu__Model__Main__Pos");
    bool rangeModel = findField(componentPos, "m", fieldMPos) &&
                        findField(fieldClass(fieldMPos), "_pos", fieldPosMain);

    void* componentPortrait = classOf("Net__Account__Portrait__Main__Pos");
    bool rangePortrait = findField(componentPortrait, "m", fieldMPortrait) &&
                         findField(fieldClass(fieldMPortrait), "_pos", fieldPosPortrait);

    bool fieldPosHas =
        findFieldPos(rangeModel ? fieldClass(fieldPosMain) : fieldClass(fieldPosPortrait));
    if (rangeModel && fieldPosHas) {
        LOGI("Field rentang skin OK");
    } else {
        LOGE("Field rentang skin tidak ditemukan; X/Y tetap -5..5");
    }
    if (!rangePortrait || !fieldPosHas) {
        LOGE("Field rentang potret tidak ditemukan; X/Y potret tetap -5..5");
    }

    installHook(target(T_POS_REFRESH_STEP), reinterpret_cast<void*>(hookStepPos));
    installHook(target(T_PORTRAIT_REFRESH_STEP), reinterpret_cast<void*>(hookStepPortrait));
}
