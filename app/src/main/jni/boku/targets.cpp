#include "targets.h"

#include <cstdint>
#include <cstring>

#include "il2cpp_api.h"
#include "log.h"

namespace {

Target targets[T_COUNT] = {};
char poolName[kTargetBlobSize + 1] = {};
bool ready = false;

uint64_t mix(uint64_t masukan) {
    masukan = (masukan ^ (masukan >> 30)) * 0xbf58476d1ce4e5b9ULL;
    masukan = (masukan ^ (masukan >> 27)) * 0x94d049bb133111ebULL;
    return masukan ^ (masukan >> 31);
}

uint8_t byteKey(uint64_t seed, uint32_t indeks) {
    uint64_t value = seed ^ (static_cast<uint64_t>(indeks) * 0x9e3779b97f4a7c15ULL);
    return static_cast<uint8_t>(mix(value) & 0xFF);
}

// Overload bisa berbagi nama dan jumlah argumen, jadi sebuah target boleh juga
// menamai kelas parameter terakhirnya. Kelas array adalah "String[]" dan kelas
// generik mempertahankan nama "List`1", jadi perbandingan persis sudah cukup untuk
// memilih satu overload.
bool paramMatch(const Target& entry, void* method) {
    if (!entry.paramType)
        return true;
    Api& api = apiIl2cpp();
    void* type = api.param(method, static_cast<uint32_t>(entry.argc - 1));
    void* klass = type ? api.classFromType(type) : nullptr;
    const char* name = klass ? api.className(klass) : nullptr;
    return name && !strcmp(entry.paramType, name);
}

} // namespace

bool initializeTargets() {
    if (ready)
        return true;

    for (uint32_t i = 0; i < kTargetBlobSize; i++)
        poolName[i] = static_cast<char>(kTargetBlob[i] ^ byteKey(kBuildSeed, i));
    poolName[kTargetBlobSize] = '\0';

    for (uint32_t i = 0; i < static_cast<uint32_t>(T_COUNT); i++) {
        const TargetData& data = kTargetData[i];
        Target& entry = targets[i];
        entry.cls = poolName + data.cls;
        entry.name = poolName + data.name;
        entry.paramType = data.param == kNoParam ? nullptr : poolName + data.param;
        entry.argc = data.argc;
    }
    ready = true;
    return true;
}

Target& target(TargetId id) { return targets[id]; }

int scan() {
    if (!ready)
        return 0;

    Api& api = apiIl2cpp();
    void* domain = api.domain();
    if (!domain)
        return 0;

    size_t countAssembly = 0;
    void** assemblies = api.assemblies(domain, &countAssembly);
    if (!assemblies)
        return 0;

    for (size_t i = 0; i < countAssembly; i++) {
        void* image = api.image(assemblies[i]);
        if (!image)
            continue;

        size_t countKlass = api.classCount(image);
        for (size_t j = 0; j < countKlass; j++) {
            void* klass = api.classAt(image, j);
            if (!klass)
                continue;

            const char* className = api.className(klass);
            if (!className)
                continue;

            bool dicari = false;
            for (const Target& entry : targets) {
                if (!strcmp(entry.cls, className)) {
                    dicari = true;
                    break;
                }
            }
            if (!dicari)
                continue;

            void* iterator = nullptr;
            while (void* method = api.methods(klass, &iterator)) {
                const char* nameMethod = api.methodName(method);
                if (!nameMethod)
                    continue;

                int countParam = api.paramCount(method);
                for (Target& entry : targets) {
                    if (entry.addr)
                        continue;
                    if (strcmp(entry.cls, className) || strcmp(entry.name, nameMethod) ||
                        entry.argc != countParam)
                        continue;
                    if (!paramMatch(entry, method))
                        continue;

                    void* address = addressMethod(method);
                    if (!ownBody(address))
                        continue;
                    entry.addr = address;
                    entry.method = method;
                    bindKlass(className, klass);
                }
            }
        }
    }

    int ditemukan = 0;
    for (const Target& entry : targets) {
        if (entry.addr) {
            ditemukan++;
            continue;
        }
        // Nama yang tidak cocok dengan build game disebut satu per satu, supaya
        // hitungan yang kurang dari T_COUNT bisa ditelusuri tanpa menebak.
        LOGE("Target tidak ditemukan saat scan: %s.%s/%d", entry.cls, entry.name, entry.argc);
    }
    return ditemukan;
}
