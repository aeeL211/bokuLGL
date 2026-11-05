package com.android.support;

import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.widget.CompoundButton;

/**
 * Sakelar pil 52x26dp dengan animasi 170 ms.
 *
 * Tetap turunan CompoundButton supaya listener, aksesibilitas, dan
 * Preferences.changeFeatureBool berjalan sama seperti Switch bawaan. View-nya
 * setinggi 48dp agar area sentuh memenuhi ukuran minimum, sedangkan pilnya
 * digambar di tengah.
 */
public class AnimatedSwitch extends CompoundButton {
    private static final long DURASI = 170L;
    private static final long DURASI_REMAS = 90L;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArgbEvaluator warna = new ArgbEvaluator();

    // posisi 0 = mati, 1 = hidup. remas 0..1 adalah squash saat ditekan.
    private float posisi;
    private float remas;
    private ValueAnimator animasiPosisi;
    private ValueAnimator animasiRemas;

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
        int lebar = (int) Theme.dp(getContext(), 52f);
        int tinggi = (int) Theme.dp(getContext(), 48f);
        setMeasuredDimension(resolveSize(lebar, widthMeasureSpec),
                resolveSize(tinggi, heightMeasureSpec));
    }

    /**
     * Nilai dari restore dipasang tanpa animasi karena view belum punya ukuran
     * saat Preferences.loadPrefBool dipanggil. Ketukan pengguna sesudah layout
     * selalu dianimasikan dari posisi sekarang, jadi ketukan cepat bisa dibalik.
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
            remasKe(isPressed() ? 1f : 0f);
        invalidate();
    }

    private void animasiKe(final float tujuan) {
        hentikanAnimasi();
        animasiPosisi = ValueAnimator.ofFloat(posisi, tujuan);
        animasiPosisi.setDuration(DURASI);
        animasiPosisi.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                posisi = (Float) animator.getAnimatedValue();
                invalidate();
            }
        });
        animasiPosisi.start();
    }

    private void remasKe(float tujuan) {
        if (remas == tujuan)
            return;
        if (animasiRemas != null)
            animasiRemas.cancel();
        animasiRemas = ValueAnimator.ofFloat(remas, tujuan);
        animasiRemas.setDuration(DURASI_REMAS);
        animasiRemas.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                remas = (Float) animator.getAnimatedValue();
                invalidate();
            }
        });
        animasiRemas.start();
    }

    private void hentikanAnimasi() {
        if (animasiPosisi != null) {
            animasiPosisi.cancel();
            animasiPosisi = null;
        }
        if (animasiRemas != null) {
            animasiRemas.cancel();
            animasiRemas = null;
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        hentikanAnimasi();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float lebar = getWidth();
        float tinggi = getHeight();
        float trackTinggi = Theme.dp(getContext(), 26f);
        float atas = (tinggi - trackTinggi) / 2f;

        // Squash: pil memendek sedikit ke tengah saat ditekan.
        float skala = 1f - 0.05f * remas;
        float tengah = lebar / 2f;
        float kiri = tengah - (lebar / 2f) * skala;
        float kanan = tengah + (lebar / 2f) * skala;
        float radius = trackTinggi / 2f;
        RectF track = new RectF(kiri, atas, kanan, atas + trackTinggi);

        int warnaTrack = (Integer) warna.evaluate(posisi, Theme.SWITCH_OFF_TRACK,
                Theme.SWITCH_ON_TRACK);
        int warnaKnob = (Integer) warna.evaluate(posisi, Theme.SWITCH_OFF_KNOB,
                Theme.SWITCH_ON_KNOB);

        cat.setStyle(Paint.Style.FILL);
        cat.setColor(warnaTrack);
        canvas.drawRoundRect(track, radius, radius, cat);

        // Garis pil memakai outline panel, bukan warna kontrol: track mati
        // memang berwarna sama dengan garis kontrol sehingga tidak akan terbaca.
        float tebal = Theme.dp(getContext(), Theme.TEBAL_KONTROL);
        float masuk = tebal / 2f;
        RectF garis = new RectF(kiri + masuk, atas + masuk, kanan - masuk,
                atas + trackTinggi - masuk);
        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(tebal);
        cat.setColor(Theme.OUTLINE);
        canvas.drawRoundRect(garis, radius, radius, cat);

        float jarak = Theme.dp(getContext(), 4f);
        float sisiKnob = Theme.dp(getContext(), 18f);
        float lebarKnob = sisiKnob + Theme.dp(getContext(), 4f) * remas;
        float tinggiKnob = sisiKnob - Theme.dp(getContext(), 2f) * remas;
        float jalur = (kanan - kiri) - 2f * jarak - sisiKnob;
        float knobKiri = kiri + jarak + jalur * posisi;
        if (posisi > 0.5f)
            knobKiri -= lebarKnob - sisiKnob;
        float knobAtas = atas + (trackTinggi - tinggiKnob) / 2f;
        RectF knob = new RectF(knobKiri, knobAtas, knobKiri + lebarKnob,
                knobAtas + tinggiKnob);

        cat.setStyle(Paint.Style.FILL);
        cat.setColor(warnaKnob);
        float radiusKnob = Theme.dp(getContext(), Theme.RADIUS_KNOB);
        canvas.drawRoundRect(knob, radiusKnob, radiusKnob, cat);

        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(Theme.dp(getContext(), 1f));
        cat.setColor(Theme.OUTLINE);
        canvas.drawRoundRect(knob, radiusKnob, radiusKnob, cat);
    }
}
