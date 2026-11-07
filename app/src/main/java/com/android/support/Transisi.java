package com.android.support;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;

import java.util.Map;
import java.util.WeakHashMap;

/**
 * Transisi singkat untuk menu: fade saat panel atau halaman berganti dan lipat
 * halus untuk section. Semua transisi dilewati bila pengguna mematikan animasi
 * sistem (skala animasi 0), dan view selalu berakhir di keadaan akhir yang benar.
 */
final class Transisi {
    private static final long DURASI_MS = 160L;
    private static final float AWAL_SKALA = 0.94f;

    private static final Map<View, ValueAnimator> lipatan = new WeakHashMap<View, ValueAnimator>();

    private Transisi() {
    }

    /** False bila animasi dimatikan di pengaturan sistem. */
    static boolean aktif(Context context) {
        try {
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f;
        } catch (Throwable t) {
            return true;
        }
    }

    /** Memunculkan view dengan fade dari transparan sampai {@code alphaAkhir}. */
    static void muncul(View view, float alphaAkhir) {
        view.animate().cancel();
        if (!aktif(view.getContext())) {
            view.setAlpha(alphaAkhir);
            return;
        }
        view.setAlpha(0f);
        view.animate().alpha(alphaAkhir).setDuration(DURASI_MS)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    /** Dialog muncul dengan fade dan sedikit membesar dari tengah. */
    static void masuk(View view) {
        view.animate().cancel();
        if (!aktif(view.getContext())) {
            view.setAlpha(1f);
            view.setScaleX(1f);
            view.setScaleY(1f);
            return;
        }
        view.setAlpha(0f);
        view.setScaleX(AWAL_SKALA);
        view.setScaleY(AWAL_SKALA);
        view.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(DURASI_MS)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    /** Membuka atau menutup isi section dengan animasi tinggi. */
    static void lipat(final View isi, final boolean buka) {
        ValueAnimator lama = lipatan.remove(isi);
        if (lama != null)
            lama.cancel();

        final ViewGroup.LayoutParams params = isi.getLayoutParams();
        int lebarInduk = isi.getParent() instanceof View ? ((View) isi.getParent()).getWidth() : 0;
        boolean bisaAnimasi = params != null && lebarInduk > 0 && isi.getWindowToken() != null
                && aktif(isi.getContext());
        if (!bisaAnimasi) {
            if (params != null)
                params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            isi.setVisibility(buka ? View.VISIBLE : View.GONE);
            isi.requestLayout();
            return;
        }

        int awal;
        int akhir;
        if (buka) {
            isi.measure(View.MeasureSpec.makeMeasureSpec(lebarInduk, View.MeasureSpec.EXACTLY),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            awal = isi.getVisibility() == View.VISIBLE ? isi.getHeight() : 0;
            akhir = isi.getMeasuredHeight();
            params.height = awal;
            isi.setVisibility(View.VISIBLE);
        } else {
            awal = isi.getHeight();
            akhir = 0;
        }

        final boolean[] batal = {false};
        ValueAnimator animator = ValueAnimator.ofInt(awal, akhir);
        animator.setDuration(DURASI_MS);
        animator.setInterpolator(new DecelerateInterpolator());
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                params.height = (Integer) animation.getAnimatedValue();
                isi.setLayoutParams(params);
            }
        });
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                batal[0] = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                if (batal[0])
                    return;
                lipatan.remove(isi);
                params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
                isi.setVisibility(buka ? View.VISIBLE : View.GONE);
                isi.setLayoutParams(params);
            }
        });
        lipatan.put(isi, animator);
        animator.start();
    }
}
