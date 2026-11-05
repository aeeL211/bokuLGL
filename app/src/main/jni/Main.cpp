#include <jni.h>

#include "Includes/obfuscate.h"
#include "Menu/Menu.hpp"
#include "boku/fitur.h"

// Nomor fitur ditulis eksplisit supaya urutannya tidak bergeser saat entri baru
// disisipkan. Collapse dan PlayerList tidak dihitung sebagai fitur.
// Toggle, ButtonOnOff, dan CheckBox menyala sejak awal bila memakai awalan True_.
jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    (void)context;
    jobjectArray ret;

    // Setiap kategori adalah satu header Collapse dan isinya memakai awalan
    // CollapseAdd_, jadi seluruh section tertutup saat menu dibuka dan tinggal
    // dibuka satu per satu. Nomor fitur ditulis eksplisit di isi section, jadi
    // memindahkan sebuah baris ke section lain tidak menggeser nomor mana pun
    // dan Changes() tetap memakai nomor yang sama.
    const char *features[] = {
            OBFUSCATE("Collapse_Room dan Dunia"),
            OBFUSCATE("CollapseAdd_6_Toggle_True_Masuk room tanpa sandi"),
            OBFUSCATE("CollapseAdd_9_Toggle_True_Pesan pribadi ikut tampil di chat"),
            OBFUSCATE("CollapseAdd_5_Toggle_True_Simpan dunia dan room besar"),
            OBFUSCATE("CollapseAdd_PlayerList_Pemain di room ini"),

            OBFUSCATE("Collapse_Nama dan Profil"),
            OBFUSCATE("CollapseAdd_2_Toggle_True_Nama dan bio tanpa batas"),
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

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

// Setiap sakelar hanya menulis nilai. Hook membacanya pada tiap panggilan, jadi
// tidak ada hook yang dipasang atau dilepas saat fitur diubah.
void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value,
             jlong Lvalue, jboolean boolean, jstring text) {
    (void)env;
    (void)clazz;
    (void)obj;
    (void)featName;
    (void)Lvalue;
    (void)text;

    Fitur &sakelar = fitur();
    switch (featNum) {
        case 0:
            sakelar.iap = boolean;
            break;
        case 1:
            sakelar.slot = boolean;
            break;
        case 2:
            sakelar.inputTeks = boolean;
            break;
        case 4:
            sakelar.kataKasar = boolean;
            break;
        case 5:
            sakelar.simpanDunia = boolean;
            sakelar.ukuranRoom = boolean;
            break;
        case 6:
            sakelar.sandiRoom = boolean;
            break;
        case 7:
            sakelar.privasi = boolean;
            break;
        case 9:
            sakelar.chatPribadi = boolean;
            break;
        case 11:
            sakelar.tembakOtomatis = boolean;
            break;
        case 12:
            sakelar.busterPenuh = boolean;
            break;
        case 13:
            sakelar.kotoran = boolean;
            break;
        case 14:
            sakelar.makanan = boolean;
            break;
        case 15:
            sakelar.getar = boolean;
            break;
        case 16:
            sakelar.terpental = boolean;
            break;
        case 19:
            sakelar.noClip = boolean;
            break;
        case 20:
            permintaanTeleport().x.store(value);
            break;
        case 21:
            permintaanTeleport().y.store(value);
            break;
        case 22:
            permintaanTeleport().menunggu.store(true);
            break;
        case 23:
            sakelar.kecepatanPemain.store(value);
            break;
        case 24:
            sakelar.gravitasiPemain.store(value);
            break;
        case 25:
            sakelar.kecepatanPemain.store(10);
            break;
        case 26:
            sakelar.gravitasiPemain.store(10);
            break;
        case 27:
            sakelar.rentangSkin.store(value);
            break;
        case 28:
            sakelar.pecahan = boolean;
            break;
        default:
            break;
    }
}
