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
int32_t skinWidened[kSlotRange][kMaxAxis] = {};
int32_t skinLimit[kSlotRange][kMaxAxis] = {};

// Refresh__Step resets the X/Y slider to -5..+5 then derives steps_Half,
// steps_W, and per_Step from steps_Max. Only axes still at -5..+5 are widened
// (rotation axes are left alone) and the derived values are rebuilt so
// Value__To__Step sees the -limit..+limit range on the same Refresh() path.
// The limit comes from the menu slider: 5 means unchanged, 25 is the old
// default, and 100 is the furthest shift.
void widenRange(void* self, const Field& fieldOwner, const Field& fieldPos, int slot) {
    int32_t limit = features().skinRange.load();
    if (limit < 5)
        limit = 5;
    if (limit > 100)
        limit = 100;
    if (!fieldOwner.field || !fieldPos.field || !fieldStepMax.field || !fieldStepHalf.field ||
        !fieldWidthStep.field || !fieldWidthPos.field || !fieldValuePos.field ||
        !fieldValueMin.field || !fieldValueMax.field || !fieldValuePerStep.field) {
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
        // Some Refresh paths reset min/max to -5..+5, others keep the values
        // already written. Both are accepted so the new limit still applies on
        // either path.
        bool ourMark = skinLimit[slot][i] != 0 &&
                       nearValue(minLimit, -static_cast<float>(skinLimit[slot][i])) &&
                       nearValue(maxLimit, static_cast<float>(skinLimit[slot][i]));
        if (!stillDefault && !ourMark)
            continue;

        char* addressMax = arrayData(maxStep) + i * sizeof(int32_t);
        int32_t* ptrMax = reinterpret_cast<int32_t*>(addressMax);
        int32_t now = *ptrMax;
        if (now <= 0) {
            LOGE("Skin range: axis %d has steps_Max=%d, skipped", i, now);
            continue;
        }

        // steps_Max can hold the base value (after the game resets it) or the
        // widened value from a previous read. Both point at the same base, so
        // the slider limit can change without losing the game's original value.
        int32_t base;
        if (skinBase[slot][i] != 0 && (now == skinBase[slot][i] || now == skinWidened[slot][i])) {
            base = skinBase[slot][i];
        } else {
            base = now;
        }

        int64_t widthNew = static_cast<int64_t>(base) * limit / 5;
        if (widthNew > INT32_MAX) {
            LOGE("Skin range: axis %d has %d steps, skipped", i, base);
            continue;
        }
        int32_t widened = static_cast<int32_t>(widthNew);
        if (widened < base)
            widened = base;

        if (skinBase[slot][i] != base || skinWidened[slot][i] != widened) {
            skinBase[slot][i] = base;
            skinWidened[slot][i] = widened;
            LOGI("Skin range: axis %d -%d..%d, %d -> %d steps", i, limit, limit, base, widened);
        }
        skinLimit[slot][i] = limit;

        *ptrMax = widened;
        if (i < countHalf) {
            char* addressHalf = arrayData(halfStep) + i * sizeof(int32_t);
            *reinterpret_cast<int32_t*>(addressHalf) = widened / 2;
        }
        if (i < countWidth && widthPos > 0.0f) {
            char* addressWidth = arrayData(stepWidth) + i * sizeof(float);
            *reinterpret_cast<float*>(addressWidth) = widthPos / static_cast<float>(widened);
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

// The portrait editor has its own _Pos instance and step arrays, so it uses the
// second memo slot instead of overwriting the model editor's.
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
        LOGI("Skin range fields OK");
    } else {
        LOGE("Skin range fields not found; X/Y stays -5..5");
    }
    if (!rangePortrait || !fieldPosHas) {
        LOGE("Portrait range fields not found; portrait X/Y stays -5..5");
    }

    installHook(target(T_POS_REFRESH_STEP), reinterpret_cast<void*>(hookStepPos));
    installHook(target(T_PORTRAIT_REFRESH_STEP), reinterpret_cast<void*>(hookStepPortrait));
}
