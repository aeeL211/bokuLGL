#include "hooks.h"

#include <cstdint>

#include "feature_flags.h"
#include "il2cpp_api.h"
#include "log.h"
#include "targets.h"

namespace {

// The three background parts behave the same way: each has a current id, an
// _id table that maps a type name to that zone's id, and a setter that applies
// the change locally. The setter also notifies the network, but the game only
// sends when this device is the host, so a guest stays local.
constexpr int32_t kMaxOption = 10;
// A rejected or interrupted change is retried after about two seconds instead
// of every frame, so an animation is never restarted continuously.
constexpr int32_t kRetryFrames = 120;

struct Part {
    Field component;          // field on Background__Manager
    Field id;                 // current selection on the part
    Field table;              // _id table on the part
    Field parent[kMaxOption]; // optional nested table before the option
    Field option[kMaxOption];
    int32_t count;
    TargetId setTarget;
    const char* label;
    // Only touched from the player update hook, which runs on the game thread.
    void* world;
    int32_t zoneId;
    int32_t original;
    int32_t cooldown;
    bool captured;
};

Part partBackground = {};
Part partPeriod = {};
Part partWeather = {};

Field fieldPlayerManager = {};
Field fieldManagerBackground = {};
Field fieldManagerZone = {};
Field fieldZoneCurrent = {};

bool worldReady = false;
bool worldActive = false;

const char* const kBackgroundNames[] = {"mountain", "forest",      "city",
                                        "desert",   "forest_Dead", "planet"};
const char* const kPeriodNames[] = {"morning", "twilight", "night"};

void callSet(const Part& part, void* object, int32_t zone, int32_t id) {
    Target& entry = target(part.setTarget);
    if (!entry.addr)
        return;
    if (part.setTarget == T_PERIOD_SET) {
        auto call = reinterpret_cast<void (*)(void*, int32_t, int32_t, void*)>(entry.addr);
        call(object, zone, id, entry.method);
        return;
    }
    // Background and weather take the Init flag, which is false here so the
    // normal transition runs instead of a rebuild.
    auto call = reinterpret_cast<void (*)(void*, int32_t, int32_t, bool, void*)>(entry.addr);
    call(object, zone, id, false, entry.method);
}

int32_t optionId(void* table, const Part& part, int32_t index) {
    if (!table || index < 0 || index >= part.count || !part.option[index].field)
        return -1;
    void* base = table;
    if (part.parent[index].field) {
        base = readPtr(table, part.parent[index]);
        if (!base)
            return -1;
    }
    return readInt(base, part.option[index]);
}

int32_t indexOf(void* table, const Part& part, int32_t id) {
    if (id < 0)
        return -1;
    for (int32_t i = 0; i < part.count; i++) {
        if (optionId(table, part, i) == id)
            return i;
    }
    return -1;
}

// The game may rebuild the _id table when a zone is loaded, and each zone keeps
// its own selection, so a captured original only belongs to the world it was
// read from.
void forgetWorld(Part& part, void* table, int32_t zone) {
    part.world = table;
    part.zoneId = zone;
    part.captured = false;
    part.original = -1;
    part.cooldown = 0;
}

void restorePart(void* component, Part& part, int32_t zone) {
    if (!component || !part.captured)
        return;
    void* table = readPtr(component, part.table);
    int32_t original = part.original;
    bool sameWorld = table && table == part.world;
    forgetWorld(part, table, zone);
    if (!sameWorld || original < 0)
        return;
    int32_t id = optionId(table, part, original);
    if (id < 0 || readInt(component, part.id) == id)
        return;
    callSet(part, component, zone, id);
    LOGI("World override: %s restored to %d", part.label, id);
}

void applyPart(void* component, Part& part, int32_t target, int32_t zone) {
    if (!component || !part.count)
        return;
    void* table = readPtr(component, part.table);
    if (!table)
        return;
    if (table != part.world || zone != part.zoneId)
        forgetWorld(part, table, zone);
    if (target <= 0) {
        // "Bawaan" hands this part back to the host while the switch stays on.
        restorePart(component, part, zone);
        return;
    }
    if (target > part.count)
        target = part.count;

    int32_t targetId = optionId(table, part, target - 1);
    if (targetId < 0)
        return;
    int32_t current = readInt(component, part.id);
    if (!part.captured) {
        part.original = indexOf(table, part, current);
        part.captured = true;
        if (part.original < 0)
            LOGE("World override: %s original id %d is unknown", part.label, current);
    }
    if (current == targetId) {
        part.cooldown = 0;
        return;
    }
    if (part.cooldown > 0) {
        part.cooldown--;
        return;
    }
    callSet(part, component, zone, targetId);
    part.cooldown = kRetryFrames;
    LOGI("World override: %s set to %d", part.label, targetId);
}

void restoreAll(void* bgManager, int32_t zone) {
    restorePart(readPtr(bgManager, partBackground.component), partBackground, zone);
    restorePart(readPtr(bgManager, partPeriod.component), partPeriod, zone);
    restorePart(readPtr(bgManager, partWeather.component), partWeather, zone);
}

void applyAll(void* bgManager, int32_t zone) {
    const Features& flag = features();
    applyPart(readPtr(bgManager, partBackground.component), partBackground,
              flag.worldBackground.load(), zone);
    applyPart(readPtr(bgManager, partPeriod.component), partPeriod, flag.worldPeriod.load(), zone);
    applyPart(readPtr(bgManager, partWeather.component), partWeather, flag.worldWeather.load(),
              zone);
}

bool setupPart(Part& part, void* componentClass, TargetId setTarget, const char* label,
               const char* const* names, int32_t count) {
    part.setTarget = setTarget;
    part.label = label;
    if (!target(setTarget).addr) {
        LOGE("World override: setter for %s not found", label);
        return false;
    }
    if (!findField(componentClass, "id", part.id) ||
        !findField(componentClass, "_id", part.table)) {
        LOGE("World override: fields for %s not found", label);
        return false;
    }
    void* tableClass = fieldClass(part.table);
    if (!tableClass) {
        LOGE("World override: id table for %s not found", label);
        return false;
    }
    for (int32_t i = 0; i < count; i++) {
        if (!findField(tableClass, names[i], part.option[i])) {
            LOGE("World override: option %s for %s not found", names[i], label);
            return false;
        }
    }
    part.count = count;
    return true;
}

// Weather has two virus variants in one nested table, so they cannot use the
// flat name list used by the other parts.
bool setupWeather(void* componentClass) {
    Part& part = partWeather;
    part.setTarget = T_WEATHER_SET;
    part.label = "weather";
    if (!target(T_WEATHER_SET).addr) {
        LOGE("World override: setter for weather not found");
        return false;
    }
    if (!findField(componentClass, "id", part.id) ||
        !findField(componentClass, "_id", part.table)) {
        LOGE("World override: fields for weather not found");
        return false;
    }
    void* tableClass = fieldClass(part.table);
    if (!tableClass) {
        LOGE("World override: id table for weather not found");
        return false;
    }

    static const char* const plain[] = {"sunshine", "rain",   "thunderstorm", "snow",
                                        "leaf",     "sakura", "glow",         "star"};
    for (int32_t i = 0; i < 8; i++) {
        if (!findField(tableClass, plain[i], part.option[i])) {
            LOGE("World override: option %s for weather not found", plain[i]);
            return false;
        }
    }
    if (!findField(tableClass, "virus", part.parent[8])) {
        LOGE("World override: option virus for weather not found");
        return false;
    }
    void* virusClass = fieldClass(part.parent[8]);
    part.parent[9] = part.parent[8];
    if (!virusClass || !findField(virusClass, "grn", part.option[8]) ||
        !findField(virusClass, "ppl", part.option[9])) {
        LOGE("World override: virus colors for weather not found");
        return false;
    }
    part.count = 10;
    return true;
}

} // namespace

// Runs at the end of the local player update, on the game thread. Only the
// local view is changed: the host keeps its own world, and a host that uses the
// feature simply sees the game's normal broadcast.
void applyWorldOverride(void* playerManager) {
    if (!worldReady || !playerManager)
        return;
    bool want = features().worldLocal.load(std::memory_order_relaxed);
    if (!want) {
        if (!worldActive)
            return;
        worldActive = false;
        void* manager = readPtr(playerManager, fieldPlayerManager);
        void* zone = manager ? readPtr(manager, fieldManagerZone) : nullptr;
        int32_t zoneId = zone ? readInt(zone, fieldZoneCurrent) : -1;
        void* bgManager = manager ? readPtr(manager, fieldManagerBackground) : nullptr;
        if (!bgManager || zoneId < 0) {
            LOGI("World override: disabled while no world is loaded");
            return;
        }
        restoreAll(bgManager, zoneId);
        return;
    }

    if (!worldActive) {
        worldActive = true;
        LOGI("World override: enabled");
    }
    void* manager = readPtr(playerManager, fieldPlayerManager);
    void* zone = manager ? readPtr(manager, fieldManagerZone) : nullptr;
    int32_t zoneId = zone ? readInt(zone, fieldZoneCurrent) : -1;
    void* bgManager = manager ? readPtr(manager, fieldManagerBackground) : nullptr;
    if (!bgManager || zoneId < 0)
        return;
    applyAll(bgManager, zoneId);
}

void installWorldHook() {
    void* playerClass = classOf("Player__Manager");
    if (!findField(playerClass, "a", fieldPlayerManager)) {
        LOGE("World override: Manager field on Player__Manager not found");
        return;
    }
    void* managerClass = fieldClass(fieldPlayerManager);
    if (!findField(managerClass, "bg", fieldManagerBackground) ||
        !findField(managerClass, "zone", fieldManagerZone)) {
        LOGE("World override: background or zone manager field not found");
        return;
    }
    void* bgManagerClass = fieldClass(fieldManagerBackground);
    void* zoneClass = fieldClass(fieldManagerZone);
    if (!bgManagerClass || !zoneClass || !findField(zoneClass, "current", fieldZoneCurrent)) {
        LOGE("World override: current zone field not found");
        return;
    }
    if (!findField(bgManagerClass, "background", partBackground.component) ||
        !findField(bgManagerClass, "period", partPeriod.component) ||
        !findField(bgManagerClass, "weather", partWeather.component)) {
        LOGE("World override: background parts not found");
        return;
    }

    bool ready =
        setupPart(partBackground, fieldClass(partBackground.component), T_BG_SET, "background",
                  kBackgroundNames,
                  static_cast<int32_t>(sizeof(kBackgroundNames) / sizeof(kBackgroundNames[0])));
    ready = setupPart(partPeriod, fieldClass(partPeriod.component), T_PERIOD_SET, "period",
                      kPeriodNames,
                      static_cast<int32_t>(sizeof(kPeriodNames) / sizeof(kPeriodNames[0]))) &&
            ready;
    ready = setupWeather(fieldClass(partWeather.component)) && ready;
    if (!ready) {
        LOGE("World override fields incomplete; feature disabled");
        return;
    }
    worldReady = true;
    LOGI("World override fields ready");
}
