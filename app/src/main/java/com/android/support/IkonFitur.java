package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.View;

import java.util.Locale;

/**
 * Ikon pixel-art 16x16 untuk setiap fitur, tiga nada seperti ikon game:
 * '1' terang (#CCAA77), '2' sedang (#BB8855), '3' gelap (#996633), '.' kosong.
 *
 * Kotak ikon menempel penuh di tepi kiri baris (tanpa margin dan tanpa garis),
 * persis seperti kotak ikon di menu game. Hanya sudut kiri yang melengkung,
 * dengan radius yang sama seperti baris.
 */
public class IkonFitur extends View {
    private static final char NADA_TERANG = '1';
    private static final char NADA_SEDANG = '2';
    private static final char NADA_GELAP = '3';

    /** Lebar kotak ikon dalam dp. Tingginya mengikuti tinggi baris. */
    public static final int LEBAR_DP = 48;

    private final Paint cat = new Paint();
    private final Path kotak = new Path();
    private final RectF area = new RectF();
    private final String[] pola;
    private final boolean sudutKiri;

    public static final String[] BELI = {
            "................",
            "111.............",
            "1111............",
            "..11............",
            "...111311311111.",
            "...111311311111.",
            "....2222222222..",
            "....1131131111..",
            ".....13113111...",
            ".....13113111...",
            ".....22222222...",
            "................",
            ".....11....11...",
            "....1311..1311..",
            ".....11....11...",
            "................",
    };

    public static final String[] SLOT = {
            "................",
            ".111111..111111.",
            ".133221..133221.",
            ".133221..133221.",
            ".122221..122221.",
            ".122221..122221.",
            ".111111..111111.",
            "................",
            "................",
            ".111111..111111.",
            ".133221..133221.",
            ".133221..133221.",
            ".122221..122221.",
            ".122221..122221.",
            ".111111..111111.",
            "................",
    };

    public static final String[] NAMA = {
            "................",
            "......2222......",
            "......2332......",
            "..111122221111..",
            ".11111111111111.",
            ".11111111111111.",
            ".11222211111111.",
            ".11233213333311.",
            ".11233211111111.",
            ".11333311111111.",
            ".11333313333311.",
            ".11111111111111.",
            ".11222222222211.",
            ".11111111111111.",
            "..111111111111..",
            "................",
    };

    public static final String[] SKIN = {
            "................",
            "................",
            "..11113..31111..",
            ".11111133111111.",
            ".11111111111111.",
            "1111111111111111",
            "1111111111221111",
            "2222111111222222",
            "2222111111222222",
            "....11111122....",
            "....11111122....",
            "....11111122....",
            "....11111122....",
            "....11111122....",
            "....11111122....",
            "................",
    };

    public static final String[] SENSOR = {
            "................",
            "................",
            "................",
            "................",
            ".....111111.....",
            "...1112222111...",
            "..111211322111..",
            ".11112113321111.",
            ".11112333321111.",
            "..111223322111..",
            "...1112222111...",
            ".....111111.....",
            "................",
            "................",
            "................",
            "................",
    };

    public static final String[] DUNIA = {
            "................",
            ".......11.......",
            "....11111111....",
            "...1111222111...",
            "..111111221111..",
            "..111111121121..",
            "..121111222221..",
            ".11211112222211.",
            ".11211122211111.",
            "..122122211111..",
            "..122222211111..",
            "..112222211111..",
            "...1122222111...",
            "....11111111....",
            ".......11.......",
            "................",
    };

    public static final String[] SANDI = {
            "................",
            "......1111......",
            ".....111111.....",
            "....11....11....",
            "....11....11....",
            "....11....11....",
            "....11....11....",
            "...1111111111...",
            "...1111111112...",
            "...1111111112...",
            "...1113333112...",
            "...1113333112...",
            "...1111331112...",
            "...1111331112...",
            "...2222222222...",
            "................",
    };

    public static final String[] DETAIL = {
            "................",
            ".....222222.....",
            "...2221111222...",
            "..221113311122..",
            "..211113311112..",
            ".22111111111122.",
            ".21111333111112.",
            ".21111133111112.",
            ".21111133111112.",
            ".21111133111112.",
            ".22111133111122.",
            "..211113311112..",
            "..221133331122..",
            "...2221111222...",
            ".....222222.....",
            "................",
    };

    public static final String[] CHAT = {
            "................",
            "................",
            "..111111111111..",
            ".11111111111111.",
            ".11111111111111.",
            ".11133133133111.",
            ".11133133133111.",
            ".11133133133111.",
            ".11111111111111.",
            ".11111111111111.",
            "..111111111111..",
            "...1111.........",
            "...111..........",
            "...11...........",
            "................",
            "................",
    };

    public static final String[] TEMBAK = {
            ".......11.......",
            ".......11.......",
            "....11111111....",
            "...1111111111...",
            "..111......111..",
            "..11........11..",
            "..11........11..",
            "1111...22...1111",
            "1111...22...1111",
            "..11........11..",
            "..11........11..",
            "..111......111..",
            "...1111111111...",
            "....11111111....",
            ".......11.......",
            ".......11.......",
    };

    public static final String[] PELURU = {
            "................",
            ".......11.......",
            "......1111......",
            ".....111122.....",
            ".....111122.....",
            ".....111122.....",
            ".....111122.....",
            ".....111122.....",
            ".....111122.....",
            ".....111122.....",
            "....33333333....",
            "....22222222....",
            "....22233222....",
            "....22222222....",
            "....33333333....",
            "................",
    };

    public static final String[] KOTORAN = {
            "................",
            ".......12.......",
            "......1112......",
            ".....111112.....",
            "....11111112....",
            "...2222222222...",
            "...1111111112...",
            "..111311113112..",
            "..111111111112..",
            ".22222222222222.",
            ".11111111111112.",
            "1111111111111112",
            "1111111111111112",
            "2222222222222222",
            "................",
            "................",
    };

    public static final String[] MAKANAN = {
            "................",
            "........3.11....",
            "........3111....",
            "...11111311.....",
            "..111111311111..",
            "..1111111111112.",
            "..111111111112..",
            "..111111111122..",
            "..111111111122..",
            "..111111111122..",
            "...1111111222...",
            "...1111112222...",
            "....11112222....",
            ".....222222.....",
            "................",
            "................",
    };

    public static final String[] PECAHAN = {
            "................",
            ".1111112........",
            ".111112.........",
            ".11112..........",
            ".1112......1112.",
            ".112....1111112.",
            ".12.....2111112.",
            ".2.......111112.",
            ".........111112.",
            ".........111112.",
            ".........211112.",
            ".211112...1222..",
            "..2112....2.....",
            "...22...........",
            "................",
            "................",
    };

    public static final String[] PENTAL = {
            "................",
            "................",
            "..2222..........",
            "..........1.....",
            "2222......11....",
            "..........111...",
            "...11111111111..",
            "...111111111111.",
            "...111111111111.",
            "...22222221111..",
            "..........111...",
            "2222......11....",
            "..........1.....",
            ".2222...........",
            "................",
            "................",
    };

    public static final String[] GETAR = {
            "................",
            "................",
            ".....111111.....",
            ".....122221.....",
            "...1.122221.1...",
            "...1.122221.1...",
            ".2.1.122221.1.2.",
            ".2.1.122221.1.2.",
            ".2.1.122221.1.2.",
            ".2.1.122221.1.2.",
            "...1.122221.1...",
            "...1.122221.1...",
            ".....111111.....",
            ".....112211.....",
            "................",
            "................",
    };

    public static final String[] DINDING = {
            "................",
            "................",
            "1111111311111111",
            "1111111311111111",
            "1111111311111111",
            "3333333333333333",
            "1113111111131111",
            "1113111111131111",
            "1113111111131111",
            "3333333333333333",
            "1111111311111111",
            "1111111311111111",
            "1111111311111111",
            "3333333333333333",
            "................",
            "................",
    };

    public static final String[] TITIK_X = {
            "................",
            "................",
            "................",
            "................",
            "....1......1....",
            "...11......11...",
            "..111......111..",
            ".11111111111111.",
            ".11122222222111.",
            "..111......111..",
            "...11......11...",
            "....1......1....",
            "................",
            "................",
            "................",
            "................",
    };

    public static final String[] TITIK_Y = {
            "................",
            ".......11.......",
            "......1111......",
            ".....111111.....",
            "....11112111....",
            ".......12.......",
            ".......12.......",
            ".......12.......",
            ".......12.......",
            ".......12.......",
            ".......12.......",
            "....11112111....",
            ".....111111.....",
            "......1111......",
            ".......11.......",
            "................",
    };

    public static final String[] PINDAH = {
            "................",
            ".....111111.....",
            "....11111111....",
            "...1111111111...",
            "...1111331111...",
            "...1113333111...",
            "...1113333111...",
            "...1111331111...",
            "...1111111111...",
            "....11111111....",
            ".....111111.....",
            "......1111......",
            ".......11.......",
            ".......11.......",
            ".....222222.....",
            "...2222222222...",
    };

    public static final String[] KECEPATAN = {
            ".........12.....",
            "........112.....",
            "........12......",
            ".......112......",
            "......112.......",
            "......112.......",
            ".....11111112...",
            "....11111112....",
            "...11111112.....",
            ".......112......",
            ".......12.......",
            "......112.......",
            "......12........",
            "......2.........",
            ".....2..........",
            "................",
    };

    public static final String[] GRAVITASI = {
            "................",
            "...........11...",
            ".....1.....11...",
            "...11111...11...",
            "..1111111..11...",
            "..1111111..11...",
            "..1111112..11...",
            "..1111122..11...",
            "...11122...11...",
            ".....2.....11...",
            ".........111111.",
            "..........1111..",
            "...........11...",
            "................",
            "1111111111111111",
            "2222222222222222",
    };

    public static final String[] OPASITAS = {
            "................",
            "................",
            ".....222131.....",
            "....22113131....",
            "...2111113131...",
            "..221111313131..",
            "..211111131313..",
            "..211111313131..",
            "..211111131313..",
            "..211111313131..",
            "..221111131313..",
            "...2111131313...",
            "....22111313....",
            ".....222313.....",
            "................",
            "................",
    };

    public static final String[] UKURAN = {
            "................",
            "........1111111.",
            "........1111111.",
            ".............11.",
            "...........1111.",
            "..........11111.",
            ".........111.11.",
            "........111..11.",
            ".11....111......",
            ".11...111.......",
            ".11..111........",
            ".11.111.........",
            ".11.11..........",
            ".1111111........",
            ".1111111........",
            "................",
    };

    public static final String[] GAMBAR = {
            "................",
            "................",
            ".11111111111111.",
            ".13333333333331.",
            ".13333333313331.",
            ".13333333111331.",
            ".13333333111331.",
            ".13333333313331.",
            ".13333223333331.",
            ".13332222311331.",
            ".13322222111131.",
            ".13222221111111.",
            ".11111111111111.",
            ".11111111111111.",
            "................",
            "................",
    };

    public static final String[] TELEGRAM = {
            "................",
            ".............1..",
            "...........111..",
            ".........11112..",
            "......1111112...",
            "....111111121...",
            "..11111111211...",
            "..1111111121....",
            "....11111211....",
            "......112111....",
            ".......21111....",
            "......221111....",
            "......22.11.....",
            "......22..1.....",
            "................",
            "................",
    };

    public static final String[] PEMAIN = {
            "................",
            "......1111......",
            ".....111111.....",
            "....11111112....",
            "....11111112....",
            "....11111112....",
            ".....1111112....",
            "......11122.....",
            ".....2222222....",
            "...11111111112..",
            "..1111111111122.",
            "..1111111111122.",
            "..1111111111122.",
            "..1111111111122.",
            "..2222222222222.",
            "................",
    };

    public static final String[] POSISI = {
            "................",
            "..11............",
            ".1111...........",
            ".1111...........",
            "..11............",
            "..11...1111.....",
            "..113331221.....",
            "..11...1221.....",
            "..11...1111.....",
            "..11....3.......",
            "..11....3.......",
            "..11....3...11..",
            "..1111111111111.",
            "..1111111111111.",
            "............11..",
            "................",
    };

    public static final String[] SIMPAN = {
            "................",
            "................",
            "..1112222221....",
            "..11122233211...",
            "..111222332112..",
            "..111222222112..",
            "..111111111112..",
            "..111111111112..",
            "..113333333312..",
            "..113111111312..",
            "..113333333312..",
            "..113111111312..",
            "..113333333312..",
            "..113333333312..",
            "................",
            "................",
    };

    public static final String[] ULANG = {
            "................",
            ".........1......",
            ".........11.....",
            ".........111....",
            ".....11111111...",
            "....11111111....",
            "...111...11.....",
            "...11....1......",
            "..111.......11..",
            "..111......111..",
            "..111......111..",
            "...11......11...",
            "...111....112...",
            "....11111112....",
            ".....111112.....",
            "................",
    };

    public static final String[] PENGATURAN = {
            "................",
            ".......11.......",
            ".......11.......",
            "...11.1111.11...",
            "...1111111111...",
            "....11111111....",
            "...111....111...",
            ".11111....11112.",
            ".11111....11122.",
            "...111....112...",
            "....11111112....",
            "...1111111222...",
            "...11.1112.22...",
            ".......12.......",
            ".......22.......",
            "................",
    };

    public IkonFitur(Context context, String[] pola, boolean sudutKiri) {
        super(context);
        this.pola = pola;
        this.sudutKiri = sudutKiri;
        cat.setFilterBitmap(false);
    }

    /** Pola untuk satu nomor fitur. Nomor yang tidak punya ikon memakai roda gigi. */
    public static String[] pola(int fitur) {
        switch (fitur) {
            case -1:
                return SIMPAN;
            case -6:
                return GAMBAR;
            case -5:
                return UKURAN;
            case -4:
                return OPASITAS;
            case 0:
                return BELI;
            case 1:
                return SLOT;
            case 2:
                return NAMA;
            case 3:
                return SKIN;
            case 4:
                return SENSOR;
            case 5:
                return DUNIA;
            case 6:
                return SANDI;
            case 7:
                return DETAIL;
            case 9:
                return CHAT;
            case 11:
                return TEMBAK;
            case 12:
                return PELURU;
            case 13:
                return KOTORAN;
            case 14:
                return MAKANAN;
            case 15:
                return PECAHAN;
            case 16:
                return PENTAL;
            case 19:
                return DINDING;
            case 20:
                return TITIK_X;
            case 21:
                return TITIK_Y;
            case 22:
                return PINDAH;
            case 23:
                return KECEPATAN;
            case 24:
                return GRAVITASI;
            default:
                return PENGATURAN;
        }
    }

    /**
     * Pola untuk satu fitur. Nama fitur dicek lebih dulu untuk ikon yang
     * maknanya jelas dari namanya (getar, terpental, pecahan, kotoran, makanan),
     * jadi ikon tetap benar walau nomor fitur di native berubah. Bila tidak ada
     * yang cocok, dipakai pola berdasarkan nomor.
     */
    public static String[] pola(int fitur, String nama) {
        if (nama != null) {
            String n = nama.toLowerCase(Locale.ROOT);
            if (n.contains("getar"))
                return GETAR;
            if (n.contains("pental"))
                return PENTAL;
            if (n.contains("pecahan"))
                return PECAHAN;
            if (n.contains("kotoran"))
                return KOTORAN;
            if (n.contains("makanan"))
                return MAKANAN;
            // Setelan memakai nomor negatif. Nama dicek supaya ikonnya benar walau
            // nomornya berbeda, dan fitur game tidak ikut terpengaruh.
            if (fitur < 0) {
                if (n.contains("simpan") || n.contains("save") || n.contains("preferen"))
                    return SIMPAN;
                if (n.contains("opasitas") || n.contains("opacity") || n.contains("transparan"))
                    return OPASITAS;
                if (n.contains("ukuran") || n.contains("size"))
                    return UKURAN;
                if (n.contains("gambar") || n.contains("image") || n.contains("url")
                        || n.contains("ikon") || n.contains("icon"))
                    return GAMBAR;
            }
        }
        return pola(fitur);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int lebar = (int) Theme.dp(getContext(), LEBAR_DP);
        int tinggi = (int) Theme.dp(getContext(), LEBAR_DP);
        setMeasuredDimension(resolveSize(lebar, widthMeasureSpec),
                resolveSize(tinggi, heightMeasureSpec));
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float lebar = getWidth();
        float tinggi = getHeight();
        float radius = Theme.dp(getContext(), Theme.RADIUS_ROW);

        float[] sudut = sudutKiri
                ? new float[]{radius, radius, 0f, 0f, 0f, 0f, radius, radius}
                : new float[]{radius, radius, radius, radius, radius, radius, radius, radius};

        cat.setAntiAlias(true);
        cat.setStyle(Paint.Style.FILL);
        // Hanya view yang bisa diketuk yang pernah berstatus ditekan.
        cat.setColor(isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        kotak.reset();
        area.set(0f, 0f, lebar, tinggi);
        kotak.addRoundRect(area, sudut, Path.Direction.CW);
        canvas.drawPath(kotak, cat);

        gambarPola(canvas, lebar, tinggi);
    }

    private void gambarPola(Canvas canvas, float lebar, float tinggi) {
        if (pola == null || pola.length == 0)
            return;

        // Satu sel selalu bilangan bulat piksel supaya tepi ikon tetap tajam.
        // Ikon memakai sekitar 70% sisi terpendek kotak, seperti di game.
        float sel = (float) Math.floor(Math.min(lebar, tinggi) * 0.70 / 16.0);
        if (sel < 1f)
            sel = 1f;

        float kiri = Math.round((lebar - sel * 16f) / 2f);
        float atas = Math.round((tinggi - sel * pola.length) / 2f);

        cat.setAntiAlias(false);
        cat.setStyle(Paint.Style.FILL);
        for (int y = 0; y < pola.length; y++) {
            String baris = pola[y];
            for (int x = 0; x < baris.length(); x++) {
                char isi = baris.charAt(x);
                if (isi == NADA_TERANG)
                    cat.setColor(Theme.ICON_LIGHT);
                else if (isi == NADA_SEDANG)
                    cat.setColor(Theme.ICON_MID);
                else if (isi == NADA_GELAP)
                    cat.setColor(Theme.ICON_DARK);
                else
                    continue;
                canvas.drawRect(kiri + x * sel, atas + y * sel,
                        kiri + (x + 1) * sel, atas + (y + 1) * sel, cat);
            }
        }
        cat.setAntiAlias(true);
    }
}
