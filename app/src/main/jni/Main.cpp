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
    // dibuka satu per satu. Nomor fitur ditulis eksplisit di isi section.
    const char *features[] = {
            OBFUSCATE("Collapse_Ekonomi"),
            OBFUSCATE("CollapseAdd_0_Toggle_True_Simulasi pembelian"),
            OBFUSCATE("CollapseAdd_1_Toggle_True_Slot penyimpanan tanpa batas"),

            OBFUSCATE("Collapse_Input dan Profil"),
            OBFUSCATE("CollapseAdd_2_Toggle_True_Batas input dan profil"),
            OBFUSCATE("CollapseAdd_3_Toggle_True_Rentang slider skin"),
            OBFUSCATE("CollapseAdd_4_Toggle_True_Bebas kata terlarang"),

            OBFUSCATE("Collapse_Room dan Dunia"),
            OBFUSCATE("CollapseAdd_5_Toggle_True_Simpan dunia dan ukuran room"),
            OBFUSCATE("CollapseAdd_6_Toggle_True_Sandi room kosong"),
            OBFUSCATE("CollapseAdd_PlayerList_Tamu di room"),

            OBFUSCATE("Collapse_Akun dan Privasi"),
            OBFUSCATE("CollapseAdd_7_Toggle_True_Privasi akun"),

            OBFUSCATE("Collapse_Chat dan Sosial"),
            OBFUSCATE("CollapseAdd_9_Toggle_True_Pesan pribadi di chat room"),
            OBFUSCATE("CollapseAdd_10_Toggle_True_Tombol tendang tamu"),

            OBFUSCATE("Collapse_Aksi Pemain"),
            OBFUSCATE("CollapseAdd_11_Toggle_True_Tembak otomatis"),
            OBFUSCATE("CollapseAdd_12_Toggle_True_Buster amunisi penuh"),
            OBFUSCATE("CollapseAdd_13_Toggle_True_Kotoran di tangan"),
            OBFUSCATE("CollapseAdd_14_Toggle_True_Makanan di tangan"),
            OBFUSCATE("CollapseAdd_16_Toggle_True_Tanpa terpental"),

            OBFUSCATE("Collapse_Optimisasi"),
            OBFUSCATE("CollapseAdd_15_Toggle_True_Matikan getar dan puing"),

            OBFUSCATE("Collapse_Gerak"),
            OBFUSCATE("CollapseAdd_19_Toggle_No Clip"),
            OBFUSCATE("CollapseAdd_20_InputValue_600_X"),
            OBFUSCATE("CollapseAdd_21_InputValue_300_Y"),
            OBFUSCATE("CollapseAdd_22_Button_Teleport"),
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
        case 3:
            sakelar.rentangSkin = boolean;
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
        case 10:
            sakelar.tombolTendang = boolean;
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
            sakelar.efek = boolean;
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
        default:
            break;
    }
}
