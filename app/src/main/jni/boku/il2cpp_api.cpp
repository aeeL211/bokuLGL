#include "il2cpp_api.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>

#include "log.h"

namespace {

Api api = {};

// The same simple name can exist in several assemblies ("Iap" and "_Unlock" are
// both duplicated), so every needed class is registered here and only bound
// after the scan matches one of its methods.
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

// On arm64 the array header holds klass, monitor, and bounds (pointers) plus
// max_length (int32) and four bytes of padding, so data starts at 0x20.
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

void* readPtr(void* object, const Field& field) {
    if (!object || !field.field)
        return nullptr;
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(object) + field.offset);
}

int32_t readInt(void* object, const Field& field) {
    if (!object || !field.field)
        return 0;
    return *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(object) + field.offset);
}

void writeInt(void* object, const Field& field, int32_t value) {
    if (!object || !field.field)
        return;
    *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(object) + field.offset) = value;
}

float readFloat(void* object, const Field& field) {
    if (!object || !field.field)
        return 0.0f;
    return *reinterpret_cast<float*>(reinterpret_cast<char*>(object) + field.offset);
}

void writeFloat(void* object, const Field& field, float value) {
    if (!object || !field.field)
        return;
    *reinterpret_cast<float*>(reinterpret_cast<char*>(object) + field.offset) = value;
}

bool readBool(void* object, const Field& field) {
    if (!object || !field.field)
        return false;
    return *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(object) + field.offset) != 0;
}

void writeBool(void* object, const Field& field, bool value) {
    if (!object || !field.field)
        return;
    *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(object) + field.offset) =
        value ? 1 : 0;
}

// il2cpp_field_set_value in this build stores the reference directly: the setter
// accepts an object pointer and never reads it. Passing a pointer to a local
// variable puts a stack address in the field, then the garbage collector frees
// the string that field was supposed to keep alive. Reading it back also covers
// builds that expect a pointer to the value.
void setRef(void* object, const Field& field, void* value) {
    if (!object || !field.field)
        return;
    if (api.fieldSetValue) {
        api.fieldSetValue(object, field.field, value);
        if (readPtr(object, field) == value)
            return;
        api.fieldSetValue(object, field.field, &value);
        if (readPtr(object, field) == value)
            return;
    }
    *reinterpret_cast<void**>(reinterpret_cast<char*>(object) + field.offset) = value;
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

// A broken sequence is read as one byte and becomes U+FFFD, so the rest of the
// text still gets copied. Returns the number of bytes consumed.
size_t readPointCode(const char* text, size_t available, uint32_t& code) {
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
    if (length > available) {
        code = 0xFFFD;
        return 1;
    }
    for (size_t i = 1; i < length; i++) {
        unsigned char more = static_cast<unsigned char>(text[i]);
        if ((more & 0xC0) != 0x80) {
            code = 0xFFFD;
            return 1;
        }
        code = (code << 6) | (more & 0x3F);
    }
    // The shortest form is rejected so one character cannot have two encodings.
    if ((length == 2 && code < 0x80) || (length == 3 && code < 0x800) ||
        (length == 4 && code < 0x10000) || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        code = 0xFFFD;
        return 1;
    }
    return length;
}

// At most four bytes, so a caller that leaves four bytes has room for any code.
size_t writePointCode(uint32_t code, char* output) {
    if (code < 0x80) {
        output[0] = static_cast<char>(code);
        return 1;
    }
    if (code < 0x800) {
        output[0] = static_cast<char>(0xC0 | (code >> 6));
        output[1] = static_cast<char>(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        output[0] = static_cast<char>(0xE0 | (code >> 12));
        output[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
        output[2] = static_cast<char>(0x80 | (code & 0x3F));
        return 3;
    }
    output[0] = static_cast<char>(0xF0 | (code >> 18));
    output[1] = static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    output[2] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    output[3] = static_cast<char>(0x80 | (code & 0x3F));
    return 4;
}

} // namespace

void* stringFromUtf8(const char* text) {
    if (!text)
        return nullptr;
    size_t length = strlen(text);
    static const uint16_t empty = 0;
    if (!length)
        return stringUtf16New(&empty, 0);
    // One UTF-8 code point becomes at most two UTF-16 units, and the byte count
    // is an upper bound for the number of code points.
    size_t capacity = length * 2 + 2;
    auto* buf = static_cast<uint16_t*>(malloc(capacity * sizeof(uint16_t)));
    if (!buf)
        return nullptr;

    int32_t used = 0;
    for (size_t i = 0; i < length;) {
        uint32_t code = 0;
        i += readPointCode(text + i, length - i, code);
        if (code >= 0x10000) {
            code -= 0x10000;
            buf[used++] = static_cast<uint16_t>(0xD800 + (code >> 10));
            buf[used++] = static_cast<uint16_t>(0xDC00 + (code & 0x3FF));
        } else {
            buf[used++] = static_cast<uint16_t>(code);
        }
    }
    void* result = stringUtf16New(buf, used);
    free(buf);
    return result;
}

bool toUtf8(void* text, char* output, size_t size) {
    int32_t length = lengthString(text);
    const uint16_t* character = characterString(text);
    if (length <= 0 || !character)
        return false;

    size_t used = 0;
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
        if (used + 4 >= size)
            return false;
        used += writePointCode(code, output + used);
    }
    output[used] = '\0';
    return true;
}

void* dataFieldStatic(void* klass) {
    if (!klass || !api.staticFieldData)
        return nullptr;
    return api.staticFieldData(klass);
}

bool nearValue(float value, float target) {
    float delta = value - target;
    return delta > -0.01f && delta < 0.01f;
}

void* addressMethod(void* method) { return method ? *reinterpret_cast<void**>(method) : nullptr; }

bool ownBody(void* address) {
    if (!address)
        return false;
    uintptr_t position = reinterpret_cast<uintptr_t>(address);
    Dl_info information;
    if (!dladdr(reinterpret_cast<void*>(position), &information) || !information.dli_fbase)
        return false;
    return *reinterpret_cast<uint32_t*>(position) != 0xd65f03c0;
}

bool bindApi() {
    void* library = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!library)
        library = dlopen("libil2cpp.so", RTLD_LAZY);
    if (!library)
        return false;

#define IKAT(member, symbol)                                                                      \
    api.member = reinterpret_cast<decltype(api.member)>(dlsym(library, symbol));                 \
    if (!api.member) {                                                                            \
        LOGE("Symbol not found: %s", symbol);                                                      \
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
        reinterpret_cast<decltype(api.fieldType)>(dlsym(library, "il2cpp_field_get_type"));
    api.classFromType = reinterpret_cast<decltype(api.classFromType)>(
        dlsym(library, "il2cpp_class_from_il2cpp_type"));
    api.elementClass = reinterpret_cast<decltype(api.elementClass)>(
        dlsym(library, "il2cpp_class_get_element_class"));
    api.staticFieldData = reinterpret_cast<decltype(api.staticFieldData)>(
        dlsym(library, "il2cpp_class_get_static_field_data"));
    api.fieldSetValue =
        reinterpret_cast<decltype(api.fieldSetValue)>(dlsym(library, "il2cpp_field_set_value"));
    api.stringChars =
        reinterpret_cast<decltype(api.stringChars)>(dlsym(library, "il2cpp_string_chars"));
    api.stringNewUtf16 =
        reinterpret_cast<decltype(api.stringNewUtf16)>(dlsym(library, "il2cpp_string_new_utf16"));
    api.classGetType =
        reinterpret_cast<decltype(api.classGetType)>(dlsym(library, "il2cpp_class_get_type"));
    api.typeGetObject =
        reinterpret_cast<decltype(api.typeGetObject)>(dlsym(library, "il2cpp_type_get_object"));
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

    // Only Unity classes are looked up outside the target table, so results are
    // kept here and the full scan is not repeated.
    constexpr int kLimitKlassExtra = 8;
    static const char* nameSaved[kLimitKlassExtra] = {};
    static void* klassSaved[kLimitKlassExtra] = {};
    for (int i = 0; i < kLimitKlassExtra && nameSaved[i]; i++) {
        if (!strcmp(nameSaved[i], name))
            return klassSaved[i];
    }

    void* known = classOf(name);
    if (known)
        return known;

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

    // More than one class can share the same name, so a candidate is accepted
    // only after the requested field actually exists in it.
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
