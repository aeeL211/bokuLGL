#pragma once

#include <cstddef>
#include <cstdint>

// Tabel fungsi IL2CPP diikat sekali oleh ikatApi(). Tabel itu diambil lewat
// sebuah fungsi pengambil supaya tidak ada tabel yang bisa berubah di lingkup
// berkas.
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

// Nama sederhana yang sama bisa ada di beberapa assembly, jadi sebuah kelas baru
// diterima setelah salah satu metodenya cocok dengan nama dan jumlah argumen
// yang diminta.
void* classOf(const char* name);
void bindKlass(const char* name, void* klass);

struct Field {
    void* field;
    size_t offset;
};

bool findField(void* klass, const char* name, Field& result);
void* fieldClass(const Field& field);
// Untuk field array, fungsi ini mengembalikan kelas elemennya, bukan kelas array-nya.
void* fieldElementClass(const Field& field);

void* readPtr(void* objek, const Field& field);
int32_t readInt(void* objek, const Field& field);
void writeInt(void* objek, const Field& field, int32_t value);
float readFloat(void* objek, const Field& field);
void writeFloat(void* objek, const Field& field, float value);
bool readBool(void* objek, const Field& field);
void writeBool(void* objek, const Field& field, bool value);
void setRef(void* objek, const Field& field, void* value);

int32_t lengthArray(void* array);
char* arrayData(void* array);
int32_t lengthString(void* text);
const uint16_t* characterString(void* text);
// Dua string IL2CPP dibandingkan berdasarkan nilai; null di salah satu sisi
// tidak pernah dianggap sama.
bool sameString(void* first, void* secondary);
void* stringUtf16New(const uint16_t* character, int32_t length);
// Teks dari menu atau berkas datang sebagai UTF-8, sedangkan argumen game
// bertipe string IL2CPP.
void* stringFromUtf8(const char* text);
// Teks IL2CPP memakai UTF-16, sedangkan baris log dan path file memakai UTF-8.
bool toUtf8(void* text, char* keluaran, size_t size);
void* dataFieldStatic(void* klass);

bool nearValue(float value, float target);

// MethodInfo runtime diawali oleh pointer kode hasil pencarian.
void* addressMethod(void* method);
// Method yang isinya hanya "return" tidak punya badan yang layak dipasangi hook.
bool ownBody(void* address);

// Objek System.Type untuk sebuah kelas, dipakai GetComponent(Type) milik Unity.
void* typeKlass(void* klass);

// Mencari kelas dari namanya di seluruh assembly. Dipakai untuk kelas Unity yang
// tidak dinamai target mana pun, jadi tidak ikut terikat oleh scan().
void* findClass(const char* name);

// Nama sederhana bisa dipakai lebih dari satu kelas (misalnya _Tile data tile
// dan _Tile perilaku), jadi kelas hanya diterima bila punya field yang diminta.
void* findKlassField(const char* name, const char* nameField);
