#include "il2cpp_api.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>

#include "log.h"

namespace {

Api api = {};

// Nama sederhana yang sama bisa ada di beberapa assembly ("Iap" dan "_Unlock"
// sama-sama terduplikasi), jadi setiap kelas yang dibutuhkan didaftarkan di sini
// dan baru diikat setelah scan mencocokkan salah satu metodenya.
struct SlotKlass {
    const char* name;
    void* klass;
};

SlotKlass listKlass[] = {
    {"Iap", nullptr},
    {"UI__Iap", nullptr},
    {"_Unlock", nullptr},
    {"Net__Multi__Client", nullptr},
    {"UI__Menu__Net__Multi__Join__Room_List__Detail__Password", nullptr},
    {"UIInput", nullptr},
    {"UI__Manager", nullptr},
    {"Net__Account__Manager", nullptr},
    {"Net__Account__My_Account__Detail", nullptr},
    {"Net__Account__Verify__Detail", nullptr},
    {"Net__Account__Friend_Profile", nullptr},
    {"Net__Account__Add_Friend", nullptr},
    {"UI__Menu__Option__Profile", nullptr},
    {"MyBehaviour", nullptr},
    {"UI__Menu__Model__Main__Pos", nullptr},
    {"Net__Account__Portrait__Main__Pos", nullptr},
    {"UI__Menu__Option__About", nullptr},
    {"Title__Cmn", nullptr},
    {"UI__Menu__Net__Multi__Help", nullptr},
    {"_Tile", nullptr},
    {"Tile__Explode", nullptr},
    {"Global", nullptr},
    {"Net__Multi__Transfer__Pipe", nullptr},
    {"Net__Multi__Transfer__Host", nullptr},
    {"Net__Multi__Chat", nullptr},
    {"Net__Multi", nullptr},
    {"Net__Multi__Guest__Chat", nullptr},
    {"Guest__Manager", nullptr},
    {"UI__Menu__World__New__Setting__Size", nullptr},
    {"Player__State", nullptr},
    {"Player__State__Cmn", nullptr},
    {"Player__State__Std__Air", nullptr},
    {"Player__State__Std__Action__Hh__Weapon", nullptr},
    {"Camera__Manager", nullptr},
    {"Eff__Type__Tile", nullptr},
    {"Player__Manager", nullptr},
    {"Player__State__Std__Action__Hh__Nature", nullptr},
    {"Net__Multi__Guest__Main__Console", nullptr},
    {"Net__Multi__Guest__Menu", nullptr},
    {"Input__Began__Net", nullptr},
};

// Di arm64, kepala array berisi klass, monitor, dan bounds (pointer) ditambah
// max_length (int32) dan empat byte padding, jadi data dimulai di 0x20.
constexpr size_t kDataArray = 4 * sizeof(void*);

} // namespace

Api& apiIl2cpp() { return api; }

void* classOf(const char* name) {
    for (SlotKlass& s : listKlass) {
        if (!strcmp(s.name, name))
            return s.klass;
    }
    return nullptr;
}

void bindKlass(const char* name, void* klass) {
    for (SlotKlass& s : listKlass) {
        if (!s.klass && !strcmp(s.name, name))
            s.klass = klass;
    }
}

bool findField(void* klass, const char* name, Field& result) {
    if (!klass)
        return false;
    result.field = api.fieldFromName(klass, name);
    if (!result.field)
        return false;
    result.offset = api.fieldOffset(result.field);
    return true;
}

void* fieldClass(const Field& field) {
    if (!field.field || !api.fieldType || !api.classFromType)
        return nullptr;
    void* type = api.fieldType(field.field);
    return type ? api.classFromType(type) : nullptr;
}

void* fieldElementClass(const Field& field) {
    void* klass = fieldClass(field);
    if (!klass || !api.elementClass)
        return nullptr;
    return api.elementClass(klass);
}

void* readPtr(void* objek, const Field& field) {
    if (!objek || !field.field)
        return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(objek) + field.offset);
}

int32_t readInt(void* objek, const Field& field) {
    if (!objek || !field.field)
        return 0;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(objek) + field.offset);
}

void writeInt(void* objek, const Field& field, int32_t value) {
    if (!objek || !field.field)
        return;
    *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(objek) + field.offset) = value;
}

float readFloat(void* objek, const Field& field) {
    if (!objek || !field.field)
        return 0.0f;
    return *reinterpret_cast<float*>(reinterpret_cast<char*>(objek) + field.offset);
}

void writeFloat(void* objek, const Field& field, float value) {
    if (!objek || !field.field)
        return;
    *reinterpret_cast<float*>(reinterpret_cast<char*>(objek) + field.offset) = value;
}

bool readBool(void* objek, const Field& field) {
    if (!objek || !field.field)
        return false;
    return *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(objek) + field.offset) != 0;
}

void writeBool(void* objek, const Field& field, bool value) {
    if (!objek || !field.field)
        return;
    *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(objek) + field.offset) =
        value ? 1 : 0;
}

// il2cpp_field_set_value pada build ini menyimpan referensinya langsung: fungsi
// penyetelnya menerima pointer objek dan tidak pernah membacanya. Memberi pointer
// ke variabel lokal menaruh alamat stack di field, lalu pengumpul sampah membebaskan
// string yang seharusnya dijaga hidup oleh field itu. Pembacaan ulang menutup build
// yang justru mengharapkan pointer ke nilai.
void setRef(void* objek, const Field& field, void* value) {
    if (!objek || !field.field)
        return;
    if (api.fieldSetValue) {
        api.fieldSetValue(objek, field.field, value);
        if (readPtr(objek, field) == value)
            return;
        api.fieldSetValue(objek, field.field, &value);
        if (readPtr(objek, field) == value)
            return;
    }
    *reinterpret_cast<void**>(reinterpret_cast<char*>(objek) + field.offset) = value;
}

int32_t lengthArray(void* array) {
    if (!array || !api.arrayLength)
        return 0;
    return static_cast<int32_t>(api.arrayLength(array));
}

char* arrayData(void* array) { return reinterpret_cast<char*>(array) + kDataArray; }

int32_t lengthString(void* text) {
    if (!text || !api.stringLength)
        return 0;
    return api.stringLength(text);
}

const uint16_t* characterString(void* text) {
    if (!text || !api.stringChars)
        return nullptr;
    return api.stringChars(text);
}

bool sameString(void* first, void* secondary) {
    if (!first || !secondary)
        return false;
    if (first == secondary)
        return true;
    int32_t length = lengthString(first);
    if (length != lengthString(secondary))
        return false;
    const uint16_t* characterFirst = characterString(first);
    const uint16_t* characterSecondary = characterString(secondary);
    if (!characterFirst || !characterSecondary)
        return false;
    for (int32_t i = 0; i < length; i++) {
        if (characterFirst[i] != characterSecondary[i])
            return false;
    }
    return true;
}

void* stringUtf16New(const uint16_t* character, int32_t length) {
    if (!api.stringNewUtf16)
        return nullptr;
    return api.stringNewUtf16(character, length);
}

namespace {

// Urutan yang rusak dibaca satu byte dan menjadi U+FFFD, jadi sisa teks tetap
// tersalin. Mengembalikan jumlah byte yang dipakai.
size_t readPointCode(const char* text, size_t tersedia, uint32_t& code) {
    unsigned char initial = static_cast<unsigned char>(text[0]);
    size_t length = 0;
    if (initial < 0x80) {
        code = initial;
        return 1;
    }
    if ((initial & 0xE0) == 0xC0) {
        code = initial & 0x1F;
        length = 2;
    } else if ((initial & 0xF0) == 0xE0) {
        code = initial & 0x0F;
        length = 3;
    } else if ((initial & 0xF8) == 0xF0) {
        code = initial & 0x07;
        length = 4;
    } else {
        code = 0xFFFD;
        return 1;
    }
    if (length > tersedia) {
        code = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i < length; i++) {
        unsigned char lanjut = static_cast<unsigned char>(text[i]);
        if ((lanjut & 0xC0) != 0x80) {
            code = 0xFFFD;
            return 1;
        }
        code = (code << 6) | (lanjut & 0x3F);
    }
    // Bentuk terpendek ditolak supaya satu karakter tidak punya dua ejaan.
    if ((length == 2 && code < 0x80) || (length == 3 && code < 0x800) ||
        (length == 4 && code < 0x10000) || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        code = 0xFFFD;
        return 1;
    }
    return length;
}

// Paling banyak empat byte, jadi pemanggil yang menyisakan empat byte punya ruang
// untuk kode apa pun.
size_t writePointCode(uint32_t code, char* keluaran) {
    if (code < 0x80) {
        keluaran[0] = static_cast<char>(code);
        return 1;
    }
    if (code < 0x800) {
        keluaran[0] = static_cast<char>(0xC0 | (code >> 6));
        keluaran[1] = static_cast<char>(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        keluaran[0] = static_cast<char>(0xE0 | (code >> 12));
        keluaran[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        keluaran[2] = static_cast<char>(0x80 | (code & 0x3F));
        return 3;
    }
    keluaran[0] = static_cast<char>(0xF0 | (code >> 18));
    keluaran[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    keluaran[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    keluaran[3] = static_cast<char>(0x80 | (code & 0x3F));
    return 4;
}

} // namespace

void* stringFromUtf8(const char* text) {
    if (!text)
        return nullptr;
    size_t length = strlen(text);
    static const uint16_t kosong = 0;
    if (!length)
        return stringUtf16New(&kosong, 0);
    // Satu titik kode UTF-8 menjadi paling banyak dua unit UTF-16, dan jumlah
    // byte menjadi batas atas jumlah titik kode.
    size_t kapasitas = length * 2 + 2;
    auto* buf = static_cast<uint16_t*>(malloc(kapasitas * sizeof(uint16_t)));
    if (!buf)
        return nullptr;

    int32_t terpakai = 0;
    for (size_t i = 0; i < length;) {
        uint32_t code = 0;
        i += readPointCode(text + i, length - i, code);
        if (code >= 0x10000) {
            code -= 0x10000;
            buf[terpakai++] = static_cast<uint16_t>(0xD800 + (code >> 10));
            buf[terpakai++] = static_cast<uint16_t>(0xDC00 + (code & 0x3FF));
        } else {
            buf[terpakai++] = static_cast<uint16_t>(code);
        }
    }
    void* result = stringUtf16New(buf, terpakai);
    free(buf);
    return result;
}

bool toUtf8(void* text, char* keluaran, size_t size) {
    int32_t length = lengthString(text);
    const uint16_t* character = characterString(text);
    if (length <= 0 || !character)
        return false;

    size_t terpakai = 0;
    for (int32_t i = 0; i < length; i++) {
        uint32_t code = character[i];
        if (code >= 0xD800 && code <= 0xDBFF && i + 1 < length) {
            uint32_t low = character[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                i++;
            }
        }
        if (code >= 0xD800 && code <= 0xDFFF)
            code = 0xFFFD;
        if (terpakai + 4 >= size)
            return false;
        terpakai += writePointCode(code, keluaran + terpakai);
    }
    keluaran[terpakai] = '\0';
    return true;
}

void* dataFieldStatic(void* klass) {
    if (!klass || !api.staticFieldData)
        return nullptr;
    return api.staticFieldData(klass);
}

bool nearValue(float value, float target) {
    float selisih = value - target;
    return selisih > -0.01f && selisih < 0.01f;
}

void* addressMethod(void* method) { return method ? *reinterpret_cast<void**>(method) : nullptr; }

bool ownBody(void* address) {
    if (!address)
        return false;
    uintptr_t position = reinterpret_cast<uintptr_t>(address);
    Dl_info informasi;
    if (!dladdr(reinterpret_cast<void*>(position), &informasi) || !informasi.dli_fbase)
        return false;
    return *reinterpret_cast<uint32_t*>(position) != 0xd65f03c0;
}

bool bindApi() {
    void* pustaka = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!pustaka)
        pustaka = dlopen("libil2cpp.so", RTLD_LAZY);
    if (!pustaka)
        return false;

#define IKAT(anggota, simbol)                                                                      \
    api.anggota = reinterpret_cast<decltype(api.anggota)>(dlsym(pustaka, simbol));                 \
    if (!api.anggota) {                                                                            \
        LOGE("Simbol tidak ditemukan: %s", simbol);                                                \
        return false;                                                                              \
    }

    IKAT(domain, "il2cpp_domain_get");
    IKAT(assemblies, "il2cpp_domain_get_assemblies");
    IKAT(image, "il2cpp_assembly_get_image");
    IKAT(classCount, "il2cpp_image_get_class_count");
    IKAT(classAt, "il2cpp_image_get_class");
    IKAT(className, "il2cpp_class_get_name");
    IKAT(methods, "il2cpp_class_get_methods");
    IKAT(methodName, "il2cpp_method_get_name");
    IKAT(paramCount, "il2cpp_method_get_param_count");
    IKAT(param, "il2cpp_method_get_param");
    IKAT(attach, "il2cpp_thread_attach");
    IKAT(fieldFromName, "il2cpp_class_get_field_from_name");
    IKAT(fieldOffset, "il2cpp_field_get_offset");
    IKAT(arrayLength, "il2cpp_array_length");
    IKAT(stringLength, "il2cpp_string_length");
#undef IKAT

    api.fieldType =
        reinterpret_cast<decltype(api.fieldType)>(dlsym(pustaka, "il2cpp_field_get_type"));
    api.classFromType = reinterpret_cast<decltype(api.classFromType)>(
        dlsym(pustaka, "il2cpp_class_from_il2cpp_type"));
    api.elementClass = reinterpret_cast<decltype(api.elementClass)>(
        dlsym(pustaka, "il2cpp_class_get_element_class"));
    api.staticFieldData = reinterpret_cast<decltype(api.staticFieldData)>(
        dlsym(pustaka, "il2cpp_class_get_static_field_data"));
    api.fieldSetValue =
        reinterpret_cast<decltype(api.fieldSetValue)>(dlsym(pustaka, "il2cpp_field_set_value"));
    api.stringChars =
        reinterpret_cast<decltype(api.stringChars)>(dlsym(pustaka, "il2cpp_string_chars"));
    api.stringNewUtf16 =
        reinterpret_cast<decltype(api.stringNewUtf16)>(dlsym(pustaka, "il2cpp_string_new_utf16"));
    api.classGetType =
        reinterpret_cast<decltype(api.classGetType)>(dlsym(pustaka, "il2cpp_class_get_type"));
    api.typeGetObject =
        reinterpret_cast<decltype(api.typeGetObject)>(dlsym(pustaka, "il2cpp_type_get_object"));
    return true;
}

void* typeKlass(void* klass) {
    if (!klass || !api.classGetType || !api.typeGetObject)
        return nullptr;
    void* type = api.classGetType(klass);
    return type ? api.typeGetObject(type) : nullptr;
}

void* findClass(const char* name) {
    if (!name)
        return nullptr;

    // Hanya kelas Unity yang dicari di luar tabel target, jadi hasilnya disimpan
    // di sini dan pemindaian penuh tidak diulang.
    constexpr int kLimitKlassExtra = 8;
    static const char* nameSaved[kLimitKlassExtra] = {};
    static void* klassSaved[kLimitKlassExtra] = {};
    for (int i = 0; i < kLimitKlassExtra && nameSaved[i]; i++) {
        if (!strcmp(nameSaved[i], name))
            return klassSaved[i];
    }

    void* dikenal = classOf(name);
    if (dikenal)
        return dikenal;

    Api& a = api;
    void* domain = a.domain ? a.domain() : nullptr;
    if (!domain || !a.assemblies || !a.image)
        return nullptr;
    size_t countAssembly = 0;
    void** assemblies = a.assemblies(domain, &countAssembly);
    if (!assemblies)
        return nullptr;

    for (size_t i = 0; i < countAssembly; i++) {
        void* image = a.image(assemblies[i]);
        if (!image)
            continue;
        size_t countKlass = a.classCount(image);
        for (size_t j = 0; j < countKlass; j++) {
            void* klass = a.classAt(image, j);
            const char* className = klass ? a.className(klass) : nullptr;
            if (!className || strcmp(className, name))
                continue;
            for (int slot = 0; slot < kLimitKlassExtra; slot++) {
                if (nameSaved[slot])
                    continue;
                nameSaved[slot] = name;
                klassSaved[slot] = klass;
                break;
            }
            return klass;
        }
    }
    return nullptr;
}

void* findKlassField(const char* name, const char* nameField) {
    if (!name || !nameField)
        return nullptr;

    Api& a = api;
    void* domain = a.domain ? a.domain() : nullptr;
    if (!domain || !a.assemblies || !a.image || !a.fieldFromName)
        return nullptr;
    size_t countAssembly = 0;
    void** assemblies = a.assemblies(domain, &countAssembly);
    if (!assemblies)
        return nullptr;

    // Kelas dengan nama sama bisa lebih dari satu, jadi kandidat baru diterima
    // setelah field yang diminta benar-benar ada di dalamnya.
    for (size_t i = 0; i < countAssembly; i++) {
        void* image = a.image(assemblies[i]);
        if (!image)
            continue;
        size_t countKlass = a.classCount(image);
        for (size_t j = 0; j < countKlass; j++) {
            void* klass = a.classAt(image, j);
            const char* className = klass ? a.className(klass) : nullptr;
            if (!className || strcmp(className, name))
                continue;
            if (a.fieldFromName(klass, nameField))
                return klass;
        }
    }
    return nullptr;
}
