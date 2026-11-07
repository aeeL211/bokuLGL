package com.android.support;

import java.util.Locale;

/**
 * Dialog text shown when a feature row is long-pressed.
 *
 * The content follows the README, so when a feature's behavior changes, the text
 * here changes with it. Rows whose number is computed by featureList itself
 * (player list, player position, link) are recognized by name first
 * so they do not collide with feature number 0.
 */
public final class Explanation {
    private Explanation() {
    }

    public static String forTarget(int features, String name) {
        String n = name == null ? "" : name.toLowerCase(Locale.ROOT);

        if (n.contains("pemain di room"))
            return "Menampilkan host, pemain sendiri, dan tamu yang sedang ada di room. "
                    + "Tombol Copy avatar menyimpan potret pemain ke slot potret kosong "
                    + "pertama; Copy skin menyimpan modelnya ke slot model kosong pertama. "
                    + "Ketuk Kick untuk kick sementara; tahan tombolnya untuk kick "
                    + "permanen yang juga menghapus tamu dari daftar. Kartu host punya tombol "
                    + "Kick host yang mengirim paket berulang, bisa membuat host lag lalu keluar "
                    + "dan menutup room. Tombol Kick semua menendang seluruh tamu secara "
                    + "permanen tanpa menyentuh host atau pemain sendiri. Daftar dibaca sekali "
                    + "per detik selama barisnya terlihat.";
        if (n.contains("posisi pemain"))
            return "Menampilkan tile dan koordinat dunia pemain sekali per detik selama "
                    + "barisnya terlihat. Tile Y memakai satuan UI game, jadi 0 adalah tengah "
                    + "dunia dan angkanya bisa langsung dipakai untuk mengisi titik teleport.";
        if (n.contains("lapor"))
            return "Membuka Telegram @aeeL2 untuk lapor bug atau saran.";

        switch (features) {
            case 0:
                return "Item bisa dibeli tanpa Play Billing dan tanpa biaya. Semua tile, model, "
                        + "dan melody langsung terbuka tanpa perlu dibeli.";
            case 1:
                return "Semua jenis slot penyimpanan terbuka tanpa batas, termasuk slot model "
                        + "dan melody.";
            case 2:
                return "Batas nama profil, tanggal lahir, dan kotak sandi room dinaikkan. Teks "
                        + "perkenalan tetap memakai batas bawaan game.";
            case 4:
                return "Kata terlarang tidak disaring lagi di perangkat ini. Pemain lain tetap "
                        + "memakai filter mereka sendiri.";
            case 5:
                return "Save world tetap bisa dilakukan walaupun host mematikan izinnya, dan "
                        + "room di atas batas ukuran perangkat tetap terdaftar dan bisa dimasuki. "
                        + "Ukuran dunia baru bisa dipilih sampai 600x300.";
            case 6:
                return "Room bersandi bisa dimasuki tanpa mengisi sandi.";
            case 7:
                return "Baris privasi akun menampilkan detail yang dipilih pemiliknya, bukan "
                        + "menyembunyikannya.";
            case 9:
                return "Pesan pribadi ikut tampil di chat room sebagai [PM -> nama penerima] dan "
                        + "dicatat di log.";
            case 11:
                return "Pistol dan buster menembak berulang selama tombol interaksi ditahan.";
            case 12:
                return "Buster selalu memakai amunisi terkuat.";
            case 13:
                return "Kotoran tetap di tangan, bisa dilempar berulang dengan kekuatan penuh dan "
                        + "arah sesuai bidikan.";
            case 14:
                return "Gingerbread dan hot cross bun tetap di tangan setelah dimakan.";
            case 15:
                return "Getar kamera dibuang dari dua titik masuknya, jadi layar tidak bergetar "
                        + "saat ledakan atau tembakan.";
            case 16:
                return "Ledakan tidak lagi membuat pemain terpental.";
            case 19:
                return "Kotak gerak pemain dilebarkan ke kiri, kanan, dan atas pada tiap "
                        + "pemeriksaan fisika tile. Sisi bawah dibiarkan supaya lantai tetap "
                        + "dipijak.";
            case 20:
                return "Koordinat tile X dunia untuk titik tujuan teleport. X 0 adalah tepi kiri "
                        + "dunia. Isi X dan Y, lalu tekan Pindahkan pemain.";
            case 21:
                return "Koordinat Y tujuan dalam satuan UI game. Nilai 0 adalah tengah dunia, "
                        + "dan Y UI dihitung dari Y tile dikurangi setengah tinggi dunia yang "
                        + "sedang dipakai. Isi X dan Y, lalu tekan Pindahkan pemain.";
            case 22:
                return "Memindahkan pemain ke titik X/Y. Y menu digeser setengah tinggi dunia "
                        + "supaya sama dengan Y UI game, lalu jarak tile dikonversi dengan ukuran "
                        + "tile (pixel_Size) dari posisi sekarang. Tile dan dunia ditulis bersama, "
                        + "kecepatan dinolkan, dan tile di sekitar badan dibangun ulang. Input di "
                        + "luar -1024..1024 ditolak.";
            case 23:
                return "Mengalikan speed.add.x pemain lokal tepat sebelum Pos__Check menghitung "
                        + "langkah berikutnya, lalu nilai aslinya dikembalikan. Hasil skala dibatasi "
                        + "ke ±_speed.limit game, yaitu pixel_Size - 1 atau 15 satuan, jadi langkah "
                        + "tidak pernah melewati satu tile. Langkah normal sudah menyentuh batas itu "
                        + "di sekitar 2,6x, jadi di atas angka tersebut slider tetap bisa dinaikkan "
                        + "tetapi jarak per frame tidak bertambah. Rentang 0,5x sampai 5,0x; ketuk "
                        + "angka untuk kembali ke 1,0x.";
            case 24:
                return "Menskalakan komponen turun speed.add.y pemain lokal tepat sebelum "
                        + "Pos__Check, jadi berlaku untuk semua gerakan termasuk terbang, "
                        + "berenang, dan meluncur. Gerak turun juga dibatasi ke ±_speed.limit game "
                        + "supaya tidak melompati satu tile. 0,0x membuang tarikan turun sehingga "
                        + "pemain melayang, tetapi dorongan lompat tetap ada; naikkan lagi untuk "
                        + "turun. Rentang 0,0x sampai 3,0x; ketuk angka untuk kembali ke 1,0x.";
            case 27:
                return "Batas geser posisi skin X/Y dan potret: 5 sampai 100. Nilai 25 berarti "
                        + "-25..+25 dan 5 berarti bawaan game -5..+5. Nilai baru dipakai saat "
                        + "editor skin dibuka lagi.";
            case 28:
                return "Puing tile tidak dimunculkan saat tile pecah. Tile dan rekamannya tetap "
                        + "utuh.";
            case 29:
                return "Memaksa host menerima pemasangan, penggantian, dan penghapusan tile "
                        + "serta penambahan dan pengambilan objek dari tamu, termasuk ledakan "
                        + "TNT. Aksi tetap dikirim lewat jaringan, jadi kamu harus sudah "
                        + "diterima sebagai tamu di room itu. Tile di area yang dilindungi dan "
                        + "perubahan pintu yang sudah ada tetap ditolak host.";
            case 30:
                return "Menyimpan skin dan potret setiap tamu yang masuk room secara otomatis. "
                        + "Skin ditulis ke slot model kosong pertama, potret ke slot potret "
                        + "kosong pertama, memakai penulis bawaan game. Setiap tamu baru berarti "
                        + "berkas baru, jadi room ramai bisa membuat game terasa lebih berat. "
                        + "Mati secara bawaan dan hanya menyala setelah konfirmasi.";
            case -1:
                return "Menyimpan pilihan fitur ke preferensi. Saat dimatikan, semua preferensi "
                        + "dibersihkan.";
            case -4:
                return "Transparansi ikon melayang, 10% sampai 100%.";
            case -5:
                return "Ukuran ikon melayang, 30 sampai 80 dp.";
            case -6:
                return "Sumber ikon melayang: URL, data URI, atau path berkas gambar/GIF. "
                        + "Kosongkan untuk ikon bawaan. SVG tidak didukung.";
            default:
                return "Penjelasan fitur ini belum tersedia.";
        }
    }
}
