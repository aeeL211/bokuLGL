#pragma once

#include <cstddef>
#include <cstdint>

// The IL2CPP function table is bound once by bindApi(). It is obtained through
// an accessor function so no mutable table lives at file scope.
struct Api {
    void* (*domain)();
    void** (*assemblies)(void*, size_t*);
    void* (*image)(void*);
    size_t (*classCount)(void*);
    void* (*classAt)(void*, size_t);
    const char* (*className)(void*);
    void* (*methods)(void*, void**);
    const char* (*methodName)(void*);
    int (*paramCount)(void*);
    void* (*param)(void*, uint32_t);
    void* (*attach)(void*);
    void* (*fieldFromName)(void*, const char*);
    size_t (*fieldOffset)(void*);
    void* (*fieldType)(void*);
    void* (*classFromType)(void*);
    void* (*elementClass)(void*);
    void* (*staticFieldData)(void*);
    void (*fieldSetValue)(void*, void*, void*);
    uint32_t (*arrayLength)(void*);
    int32_t (*stringLength)(void*);
    uint16_t* (*stringChars)(void*);
    void* (*stringNewUtf16)(const uint16_t*, int32_t);
    void* (*classGetType)(void*);
    void* (*typeGetObject)(void*);
};

Api& apiIl2cpp();

bool bindApi();

// The same simple name can exist in several assemblies, so a class is accepted
// only after one of its methods matches the requested name and argument count.
void* classOf(const char* name);
void bindKlass(const char* name, void* klass);

struct Field {
    void* field;
    size_t offset;
};

bool findField(void* klass, const char* name, Field& result);
void* fieldClass(const Field& field);
// For array fields this returns the element class, not the array class.
void* fieldElementClass(const Field& field);

void* readPtr(void* object, const Field& field);
int32_t readInt(void* object, const Field& field);
void writeInt(void* object, const Field& field, int32_t value);
float readFloat(void* object, const Field& field);
void writeFloat(void* object, const Field& field, float value);
bool readBool(void* object, const Field& field);
void writeBool(void* object, const Field& field, bool value);
void setRef(void* object, const Field& field, void* value);

int32_t lengthArray(void* array);
char* arrayData(void* array);
int32_t lengthString(void* text);
const uint16_t* characterString(void* text);
// Two IL2CPP strings are compared by value; null on either side is never
// considered equal.
bool sameString(void* first, void* secondary);
void* stringUtf16New(const uint16_t* character, int32_t length);
// Text from the menu or a file arrives as UTF-8, while game arguments are of the
// IL2CPP string type.
void* stringFromUtf8(const char* text);
// IL2CPP text uses UTF-16, while log lines and file paths use UTF-8.
bool toUtf8(void* text, char* output, size_t size);
void* dataFieldStatic(void* klass);

bool nearValue(float value, float target);

// A runtime MethodInfo starts with the resolved code pointer.
void* addressMethod(void* method);
// A method whose body is only "return" has no body worth hooking.
bool ownBody(void* address);

// A System.Type object for a class, used by Unity's GetComponent(Type).
void* typeKlass(void* klass);

// Finds a class by name across all assemblies. Used for Unity classes that no
// target names, so they are not bound by scan().
void* findClass(const char* name);

// A simple name can belong to more than one class (for example _Tile data tile
// and _Tile behaviour), so a class is accepted only if it has the requested field.
void* findKlassField(const char* name, const char* nameField);
