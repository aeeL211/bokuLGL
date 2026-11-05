package com.android.support;

import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.CompoundButton;

/**
 * Sakelar pil gaya game: pil 52x26dp, garis 3dp #B28950, knob bulat 20dp yang
 * mengisi penuh bagian dalam pil.
 *
 * MATI : track #B28950, knob #CEA972 di kiri.
 * HIDUP: track #F8B551, knob #FFF799 di kanan.
 *
 * Tetap turunan CompoundButton supaya listener, aksesibilitas, dan
 * Preferences.changeFeatureBool berjalan sama seperti Switch bawaan. View-nya
 * setinggi 48dp agar area sentuh memenuhi ukuran minimum, pilnya digambar di
 * tengah.
 */
public class AnimatedSwitch extends CompoundButton {
    private static final long DURASI = 190L;
    private static final long DURASI_TEKAN = 90L;

    private static final float PIL_LEBAR_DP = 52f;
    private static final float PIL_TINGGI_DP = 26f;
    private static final float GARIS_DP = 3f;
    private static final float TEKAN_MELAR_DP = 4f;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArgbEvaluator warna = new ArgbEvaluator();
    private final RectF bidang = new RectF();

    // posisi 0 = mati, 1 = hidup (boleh sedikit melewati batas saat overshoot).
    private float posisi;
    // tekan 0..1 = knob melar saat jari menyentuh.
    private float tekan;
    private ValueAnimator animasiPosisi;
    private ValueAnimator animasiTekan;

    public AnimatedSwitch(Context context) {
        super(context);
        setBackground(null);
        setButtonDrawable(null);
        setPadding(0, 0, 0, 0);
        setClickable(true);
        setFocusable(true);
        posisi = isChecked() ? 1f : 0f;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int lebar = (int) Theme.dp(getContext(), PIL_LEBAR_DP);
        int tinggi = (int) Theme.dp(getContext(), 48f);
        setMeasuredDimension(resolveSize(lebar, widthMeasureSpec),
                resolveSize(tinggi, heightMeasureSpec));
    }

    /**
     * Nilai dari restore dipasang tanpa animasi karena view belum punya ukuran
     * saat Preferences.loadPrefBool dipanggil. Ketukan sesudah layout selalu
     * dianimasikan dari posisi sekarang, jadi ketukan cepat bisa dibalik tanpa
     * lompat.
     */
    @Override
    public void setChecked(boolean checked) {
        boolean berubah = isChecked() != checked;
        super.setChecked(checked);

        float tujuan = checked ? 1f : 0f;
        if (!berubah && posisi == tujuan)
            return;
        if (!isLaidOut()) {
            hentikanAnimasi();
            posisi = tujuan;
            invalidate();
            return;
        }
        animasiKe(tujuan);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (isLaidOut())
            tekanKe(isPressed() ? 1f : 0f);
        invalidate();
    }

    private void animasiKe(final float tujuan) {
        if (animasiPosisi != null)
            animasiPosisi.cancel();
        float jarak = Math.abs(tujuan - posisi);
        animasiPosisi = ValueAnimator.ofFloat(posisi, tujuan);
        animasiPosisi.setDuration((long) (DURASI * Math.max(0.45f, jarak)));
        // Geser mengerem halus, lalu sedikit memantul di ujung seperti saklar fisik.
        animasiPosisi.setInterpolator(jarak > 0.6f
                ? new OvershootInterpolator(0.9f) : new DecelerateInterpolator());
        animasiPosisi.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                posisi = (Float) animator.getAnimatedValue();
                invalidate();
            }
        });
        animasiPosisi.start();
    }

    private void tekanKe(float tujuan) {
        if (tekan == tujuan)
            return;
        if (animasiTekan != null)
            animasiTekan.cancel();
        animasiTekan = ValueAnimator.ofFloat(tekan, tujuan);
        animasiTekan.setDuration(DURASI_TEKAN);
        animasiTekan.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                tekan = (Float) animator.getAnimatedValue();
                invalidate();
            }
        });
        animasiTekan.start();
    }

    private void hentikanAnimasi() {
        if (animasiPosisi != null) {
            animasiPosisi.cancel();
            animasiPosisi = null;
        }
        if (animasiTekan != null) {
            animasiTekan.cancel();
            animasiTekan = null;
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        hentikanAnimasi();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        Context ctx = getContext();
        float lebar = getWidth();
        float tinggi = getHeight();
        float pilLebar = Math.min(lebar, Theme.dp(ctx, PIL_LEBAR_DP));
        float pilTinggi = Theme.dp(ctx, PIL_TINGGI_DP);
        float garis = Theme.dp(ctx, GARIS_DP);

        float kiri = (lebar - pilLebar) / 2f;
        float atas = (tinggi - pilTinggi) / 2f;
        float radius = pilTinggi / 2f;

        // Warna mengikuti posisi tetapi dijepit supaya overshoot tidak membuat
        // warna keluar dari rentang.
        float f = Math.max(0f, Math.min(1f, posisi));
        int warnaTrack = (Integer) warna.evaluate(f, Theme.SWITCH_OFF_TRACK, Theme.SWITCH_ON_TRACK);
        int warnaKnob = (Integer) warna.evaluate(f, Theme.SWITCH_OFF_KNOB, Theme.SWITCH_ON_KNOB);

        // Garis luar #B28950 sebagai pil penuh, lalu track di dalamnya. Saat mati
        // track memang berwarna sama dengan garis, persis seperti di game.
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(Theme.STROKE_KONTROL);
        bidang.set(kiri, atas, kiri + pilLebar, atas + pilTinggi);
        canvas.drawRoundRect(bidang, radius, radius, cat);

        cat.setColor(warnaTrack);
        bidang.set(kiri + garis, atas + garis, kiri + pilLebar - garis, atas + pilTinggi - garis);
        float radiusDalam = radius - garis;
        canvas.drawRoundRect(bidang, radiusDalam, radiusDalam, cat);

        // Knob bulat mengisi penuh tinggi bagian dalam. Saat ditekan knob melar
        // ke arah tengah supaya tepi luarnya tidak lompat.
        float diameter = pilTinggi - 2f * garis;
        float melar = Theme.dp(ctx, TEKAN_MELAR_DP) * tekan;
        float jalur = pilLebar - 2f * garis - diameter;
        float knobKiri = kiri + garis + jalur * posisi - melar * posisi;
        bidang.set(knobKiri, atas + garis, knobKiri + diameter + melar, atas + garis + diameter);

        cat.setColor(warnaKnob);
        canvas.drawRoundRect(bidang, diameter / 2f, diameter / 2f, cat);
    }
}
