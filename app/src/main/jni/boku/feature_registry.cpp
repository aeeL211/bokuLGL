#include "feature_registry.h"

#include "fitur.h"

void terapkanPerubahanFitur(jint nomor, jboolean aktif, jint nilai) {
    Fitur& sakelar = fitur();
    switch (nomor) {
    case fitur_nomor::kIap:
        sakelar.iap = aktif;
        break;
    case fitur_nomor::kSlot:
        sakelar.slot = aktif;
        break;
    case fitur_nomor::kInputTeks:
        sakelar.inputTeks = aktif;
        break;
    case fitur_nomor::kKataKasar:
        sakelar.kataKasar = aktif;
        break;
    case fitur_nomor::kSimpanDuniaDanRoom:
        sakelar.simpanDunia = aktif;
        sakelar.ukuranRoom = aktif;
        break;
    case fitur_nomor::kSandiRoom:
        sakelar.sandiRoom = aktif;
        break;
    case fitur_nomor::kPrivasi:
        sakelar.privasi = aktif;
        break;
    case fitur_nomor::kChatPribadi:
        sakelar.chatPribadi = aktif;
        break;
    case fitur_nomor::kTembakOtomatis:
        sakelar.tembakOtomatis = aktif;
        break;
    case fitur_nomor::kBusterPenuh:
        sakelar.busterPenuh = aktif;
        break;
    case fitur_nomor::kKotoran:
        sakelar.kotoran = aktif;
        break;
    case fitur_nomor::kMakanan:
        sakelar.makanan = aktif;
        break;
    case fitur_nomor::kGetar:
        sakelar.getar = aktif;
        break;
    case fitur_nomor::kTerpental:
        sakelar.terpental = aktif;
        break;
    case fitur_nomor::kNoClip:
        sakelar.noClip = aktif;
        break;
    case fitur_nomor::kTeleportX:
        permintaanTeleport().x.store(nilai);
        break;
    case fitur_nomor::kTeleportY:
        permintaanTeleport().y.store(nilai);
        break;
    case fitur_nomor::kTeleportJalankan:
        permintaanTeleport().menunggu.store(true);
        break;
    case fitur_nomor::kKecepatan:
        sakelar.kecepatanPemain.store(nilai);
        break;
    case fitur_nomor::kGravitasi:
        sakelar.gravitasiPemain.store(nilai);
        break;
    case fitur_nomor::kResetKecepatan:
        sakelar.kecepatanPemain.store(fitur_nomor::kNilaiKecepatanAwal);
        break;
    case fitur_nomor::kResetGravitasi:
        sakelar.gravitasiPemain.store(fitur_nomor::kNilaiGravitasiAwal);
        break;
    case fitur_nomor::kRentangSkin:
        sakelar.rentangSkin.store(nilai);
        break;
    case fitur_nomor::kPecahan:
        sakelar.pecahan = aktif;
        break;
    default:
        break;
    }
}
