#include "Menu/FeatureList.hpp"

#include "Includes/obfuscate.h"
#include "boku/jni_cache.h"
#include "boku/jni_util.h"

namespace {

jobjectArray salinDaftar(JNIEnv *env, const char *const *teks, int jumlah) {
    jclass kelasTeks = kelasString(env);
    if (!kelasTeks)
        return nullptr;

    LocalRef<jobjectArray> hasil(env, env->NewObjectArray(jumlah, kelasTeks, nullptr));
    if (!hasil) {
        bersihkanEksepsi(env, "NewObjectArray daftar menu");
        return nullptr;
    }

    for (int i = 0; i < jumlah; i++) {
        LocalRef<jstring> isi(env, env->NewStringUTF(teks[i]));
        if (!isi) {
            bersihkanEksepsi(env, "NewStringUTF daftar menu");
            break;
        }
        env->SetObjectArrayElement(hasil.get(), i, isi.get());
        bersihkanEksepsi(env, "SetObjectArrayElement daftar menu");
    }
    return hasil.lepas();
}

} // namespace

// Setiap kategori adalah satu header Collapse dan isinya memakai awalan
// CollapseAdd_, jadi seluruh section tertutup saat menu dibuka. Nomor fitur
// ditulis eksplisit di isi section, jadi memindahkan baris tidak menggeser nomor
// mana pun dan Changes() tetap memakai nomor yang sama.
jobjectArray daftarFitur(JNIEnv *env) {
    static const char *const fitur[] = {
            OBFUSCATE("Collapse_Room dan Dunia"),
            OBFUSCATE("CollapseAdd_6_Toggle_True_Masuk room tanpa sandi"),
            OBFUSCATE("CollapseAdd_9_Toggle_True_Pesan pribadi ikut tampil di chat"),
            OBFUSCATE("CollapseAdd_5_Toggle_True_Simpan dunia dan room besar"),
            OBFUSCATE("CollapseAdd_PlayerList_Pemain di room ini"),

            OBFUSCATE("Collapse_Nama dan Profil"),
            OBFUSCATE("CollapseAdd_2_Toggle_True_Nama, tanggal lahir, dan sandi tanpa batas"),
            OBFUSCATE("CollapseAdd_27_SeekBar_Jarak geser skin_5_100_25_0"),
            OBFUSCATE("CollapseAdd_4_Toggle_True_Tanpa sensor kata kasar"),
            OBFUSCATE("CollapseAdd_7_Toggle_True_Tampilkan detail akun yang disembunyikan"),

            OBFUSCATE("Collapse_Belanja"),
            OBFUSCATE("CollapseAdd_0_Toggle_True_Beli item tanpa bayar"),
            OBFUSCATE("CollapseAdd_1_Toggle_True_Slot simpan tanpa batas"),

            OBFUSCATE("Collapse_Senjata dan Item"),
            OBFUSCATE("CollapseAdd_11_Toggle_True_Tembak terus selama ditahan"),
            OBFUSCATE("CollapseAdd_12_Toggle_True_Buster selalu peluru terkuat"),
            OBFUSCATE("CollapseAdd_13_Toggle_True_Kotoran bisa dilempar berulang"),
            OBFUSCATE("CollapseAdd_14_Toggle_True_Makanan tetap di tangan"),
            OBFUSCATE("CollapseAdd_16_Toggle_True_Tidak terpental saat meledak"),

            OBFUSCATE("Collapse_Performa"),
            OBFUSCATE("CollapseAdd_15_Toggle_True_Getar kamera dimatikan"),
            OBFUSCATE("CollapseAdd_28_Toggle_True_Pecahan tile dimatikan"),

            OBFUSCATE("Collapse_Gerak"),
            OBFUSCATE("CollapseAdd_19_Toggle_Tembus dinding"),
            OBFUSCATE("CollapseAdd_23_SeekBar_Kecepatan pemain_5_50_10_1"),
            OBFUSCATE("CollapseAdd_24_SeekBar_Gravitasi pemain_0_30_10_1"),
            OBFUSCATE("CollapseAdd_LivePos_Posisi pemain"),
            OBFUSCATE("CollapseAdd_20_InputValue_1024_Titik X tujuan"),
            OBFUSCATE("CollapseAdd_21_InputValue_1024_Titik Y tujuan"),
            OBFUSCATE("CollapseAdd_22_Button_Pindahkan pemain"),
    };
    return salinDaftar(env, fitur, static_cast<int>(sizeof(fitur) / sizeof(fitur[0])));
}

jobjectArray daftarSetelan(JNIEnv *env) {
    static const char *const setelan[] = {
            OBFUSCATE("Category_Pengaturan"),
            OBFUSCATE("-1_Toggle_Simpan preferensi"),
            OBFUSCATE("Category_Ikon"),
            OBFUSCATE("-4_SeekBar_Opasitas ikon_10_100_70_0"),
            OBFUSCATE("-5_SeekBar_Ukuran ikon_30_80_45_0"),
            OBFUSCATE("-6_InputText_Sumber gambar ikon"),
            OBFUSCATE("ButtonLink_Lapor bug/saran_https://t.me/aeeL2"),
    };
    return salinDaftar(env, setelan, static_cast<int>(sizeof(setelan) / sizeof(setelan[0])));
}
