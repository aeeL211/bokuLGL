#pragma once

#include <jni.h>

// Nomor fitur adalah kontrak dengan Java: nilainya tertulis di daftar fitur dan
// dikirim kembali oleh Preferences.Changes. Konstanta ini hanya menamai angka
// yang sudah ada; urutannya tidak boleh berubah.
namespace fitur_nomor {
constexpr jint kIap = 0;
constexpr jint kSlot = 1;
constexpr jint kInputTeks = 2;
constexpr jint kKataKasar = 4;
constexpr jint kSimpanDuniaDanRoom = 5;
constexpr jint kSandiRoom = 6;
constexpr jint kPrivasi = 7;
constexpr jint kChatPribadi = 9;
constexpr jint kTembakOtomatis = 11;
constexpr jint kBusterPenuh = 12;
constexpr jint kKotoran = 13;
constexpr jint kMakanan = 14;
constexpr jint kGetar = 15;
constexpr jint kTerpental = 16;
constexpr jint kNoClip = 19;
constexpr jint kTeleportX = 20;
constexpr jint kTeleportY = 21;
constexpr jint kTeleportJalankan = 22;
constexpr jint kKecepatan = 23;
constexpr jint kGravitasi = 24;
constexpr jint kResetKecepatan = 25;
constexpr jint kResetGravitasi = 26;
constexpr jint kRentangSkin = 27;
constexpr jint kPecahan = 28;
constexpr jint kIzinRoomLain = 29;
constexpr jint kNilaiKecepatanAwal = 10;
constexpr jint kNilaiGravitasiAwal = 10;
} // namespace fitur_nomor

// Menulis satu perubahan dari menu ke state atomik. Nomor yang tidak dikenal
// diabaikan, sama seperti switch bawaan.
void terapkanPerubahanFitur(jint nomor, jboolean aktif, jint nilai);
