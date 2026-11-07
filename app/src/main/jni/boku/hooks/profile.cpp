#include "hooks.h"

#include <cstdint>

#include "features.h"
#include "hook_manager.h"
#include "log.h"
#include "targets.h"

namespace {

// The day and month fields open up to 2 characters, the year to 4.
int32_t hookLengthSpawn(void* self, int32_t fallback, void* method) {
    if (!features().inputText) {
        auto original =
            reinterpret_cast<int32_t (*)(void*, int32_t, void*)>(target(T_BIRTHDAY_LENGTH).orig);
        return original ? original(self, fallback, method) : fallback;
    }
    return INT32_MAX;
}

// Birthday__Refresh_ only shows a value when V <= Limit. The default month, day,
// and year limits are all loosened, so any number typed still shows on screen.
void spawnHook(void* self, int32_t v, int32_t i, int32_t limit, void* fallback, void* method) {
    LOGI("Birth date: v=%d i=%d limit=%d", v, i, limit);
    if (features().inputText && (limit == 12 || limit == 31 || limit == 9999))
        limit = INT32_MAX;
    auto original = reinterpret_cast<void (*)(void*, int32_t, int32_t, int32_t, void*, void*)>(
        target(T_BIRTHDAY_REFRESH).orig);
    if (!original)
        return;
    original(self, v, i, limit, fallback, method);
}

// Date_Check(ref Year, ref Month, ref Day) zeroes the year above 9999 and
// rewrites a day that does not exist in that month, which limits the birth date
// fields. Skip only while the birth date field itself is being submitted, so
// other callers keep using the game's own date validation.
thread_local bool changingSpawn = false;

void hookEndSpawn(void* self, void* text, void* method) {
    auto original = reinterpret_cast<void (*)(void*, void*, void*)>(target(T_BIRTHDAY_END).orig);
    if (!original)
        return;
    changingSpawn = features().inputText;
    original(self, text, method);
    changingSpawn = false;
}

void hookCheckDate(int32_t* year, int32_t* month, int32_t* day, void* method) {
    if (changingSpawn) {
        // Nothing is clamped here: free day and month values are the point of
        // this bypass, and the labels are text, so no array is indexed with
        // those values.
        LOGI("Birth date: check skipped y=%d m=%d d=%d", year ? *year : -1,
             month ? *month : -1, day ? *day : -1);
        return;
    }
    auto original =
        reinterpret_cast<void (*)(int32_t*, int32_t*, int32_t*, void*)>(target(T_DATE_CHECK).orig);
    if (!original)
        return;
    original(year, month, day, method);
}

} // namespace

void installProfileHook() {
    installHook(target(T_BIRTHDAY_REFRESH), reinterpret_cast<void*>(spawnHook));
    installHook(target(T_BIRTHDAY_LENGTH), reinterpret_cast<void*>(hookLengthSpawn));
    installHook(target(T_BIRTHDAY_END), reinterpret_cast<void*>(hookEndSpawn));
    installHook(target(T_DATE_CHECK), reinterpret_cast<void*>(hookCheckDate));
}
