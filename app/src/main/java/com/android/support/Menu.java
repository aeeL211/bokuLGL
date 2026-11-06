//Please don't replace listeners with lambda!

package com.android.support;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.text.Html;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.text.method.DigitsKeyListener;
import android.text.method.LinkMovementMethod;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.DecelerateInterpolator;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;
import static android.view.WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
import static android.widget.RelativeLayout.ALIGN_PARENT_RIGHT;

public class Menu {
    public static final String TAG = "Mod_Menu"; //Tag for logcat

    // Tampilan panel kayu. Semua warna ada di Theme.
    int MENU_WIDTH = 340;
    int MENU_HEIGHT = 210;
    int POS_X = 0;
    int POS_Y = 100;
    int ICON_SIZE = 45; //Change both width and height of image
    float ICON_ALPHA = 0.7f; //Transparent

    RelativeLayout mCollapsed, mRootContainer;
    LinearLayout mExpanded, mods, mSettings, mCollapse;
    WindowManager mWindowManager;
    WindowManager.LayoutParams vmParams;
    ImageView startimage;
    ImageView ikonGambar;
    Drawable ikonDrawable;
    String sumberIkon = "";
    String sumberTerpasang;
    boolean gambarSiap;
    FrameLayout rootFrame;
    ScrollView scrollView;
    boolean stopChecking, overlayRequired;
    Context getContext;

    //initialize methods from the native library
    native void Init(Context context, TextView title, TextView subTitle);

    native String Icon();

    native String IconWebViewData();

    native String[] GetFeatureList();

    native String[] SettingsList();

    native boolean IsGameLibLoaded();

    // Section Room: daftar tamu berformat "nama<TAB>kunci<TAB>peran" per baris.
    native String DaftarPemain();

    // Posisi pemain lokal: "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY". Bagian tile
    // kosong bila game tidak menyediakan field tile.
    native String PosisiPemain();

    native boolean TendangPemain(String kunci, String nama);

    native boolean SalinSkinPemain(String kunci, String nama);

    // Kunci pemain yang kartunya sedang dipilih. Pilihan disimpan sebagai teks
    // kunci, bukan indeks, jadi tetap menunjuk pemain yang sama saat daftar
    // berubah urutan dan otomatis dilepas saat pemain itu keluar.
    String kunciTerpilih;

    // Pengembali nilai bawaan untuk setelan tampilan (ukuran, opasitas, gambar ikon).
    private final List<Runnable> penyetelUlang = new LinkedList<Runnable>();

    // Satu detak milik Menu untuk semua isi yang harus tetap segar (daftar
    // pemain, posisi). Detak berjalan selama menu terbuka, tidak bergantung
    // pada urusan attach/detach tiap view, dan langsung jalan sekali saat menu
    // dibuka supaya isi tidak basi.
    private static final long DETAK_MS = 1000L;
    private final Handler handlerDetak = new Handler(android.os.Looper.getMainLooper());
    private final List<Runnable> pemantau = new LinkedList<Runnable>();
    private boolean detakJalan;
    private final Runnable detak = new Runnable() {
        @Override
        public void run() {
            if (!detakJalan)
                return;
            if (mExpanded != null && mExpanded.getVisibility() == View.VISIBLE)
                jalankanPemantau();
            handlerDetak.postDelayed(this, DETAK_MS);
        }
    };

    private void jalankanPemantau() {
        // Salinan supaya pemantau boleh mendaftar atau berhenti saat berjalan.
        for (Runnable r : new LinkedList<Runnable>(pemantau)) {
            try {
                r.run();
            } catch (Throwable t) {
                Log.d(TAG, "Pemantau gagal: " + t);
            }
        }
    }

    private void daftarkanPemantau(Runnable r) {
        pemantau.add(r);
    }

    private void mulaiDetak() {
        if (detakJalan)
            return;
        detakJalan = true;
        handlerDetak.postDelayed(detak, DETAK_MS);
    }

    private void bukaMenu() {
        aturAnimasiIkon(false);
        mCollapsed.setVisibility(View.GONE);
        mExpanded.setVisibility(View.VISIBLE);
        jalankanPemantau();
    }

    /**
     * Menyamakan ukuran, opasitas, dan sumber ikon collapsed dengan setelan.
     * Ikon bawaan tetap di startimage. Sumber dari pengguna (URL, GIF) didekode
     * IkonGambar menjadi Drawable dan ditampilkan di ImageView biasa, tanpa WebView.
     */
    private void terapkanIkon() {
        int sisi = dp(ICON_SIZE);
        for (View ikon : new View[]{startimage, ikonGambar}) {
            ViewGroup.LayoutParams params = ikon.getLayoutParams();
            params.width = sisi;
            params.height = sisi;
            ikon.setLayoutParams(params);
        }
        mCollapsed.setAlpha(ICON_ALPHA);

        String sumber = sumberIkon == null ? "" : sumberIkon.trim();
        if (sumber.length() == 0)
            sumber = IconWebViewData();
        if (sumber == null || sumber.length() == 0) {
            sumberTerpasang = null;
            lepasGambar();
            tampilkanIkon();
            return;
        }

        // Sumber yang sama tidak dimuat lagi, jadi geseran slider ukuran dan
        // opasitas tidak memicu unduhan atau dekode ulang.
        if (!sumber.equals(sumberTerpasang)) {
            sumberTerpasang = sumber;
            lepasGambar();
            // Ikon bawaan tetap tampil sampai gambarnya siap. Bila gagal, tetap
            // dipakai dan dicoba lagi saat menu dibuat berikutnya.
            final String target = sumber;
            IkonCache.muat(getContext, target, new IkonCache.Pendengar() {
                @Override
                public void siap(Drawable ikon) {
                    if (!target.equals(sumberTerpasang))
                        return;
                    ikonDrawable = ikon;
                    ikonGambar.setLayerType(IkonGambar.butuhSoftware(ikon)
                            ? View.LAYER_TYPE_SOFTWARE : View.LAYER_TYPE_NONE, null);
                    ikonGambar.setImageDrawable(ikon);
                    gambarSiap = true;
                    tampilkanIkon();
                    aturAnimasiIkon(isViewCollapsed());
                }

                @Override
                public void gagal() {
                    Log.d(TAG, "Ikon gagal dimuat, memakai ikon bawaan");
                }
            });
        }
        tampilkanIkon();
    }

    private void lepasGambar() {
        aturAnimasiIkon(false);
        ikonGambar.setImageDrawable(null);
        ikonDrawable = null;
        gambarSiap = false;
    }

    private void tampilkanIkon() {
        ikonGambar.setVisibility(gambarSiap ? View.VISIBLE : View.GONE);
        startimage.setVisibility(gambarSiap ? View.GONE : View.VISIBLE);
    }

    /** GIF hanya berjalan saat ikon kecil terlihat; saat menu terbuka dihentikan supaya hemat CPU. */
    private void aturAnimasiIkon(boolean jalan) {
        if (ikonDrawable != null)
            IkonGambar.jalankan(ikonDrawable, jalan);
    }

    //Here we write the code for our Menu
    // Reference: https://www.androidhive.info/2016/11/android-floating-widget-like-facebook-chat-head/
    public Menu(Context context) {

        getContext = context;
        Preferences.context = context;
        rootFrame = new FrameLayout(context); // Global markup
        rootFrame.setOnTouchListener(onTouchListener());
        mRootContainer = new RelativeLayout(context); // Markup on which two markups of the icon and the menu itself will be placed
        mCollapsed = new RelativeLayout(context); // Markup of the icon (when the menu is minimized)
        mCollapsed.setVisibility(View.VISIBLE);
        mCollapsed.setAlpha(ICON_ALPHA);

        //********** The box of the mod menu **********
        mExpanded = new LinearLayout(context); // Menu markup (when the menu is expanded)
        mExpanded.setVisibility(View.GONE);
        mExpanded.setOrientation(LinearLayout.VERTICAL);
        mExpanded.setBackground(Theme.panel(context));
        mExpanded.setPadding(dp(3), dp(3), dp(3), dp(3));
        mExpanded.setLayoutParams(new LinearLayout.LayoutParams(dp(MENU_WIDTH), WRAP_CONTENT));

        //********** The icon to open mod menu **********
        startimage = new ImageView(context);
        startimage.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        startimage.getLayoutParams().height = dp(ICON_SIZE);
        startimage.getLayoutParams().width = dp(ICON_SIZE);
        startimage.setScaleType(ImageView.ScaleType.FIT_XY);
        byte[] decode = Base64.decode(Icon(), 0);
        startimage.setImageBitmap(BitmapFactory.decodeByteArray(decode, 0, decode.length));
        ((ViewGroup.MarginLayoutParams) startimage.getLayoutParams()).topMargin = dp(10);
        //Initialize event handlers for buttons, etc.
        startimage.setOnTouchListener(onTouchListener());
        startimage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                bukaMenu();
            }
        });

        //********** Ikon dari sumber pilihan pengguna (URL atau GIF) **********
        ikonGambar = new ImageView(context);
        ikonGambar.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        ikonGambar.getLayoutParams().height = dp(ICON_SIZE);
        ikonGambar.getLayoutParams().width = dp(ICON_SIZE);
        ((ViewGroup.MarginLayoutParams) ikonGambar.getLayoutParams()).topMargin = dp(10);
        ikonGambar.setScaleType(ImageView.ScaleType.FIT_CENTER);
        ikonGambar.setVisibility(View.GONE);
        ikonGambar.setOnTouchListener(onTouchListener());
        ikonGambar.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                bukaMenu();
            }
        });

        //********** Settings **********
        mSettings = new LinearLayout(context);
        mSettings.setOrientation(LinearLayout.VERTICAL);
        mSettings.setPadding(dp(10), dp(6), dp(10), dp(8));
        isiSetelan(SettingsList());

        //********** Header panel: judul di tengah, hanya tombol minimize di kanan **********
        RelativeLayout header = new RelativeLayout(context);
        header.setBackground(Theme.header(context));
        header.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(56)));

        RelativeLayout.LayoutParams rlTitle = new RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT);
        final TextView title = Theme.teks(context, "", Theme.TITLE_SP, Theme.TITLE_TEXT);
        title.setGravity(Gravity.CENTER);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        title.setPadding(dp(48), 0, dp(48), 0);
        title.setLayoutParams(rlTitle);

        // Tombol sudut bergaya game: kotak hitam, garis putih, glyph putih.
        Theme.Ikon minimal = new Theme.Ikon(context, Theme.Ikon.MINIMAL, Theme.BOX_SUDUT,
                Theme.ACTIVE_BORDER, 2f, Theme.ACTIVE_BORDER, 34).siku(Theme.RADIUS_PANEL);
        RelativeLayout.LayoutParams rlMinimal = new RelativeLayout.LayoutParams(dp(34), dp(34));
        rlMinimal.addRule(ALIGN_PARENT_RIGHT);
        rlMinimal.addRule(RelativeLayout.CENTER_VERTICAL);
        rlMinimal.rightMargin = dp(8);
        minimal.setLayoutParams(rlMinimal);
        minimal.setContentDescription("Kecilkan menu");
        minimal.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(ICON_ALPHA);
                mExpanded.setVisibility(View.GONE);
                aturAnimasiIkon(true);
            }
        });

        header.addView(title);
        header.addView(minimal);

        //********** Sub title **********
        final TextView subTitle = Theme.teks(context, "", Theme.DESC_SP, Theme.MUTED);
        subTitle.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        subTitle.setMarqueeRepeatLimit(-1);
        subTitle.setSingleLine(true);
        subTitle.setSelected(true);
        subTitle.setGravity(Gravity.CENTER);
        subTitle.setPadding(dp(10), dp(4), dp(10), dp(4));
        subTitle.setBackgroundColor(Theme.BODY);

        //********** Mod menu feature list **********
        scrollView = new ScrollView(context);
        scrollView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(MENU_HEIGHT)));
        scrollView.setBackgroundColor(Theme.BODY);
        Theme.gayaScrollbar(context, scrollView);
        mods = new LinearLayout(context);
        mods.setOrientation(LinearLayout.VERTICAL);
        mods.setPadding(dp(10), dp(6), dp(10), dp(8));

        //********** Footer dengan tombol kayu **********
        LinearLayout footer = new LinearLayout(context);
        footer.setOrientation(LinearLayout.HORIZONTAL);
        footer.setBackground(Theme.footer(context));
        footer.setPadding(dp(8), dp(6), dp(8), dp(6));

        // Setelan di kiri, Sembunyikan di kanan, dengan gaya tombol yang sama.
        // Tombol Setelan berganti jadi Kembali selama halaman setelan terbuka.
        final Button setelanBtn = teksTombol("Setelan");
        setelanBtn.setBackground(Theme.tombolTekan(context));
        LinearLayout.LayoutParams paramsSetelan = new LinearLayout.LayoutParams(0, dp(44), 1f);
        paramsSetelan.setMargins(0, 0, dp(6), 0);
        setelanBtn.setLayoutParams(paramsSetelan);
        setelanBtn.setOnClickListener(new View.OnClickListener() {
            CharSequence judulAsal = "";
            CharSequence subJudulAsal = "";
            int scrollAsal;

            @Override
            public void onClick(View v) {
                try {
                    if (mSettings.getParent() == scrollView) {
                        scrollView.removeView(mSettings);
                        scrollView.addView(mods);
                        title.setText(judulAsal);
                        subTitle.setText(subJudulAsal);
                        setelanBtn.setText("Setelan");
                        // Posisi gulir daftar fitur dikembalikan setelah layout selesai.
                        scrollView.post(new Runnable() {
                            @Override
                            public void run() {
                                scrollView.scrollTo(0, scrollAsal);
                            }
                        });
                    } else {
                        scrollAsal = scrollView.getScrollY();
                        judulAsal = title.getText();
                        subJudulAsal = subTitle.getText();
                        scrollView.removeView(mods);
                        scrollView.addView(mSettings);
                        scrollView.scrollTo(0, 0);
                        // Judul utama tetap "BOKU Menu"; hanya subjudul yang
                        // menandai halaman setelan.
                        subTitle.setText("Menu \\ Setelan");
                        setelanBtn.setText("Kembali");
                    }
                } catch (IllegalStateException ignored) {
                }
            }
        });
        footer.addView(setelanBtn);

        final Button hideBtn = teksTombol("Sembunyikan");
        hideBtn.setBackground(Theme.tombolTekan(context));
        hideBtn.setLayoutParams(new LinearLayout.LayoutParams(0, dp(44), 1f));
        hideBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(0);
                mExpanded.setVisibility(View.GONE);
                aturAnimasiIkon(false);
                Toast.makeText(view.getContext(), "Ikon disembunyikan. Ingat posisi ikon tersembunyi", Toast.LENGTH_LONG).show();
            }
        });
        footer.addView(hideBtn);

        //********** Adding view components **********
        mRootContainer.addView(mCollapsed);
        mRootContainer.addView(mExpanded);
        mCollapsed.addView(startimage);
        mCollapsed.addView(ikonGambar);
        mExpanded.addView(header);
        Theme.Zigzag zigAtas = new Theme.Zigzag(context, Theme.HEADER, true);
        zigAtas.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigAtas, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(subTitle, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        scrollView.addView(mods);
        mExpanded.addView(scrollView);
        Theme.Zigzag zigBawah = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBawah.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigBawah, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(footer);

        terapkanIkon();
        Init(context, title, subTitle);
        Theme.applyFont(mExpanded);
        Theme.applyFont(mSettings);
        mulaiDetak();
    }

    public void ShowMenu() {
        rootFrame.addView(mRootContainer);

        final Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
            boolean viewLoaded = false;

            @Override
            public void run() {
                //If the save preferences is enabled, it will check if game lib is loaded before starting menu
                //Comment the if-else code out except startService if you want to run the app and test preferences
                if (Preferences.loadPref && !IsGameLibLoaded() && !stopChecking) {
                    if (!viewLoaded) {
                        Category(mods, "Simpan preferensi aktif. Menunggu library game dimuat...\n\n"
                                + "Paksa muat menu mungkin tidak langsung menerapkan mod. Kamu perlu mengaktifkannya lagi");
                        Button(mods, -100, "Paksa muat menu");
                        Theme.applyFont(mods);
                        viewLoaded = true;
                    }
                    handler.postDelayed(this, 600);
                } else {
                    mods.removeAllViews();
                    featureList(GetFeatureList(), mods);
                    Theme.applyFont(mods);
                }
            }
        }, 500);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerWindowService() {
        //Variable to check later if the phone supports Draw over other apps permission
        int iparams = Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O ? 2038 : 2002;
        vmParams = new WindowManager.LayoutParams(
                WRAP_CONTENT,
                WRAP_CONTENT,
                iparams,
                8 | FLAG_TRANSLUCENT_STATUS,
                -3);
        //params = new WindowManager.LayoutParams(WindowManager.LayoutParams.LAST_APPLICATION_WINDOW, 8, -3);
        vmParams.gravity = 51;
        vmParams.x = POS_X;
        vmParams.y = POS_Y;

        mWindowManager = (WindowManager) getContext.getSystemService(Context.WINDOW_SERVICE);
        mWindowManager.addView(rootFrame, vmParams);

        overlayRequired = true;
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerActivity() {
        vmParams = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                POS_X,//initialX
                POS_Y,//initialy
                WindowManager.LayoutParams.TYPE_APPLICATION,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_OVERSCAN |
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN |
                        WindowManager.LayoutParams.FLAG_SPLIT_TOUCH,
                PixelFormat.TRANSPARENT
        );
        vmParams.gravity = 51;
        vmParams.x = POS_X;
        vmParams.y = POS_Y;

        mWindowManager = ((Activity) getContext).getWindowManager();
        mWindowManager.addView(rootFrame, vmParams);
    }

    private View.OnTouchListener onTouchListener() {
        return new View.OnTouchListener() {
            final View collapsedView = mCollapsed;
            final View expandedView = mExpanded;
            private float initialTouchX, initialTouchY;
            private int initialX, initialY;

            public boolean onTouch(View view, MotionEvent motionEvent) {
                switch (motionEvent.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = vmParams.x;
                        initialY = vmParams.y;
                        initialTouchX = motionEvent.getRawX();
                        initialTouchY = motionEvent.getRawY();
                        return true;
                    case MotionEvent.ACTION_UP:
                        int rawX = (int) (motionEvent.getRawX() - initialTouchX);
                        int rawY = (int) (motionEvent.getRawY() - initialTouchY);
                        mExpanded.setAlpha(1f);
                        mCollapsed.setAlpha(ICON_ALPHA);
                        //The check for Xdiff <10 && YDiff< 10 because sometime elements moves a little while clicking.
                        //So that is click event.
                        if (rawX < 10 && rawY < 10 && isViewCollapsed()) {
                            //When user clicks on the image view of the collapsed layout,
                            //visibility of the collapsed layout will be changed to "View.GONE"
                            //and expanded view will become visible.
                            try {
                                bukaMenu();
                            } catch (NullPointerException ignored) {
                            }
                        }
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        mExpanded.setAlpha(0.5f);
                        mCollapsed.setAlpha(0.5f);
                        //Calculate the X and Y coordinates of the view.
                        vmParams.x = initialX + ((int) (motionEvent.getRawX() - initialTouchX));
                        vmParams.y = initialY + ((int) (motionEvent.getRawY() - initialTouchY));
                        //Update the layout with new X & Y coordinate
                        mWindowManager.updateViewLayout(rootFrame, vmParams);
                        return true;
                    default:
                        return false;
                }
            }
        };
    }

    private static final String GRUP_UMUM = "Umum";
    private static final String GRUP_IKON = "Ikon melayang";

    /**
     * Halaman setelan: fitur dikelompokkan dengan judul kecil (Umum, Ikon
     * melayang) dan diakhiri tombol setel ulang tampilan. Pengelompokan hanya
     * dipakai bila semua entri punya nomor fitur dan native tidak membuat
     * kategori sendiri; selain itu daftar dari native dipakai apa adanya.
     */
    private void isiSetelan(String[] daftar) {
        if (daftar == null)
            return;
        featureList(kelompokkanSetelan(daftar), mSettings);

        if (!penyetelUlang.isEmpty()) {
            final Button ulang = teksTombol("Setel ulang tampilan");
            ulang.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    for (Runnable r : new LinkedList<Runnable>(penyetelUlang))
                        r.run();
                    Toast.makeText(getContext, "Tampilan disetel ulang", Toast.LENGTH_SHORT).show();
                }
            });
            LinearLayout kotak = kotakTombol(IkonFitur.ULANG, ulang);
            ((LinearLayout.LayoutParams) kotak.getLayoutParams()).topMargin = dp(12);
            mSettings.addView(kotak);
        }
    }

    private String[] kelompokkanSetelan(String[] daftar) {
        List<String> umum = new LinkedList<String>();
        List<String> ikon = new LinkedList<String>();
        for (String entri : daftar) {
            Integer nomor = nomorEntri(entri);
            if (nomor == null)
                return daftar;
            if (nomor == -4 || nomor == -5 || nomor == -6)
                ikon.add(entri);
            else
                umum.add(entri);
        }
        if (umum.isEmpty() || ikon.isEmpty())
            return daftar;

        List<String> hasil = new LinkedList<String>();
        hasil.add("Category_" + GRUP_UMUM);
        hasil.addAll(umum);
        hasil.add("Category_" + GRUP_IKON);
        hasil.addAll(ikon);
        return hasil.toArray(new String[0]);
    }

    /** Nomor fitur di awal entri, atau null bila entri tidak bernomor (kategori, teks, tautan). */
    private Integer nomorEntri(String entri) {
        String f = entri.replaceFirst("_True", "").replaceFirst("CollapseAdd_", "");
        String awal = f.split("_")[0];
        if (awal.length() > 0 && (TextUtils.isDigitsOnly(awal) || awal.matches("-[0-9]+"))) {
            try {
                return Integer.parseInt(awal);
            } catch (NumberFormatException e) {
                return null;
            }
        }
        return null;
    }

    private void featureList(String[] listFT, LinearLayout linearLayout) {
        //Currently looks messy right now. Let me know if you have improvements
        int featNum, subFeat = 0;
        LinearLayout llBak = linearLayout;

        for (int i = 0; i < listFT.length; i++) {
            boolean switchedOn = false;
            //Log.i("featureList", listFT[i]);
            String feature = listFT[i];
            if (feature.contains("_True")) {
                switchedOn = true;
                feature = feature.replaceFirst("_True", "");
            }

            linearLayout = llBak;
            if (feature.contains("CollapseAdd_")) {
                //if (collapse != null)
                linearLayout = mCollapse;
                feature = feature.replaceFirst("CollapseAdd_", "");
            }
            String[] str = feature.split("_");

            //Assign feature number
            if (TextUtils.isDigitsOnly(str[0]) || str[0].matches("-[0-9]*")) {
                featNum = Integer.parseInt(str[0]);
                feature = feature.replaceFirst(str[0] + "_", "");
                subFeat++;
            } else {
                //Subtract feature number. We don't want to count ButtonLink, Category, RichTextView and RichWebView
                featNum = i - subFeat;
            }
            String[] strSplit = feature.split("_");
            switch (strSplit[0]) {
                case "Toggle":
                    Switch(linearLayout, featNum, strSplit[1], switchedOn);
                    break;
                case "SeekBar":
                    SeekBar(linearLayout, featNum, strSplit[1], Integer.parseInt(strSplit[2]),
                            Integer.parseInt(strSplit[3]),
                            strSplit.length > 4 ? Integer.parseInt(strSplit[4]) : Integer.parseInt(strSplit[2]),
                            strSplit.length > 5 ? Integer.parseInt(strSplit[5]) : 0);
                    break;
                case "Button":
                    Button(linearLayout, featNum, strSplit[1]);
                    break;
                case "ButtonOnOff":
                    ButtonOnOff(linearLayout, featNum, strSplit[1], switchedOn);
                    break;
                case "Spinner":
                    TextView(linearLayout, strSplit[1]);
                    Spinner(linearLayout, featNum, strSplit[1], strSplit[2]);
                    break;
                case "InputText":
                    InputText(linearLayout, featNum, strSplit[1]);
                    break;
                case "InputValue":
                    if (strSplit.length == 3)
                        InputNum(linearLayout, featNum, strSplit[2], Integer.parseInt(strSplit[1]));
                    if (strSplit.length == 2)
                        InputNum(linearLayout, featNum, strSplit[1], 0);
                    break;
                case "InputLValue":
                    if (strSplit.length == 3)
                        InputLNum(linearLayout, featNum, strSplit[2], Long.parseLong(strSplit[1]));
                    if (strSplit.length == 2)
                        InputLNum(linearLayout, featNum, strSplit[1], 0);
                    break;
                case "CheckBox":
                    CheckBox(linearLayout, featNum, strSplit[1], switchedOn);
                    break;
                case "RadioButton":
                    RadioButton(linearLayout, featNum, strSplit[1], strSplit[2]);
                    break;
                case "Collapse":
                    Collapse(linearLayout, strSplit[1], switchedOn);
                    subFeat++;
                    break;
                case "ButtonLink":
                    subFeat++;
                    ButtonLink(linearLayout, strSplit[1], strSplit[2]);
                    break;
                case "Category":
                    subFeat++;
                    Category(linearLayout, strSplit[1]);
                    break;
                case "PlayerList":
                    subFeat++;
                    PlayerList(linearLayout, strSplit[1]);
                    break;
                case "LivePos":
                    subFeat++;
                    BarisPosisi(linearLayout, strSplit[1]);
                    break;
                case "RichTextView":
                    subFeat++;
                    TextView(linearLayout, strSplit[1]);
                    break;
                case "RichWebView":
                    subFeat++;
                    WebTextView(linearLayout, strSplit[1]);
                    break;
            }
        }
    }

    private void Switch(LinearLayout linLayout, final int featNum, final String featName, boolean swiOn) {
        final AnimatedSwitch switchR = new AnimatedSwitch(getContext);
        switchR.setContentDescription(featName);
        switchR.setChecked(Preferences.loadPrefBool(featName, featNum, swiOn));
        switchR.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton compoundButton, boolean bool) {
                Preferences.changeFeatureBool(featName, featNum, bool);
                if (featNum == -1) {
                    Preferences.with(switchR.getContext()).writeBoolean(-1, bool);
                    if (!bool) Preferences.with(switchR.getContext()).clear(); //Clear perferences if switched off
                }
            }
        });

        TextView label = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final LinearLayout baris = baris(IkonFitur.pola(featNum, featName), label, switchR);
        baris.setBackground(Theme.barisTekan(getContext));
        baris.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switchR.toggle();
            }
        });
        pasangPenjelasan(baris, featNum, featName);
        pasangPenjelasan(switchR, featNum, featName);
        linLayout.addView(baris);
    }

    private void SeekBar(LinearLayout linLayout, final int featNum, final String featName,
                         final int min, final int max, final int def, final int desimal) {
        // Nilai 0 adalah nilai sah untuk gravitasi, jadi default hanya dipakai
        // bila fitur ini belum pernah disimpan.
        boolean tersimpan = Preferences.sudahDisimpan(featName, featNum);
        int nilaiAwal = tersimpan ? Preferences.loadPrefInt(featName, featNum) : def;
        if (nilaiAwal < min)
            nilaiAwal = min;
        if (nilaiAwal > max)
            nilaiAwal = max;
        if (!tersimpan)
            Preferences.changeFeatureInt(featName, featNum, nilaiAwal);

        // Opasitas dan ukuran ikon langsung dipakai saat slider dibaca.
        if (featNum == -4)
            ICON_ALPHA = nilaiAwal / 100f;
        else if (featNum == -5)
            ICON_SIZE = nilaiAwal;

        final String satuan = satuanFitur(featNum);

        LinearLayout kolom = new LinearLayout(getContext);
        kolom.setOrientation(LinearLayout.VERTICAL);
        kolom.setGravity(Gravity.CENTER_VERTICAL);

        // Baris judul: nama fitur di kiri, angka di kanan. Ketuk angka untuk
        // mengembalikan slider ke nilai bawaan.
        LinearLayout judulBaris = new LinearLayout(getContext);
        judulBaris.setOrientation(LinearLayout.HORIZONTAL);
        judulBaris.setGravity(Gravity.CENTER_VERTICAL);

        final TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        judul.setSingleLine(true);
        judul.setEllipsize(TextUtils.TruncateAt.END);
        judulBaris.addView(judul, new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));

        final TextView angka = Theme.teks(getContext, nilaiTeks(nilaiAwal, desimal) + satuan,
                Theme.DESC_SP, Theme.TITLE_TEXT);
        angka.setGravity(Gravity.CENTER);
        angka.setSingleLine(true);
        angka.setMinWidth(dp(44));
        angka.setPadding(dp(8), dp(2), dp(8), dp(2));
        angka.setBackground(Theme.tombolKecil(getContext));
        angka.setContentDescription(featName + ", ketuk untuk reset");
        judulBaris.addView(angka, new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));

        final SeekBar seekBar = new SeekBar(getContext);
        seekBar.setProgressDrawable(Theme.seekbarProgress(getContext));
        seekBar.setThumb(Theme.seekbarThumb(getContext));
        seekBar.setThumbOffset(0);
        // Padding mendatar setengah lebar thumb supaya thumb tidak terpotong di ujung.
        seekBar.setPadding(dp(5), 0, dp(5), 0);
        // Rentang digeser ke nol supaya setMin yang hanya ada di Oreo ke atas
        // tidak dibutuhkan.
        seekBar.setMax(max - min);
        seekBar.setProgress(nilaiAwal - min);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            public void onProgressChanged(SeekBar seekBar, int i, boolean z) {
                int nilai = i + min;
                if (nilai < min)
                    nilai = min;
                if (nilai > max)
                    nilai = max;
                Preferences.changeFeatureInt(featName, featNum, nilai);
                angka.setText(nilaiTeks(nilai, desimal) + satuan);
                if (featNum == -4 || featNum == -5) {
                    if (featNum == -4)
                        ICON_ALPHA = nilai / 100f;
                    else
                        ICON_SIZE = nilai;
                    terapkanIkon();
                }
            }
        });

        angka.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Lewat jalur geseran yang sama, jadi native ikut menerima nilai bawaan.
                seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
            }
        });

        if (featNum == -4 || featNum == -5) {
            penyetelUlang.add(new Runnable() {
                @Override
                public void run() {
                    seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
                }
            });
        }

        kolom.addView(judulBaris, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        kolom.addView(seekBar, new LinearLayout.LayoutParams(MATCH_PARENT, dp(28)));
        final LinearLayout baris = baris(IkonFitur.pola(featNum, featName), kolom, null);
        pasangPenjelasan(baris, featNum, featName);
        linLayout.addView(baris);
    }

    /** Satuan angka slider: pengali kecepatan, persen opasitas, atau dp ikon. */
    private String satuanFitur(int featNum) {
        if (featNum == 23 || featNum == 24)
            return "x";
        if (featNum == -4)
            return "%";
        if (featNum == -5)
            return "dp";
        return "";
    }

    /** Angka slider ditampilkan dengan jumlah desimal yang ditulis di feature list. */
    private String nilaiTeks(int nilai, int desimal) {
        if (desimal <= 0)
            return String.valueOf(nilai);
        return String.format(Locale.US, "%." + desimal + "f", nilai / 10f);
    }

    private void Button(LinearLayout linLayout, final int featNum, final String featName) {
        final Button button = teksTombol(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switch (featNum) {
                    case -100:
                        stopChecking = true;
                        break;
                }
                Preferences.changeFeatureInt(featName, featNum, 0);
            }
        });

        LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum, featName), button);
        pasangPenjelasan(kotak, featNum, featName);
        pasangPenjelasan(button, featNum, featName);
        linLayout.addView(kotak);
    }

    private void ButtonLink(LinearLayout linLayout, final String featName, final String url) {
        final Button button = teksTombol(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.setData(Uri.parse(url));
                getContext.startActivity(intent);
            }
        });
        final LinearLayout kotak = kotakTombol(IkonFitur.TELEGRAM, button);
        pasangPenjelasan(kotak, -100, featName);
        pasangPenjelasan(button, -100, featName);
        linLayout.addView(kotak);
    }

    private void ButtonOnOff(LinearLayout linLayout, final int featNum, String featName, boolean switchedOn) {
        final Button button = teksTombol("");
        final String finalfeatName = featName.replace("OnOff_", "");
        boolean isOn = Preferences.loadPrefBool(featName, featNum, switchedOn);
        final LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum, featName), button);
        if (isOn) {
            button.setText(Html.fromHtml(finalfeatName + ": ON"));
            kotak.setBackground(Theme.tombolAktif(getContext));
            isOn = false;
        } else {
            button.setText(Html.fromHtml(finalfeatName + ": OFF"));
            kotak.setBackground(Theme.tombolMati(getContext));
            isOn = true;
        }
        final boolean finalIsOn = isOn;
        button.setOnClickListener(new View.OnClickListener() {
            boolean isOn = finalIsOn;

            public void onClick(View v) {
                Preferences.changeFeatureBool(finalfeatName, featNum, isOn);
                //Log.d(TAG, finalfeatName + " " + featNum + " " + isActive2);
                if (isOn) {
                    button.setText(Html.fromHtml(finalfeatName + ": ON"));
                    kotak.setBackground(Theme.tombolAktif(getContext));
                    isOn = false;
                } else {
                    button.setText(Html.fromHtml(finalfeatName + ": OFF"));
                    kotak.setBackground(Theme.tombolMati(getContext));
                    isOn = true;
                }
            }
        });
        pasangPenjelasan(kotak, featNum, finalfeatName);
        pasangPenjelasan(button, featNum, finalfeatName);
        linLayout.addView(kotak);
    }

    private void Spinner(LinearLayout linLayout, final int featNum, final String featName, final String list) {
        Log.d(TAG, "spinner " + featNum + " " + featName + " " + list);
        final List<String> lists = new LinkedList<>(Arrays.asList(list.split(",")));

        final Spinner spinner = new Spinner(getContext, Spinner.MODE_DROPDOWN);
        ArrayAdapter<String> aa = new ArrayAdapter<String>(getContext,
                android.R.layout.simple_spinner_item, lists) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                TextView view = (TextView) super.getView(position, convertView, parent);
                Theme.applyFont(view);
                view.setTextColor(Theme.TEXT);
                view.setTextSize(Theme.ROW_SP);
                return view;
            }

            @Override
            public View getDropDownView(int position, View convertView, ViewGroup parent) {
                TextView view = (TextView) super.getDropDownView(position, convertView, parent);
                Theme.applyFont(view);
                view.setBackgroundColor(Theme.BODY);
                view.setTextColor(Theme.TEXT);
                view.setTextSize(Theme.ROW_SP);
                view.setPadding(dp(10), dp(8), dp(10), dp(8));
                return view;
            }
        };
        aa.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(aa);
        spinner.setSelection(Preferences.loadPrefInt(featName, featNum));
        if (spinner.getBackground() != null)
            spinner.getBackground().setColorFilter(Theme.TEXT, PorterDuff.Mode.SRC_ATOP);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
                Preferences.changeFeatureInt(spinner.getSelectedItem().toString(), featNum, position);
                ((TextView) parentView.getChildAt(0)).setTextColor(Theme.TEXT);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        final LinearLayout baris = baris(IkonFitur.pola(featNum, featName), spinner, null);
        pasangPenjelasan(baris, featNum, featName);
        pasangPenjelasan(spinner, featNum, featName);
        linLayout.addView(baris);
    }

    private void InputNum(LinearLayout linLayout, final int featNum, final String featName, final int maxValue) {
        final Button button = teksTombol("");
        int num = Preferences.loadPrefInt(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);
                final EditText editText = new EditText(getContext);
                if (maxValue != 0)
                    editText.setHint("Nilai maksimum: " + maxValue);
                editText.setInputType(InputType.TYPE_CLASS_NUMBER);
                editText.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
                InputFilter[] FilterArray = new InputFilter[1];
                FilterArray[0] = new InputFilter.LengthFilter(10);
                editText.setFilters(FilterArray);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan angka");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        int num;
                        try {
                            String inp = editText.getText().toString();
                            num = Integer.parseInt(inp.isEmpty() ? "0" : inp);
                            if (maxValue != 0 && num >= maxValue)
                                num = maxValue;
                        } catch (NumberFormatException ex) {
                            if (maxValue != 0)
                                num = maxValue;
                            else
                                num = Integer.MAX_VALUE;
                        }

                        button.setText(Html.fromHtml(featName + ": " + num));
                        Preferences.changeFeatureInt(featName, featNum, num);
                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        // dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        final LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum, featName), button);
        pasangPenjelasan(kotak, featNum, featName);
        pasangPenjelasan(button, featNum, featName);
        linLayout.addView(kotak);
    }

    private void InputLNum(LinearLayout linLayout, final int featNum, final String featName, final long maxValue) {
        final Button button = teksTombol("");
        long num = Preferences.loadPrefLong(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);
                final EditText editText = new EditText(getContext);
                if (maxValue != 0)
                    editText.setHint("Nilai maksimum: " + maxValue);
                editText.setInputType(InputType.TYPE_CLASS_NUMBER);
                editText.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
                InputFilter[] FilterArray = new InputFilter[1];
                FilterArray[0] = new InputFilter.LengthFilter(20);
                editText.setFilters(FilterArray);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan angka");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        long num;
                        try {
                            String inp = editText.getText().toString();
                            num = Long.parseLong(inp.isEmpty() ? "0" : inp);
                            if (maxValue != 0 && num >= maxValue)
                                num = maxValue;
                        } catch (NumberFormatException ex) {
                            if (maxValue != 0)
                                num = maxValue;
                            else
                                num = Long.MAX_VALUE;
                        }

                        button.setText(Html.fromHtml(featName + ": " + num));
                        Preferences.changeFeatureLong(featName, featNum, num);

                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        // dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        final LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum, featName), button);
        pasangPenjelasan(kotak, featNum, featName);
        pasangPenjelasan(button, featNum, featName);
        linLayout.addView(kotak);
    }

    private void InputText(LinearLayout linLayout, final int featNum, final String featName) {
        String string = Preferences.loadPrefString(featName, featNum);
        if (featNum == -6) {
            InputGambar(linLayout, featNum, featName, string);
            return;
        }

        final Button button = teksTombol("");
        button.setText(Html.fromHtml(featName + ": " + string));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);

                final EditText editText = new EditText(getContext);
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Masukkan teks");
                alertName.setView(editText);
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText); // displays the user input bar
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        String str = editText.getText().toString();
                        button.setText(Html.fromHtml(featName + ": " + str));
                        Preferences.changeFeatureString(featName, featNum, str);
                        editText.setFocusable(false);
                    }
                });

                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        //dialog.cancel(); // closes dialog
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        final LinearLayout kotak = kotakTombol(IkonFitur.pola(featNum, featName), button);
        pasangPenjelasan(kotak, featNum, featName);
        pasangPenjelasan(button, featNum, featName);
        linLayout.addView(kotak);
    }

    /**
     * Setelan gambar ikon: baris dua tingkat (nama di atas, sumber saat ini
     * di bawah dengan warna redup). Ketuk baris untuk mengubah, atau pilih
     * Bawaan di dialog untuk kembali ke ikon bawaan.
     */
    private void InputGambar(LinearLayout linLayout, final int featNum, final String featName,
                             String awal) {
        sumberIkon = awal == null ? "" : awal;

        LinearLayout kolom = new LinearLayout(getContext);
        kolom.setOrientation(LinearLayout.VERTICAL);

        TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        judul.setSingleLine(true);
        judul.setEllipsize(TextUtils.TruncateAt.END);
        kolom.addView(judul);

        final TextView nilai = Theme.teks(getContext, ringkasSumber(sumberIkon), Theme.DESC_SP,
                Theme.MUTED);
        nilai.setSingleLine(true);
        nilai.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        kolom.addView(nilai);

        final LinearLayout baris = baris(IkonFitur.pola(featNum, featName), kolom, null);
        baris.setBackground(Theme.barisTekan(getContext));
        pasangPenjelasan(baris, featNum, featName);
        baris.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                AlertDialog.Builder alertName = new AlertDialog.Builder(getContext);

                final EditText editText = new EditText(getContext);
                editText.setHint("URL gambar atau GIF, kosongkan untuk bawaan");
                editText.setText(sumberIkon);
                editText.setSelection(editText.length());
                editText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        if (hasFocus) {
                            imm.toggleSoftInput(InputMethodManager.SHOW_FORCED, InputMethodManager.HIDE_IMPLICIT_ONLY);
                        } else {
                            imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                        }
                    }
                });
                editText.requestFocus();

                alertName.setTitle("Sumber gambar ikon");
                LinearLayout layoutName = new LinearLayout(getContext);
                layoutName.setOrientation(LinearLayout.VERTICAL);
                layoutName.addView(editText);
                alertName.setView(layoutName);

                alertName.setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        setSumberIkon(editText.getText().toString(), featNum, featName, nilai);
                        editText.setFocusable(false);
                    }
                });
                alertName.setNeutralButton("Bawaan", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        setSumberIkon("", featNum, featName, nilai);
                    }
                });
                alertName.setNegativeButton("Batal", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int whichButton) {
                        InputMethodManager imm = (InputMethodManager) getContext.getSystemService(Context.INPUT_METHOD_SERVICE);
                        imm.toggleSoftInput(InputMethodManager.SHOW_IMPLICIT, 0);
                    }
                });

                tampilkanDialog(alertName);
            }
        });

        penyetelUlang.add(new Runnable() {
            @Override
            public void run() {
                if (sumberIkon.length() > 0)
                    setSumberIkon("", featNum, featName, nilai);
            }
        });

        linLayout.addView(baris);
    }

    private void setSumberIkon(String sumber, int featNum, String featName, TextView nilai) {
        sumber = sumber == null ? "" : sumber.trim();
        sumberIkon = sumber;
        nilai.setText(ringkasSumber(sumber));
        terapkanIkon();
        Preferences.changeFeatureString(featName, featNum, sumber);
    }

    private String ringkasSumber(String sumber) {
        return sumber.length() == 0 ? "Bawaan" : sumber;
    }

    private void CheckBox(LinearLayout linLayout, final int featNum, final String featName, boolean switchedOn) {
        final CheckBox checkBox = new CheckBox(getContext);
        checkBox.setText(featName);
        checkBox.setTextColor(Theme.TEXT);
        checkBox.setTextSize(Theme.ROW_SP);
        checkBox.setTypeface(Theme.font(getContext));
        checkBox.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
        checkBox.setChecked(Preferences.loadPrefBool(featName, featNum, switchedOn));
        checkBox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (checkBox.isChecked()) {
                    Preferences.changeFeatureBool(featName, featNum, isChecked);
                } else {
                    Preferences.changeFeatureBool(featName, featNum, isChecked);
                }
            }
        });
        final LinearLayout baris = baris(IkonFitur.pola(featNum, featName), checkBox, null);
        pasangPenjelasan(baris, featNum, featName);
        pasangPenjelasan(checkBox, featNum, featName);
        linLayout.addView(baris);
    }

    @SuppressLint("SetTextI18n")
    private void RadioButton(LinearLayout linLayout, final int featNum, String featName, final String list) {
        //Credit: LoraZalora
        final List<String> lists = new LinkedList<>(Arrays.asList(list.split(",")));

        final TextView textView = Theme.teks(getContext, featName + ":", Theme.ROW_SP, Theme.TEXT);

        final RadioGroup radioGroup = new RadioGroup(getContext);
        radioGroup.setPadding(dp(10), dp(6), dp(10), dp(6));
        radioGroup.setOrientation(LinearLayout.VERTICAL);
        radioGroup.setBackground(Theme.baris(getContext));
        LinearLayout.LayoutParams groupParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        groupParams.setMargins(0, dp(3), 0, dp(3));
        radioGroup.setLayoutParams(groupParams);
        radioGroup.addView(textView);

        for (int i = 0; i < lists.size(); i++) {
            final RadioButton Radioo = new RadioButton(getContext);
            final String finalfeatName = featName, radioName = lists.get(i);
            View.OnClickListener first_radio_listener = new View.OnClickListener() {
                public void onClick(View v) {
                    textView.setText(Html.fromHtml(finalfeatName + ": <font color='"
                            + Theme.warnaHex(Theme.ICON_BOX) + "'>" + radioName));
                    Preferences.changeFeatureInt(finalfeatName, featNum, radioGroup.indexOfChild(Radioo));
                }
            };
            System.out.println(lists.get(i));
            Radioo.setText(lists.get(i));
            Radioo.setTextColor(Theme.TEXT);
            Radioo.setTextSize(Theme.ROW_SP);
            Radioo.setTypeface(Theme.font(getContext));
            Radioo.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
            Radioo.setOnClickListener(first_radio_listener);
            pasangPenjelasan(Radioo, featNum, featName);
            radioGroup.addView(Radioo);
        }

        int index = Preferences.loadPrefInt(featName, featNum);
        if (index > 0) { //Preventing it to get an index less than 1. below 1 = null = crash
            textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.warnaHex(Theme.ICON_BOX)
                    + "'>" + lists.get(index - 1)));
            ((RadioButton) radioGroup.getChildAt(index)).setChecked(true);
        }
        pasangPenjelasan(radioGroup, featNum, featName);
        linLayout.addView(radioGroup);
    }

    @SuppressLint("SetTextI18n")
    private void Collapse(LinearLayout linLayout, final String text, final boolean expanded) {
        LinearLayout section = new LinearLayout(getContext);
        section.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams sectionParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        sectionParams.setMargins(0, dp(10), 0, 0);
        section.setLayoutParams(sectionParams);

        LinearLayout kepala = new LinearLayout(getContext);
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);
        kepala.setBackground(Theme.sectionHeader(getContext));
        kepala.setPadding(dp(14), 0, dp(10), 0);
        kepala.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(44)));

        final TextView judul = Theme.teks(getContext, text, Theme.SECTION_SP, Theme.TITLE_TEXT);
        judul.setSingleLine(true);
        judul.setEllipsize(TextUtils.TruncateAt.END);
        LinearLayout.LayoutParams judulParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        kepala.addView(judul, judulParams);

        final Theme.Ikon panah = Theme.Ikon.polos(getContext,
                expanded ? Theme.Ikon.PANAH_BAWAH : Theme.Ikon.PANAH_KANAN,
                Theme.TITLE_TEXT, 20);
        kepala.addView(panah, new LinearLayout.LayoutParams(dp(20), dp(20)));

        final LinearLayout isi = new LinearLayout(getContext);
        isi.setOrientation(LinearLayout.VERTICAL);
        isi.setPadding(0, dp(4), 0, dp(2));
        isi.setBackground(Theme.sectionBody(getContext));
        mCollapse = isi;

        final LinearLayout pembungkus = new LinearLayout(getContext);
        pembungkus.setOrientation(LinearLayout.VERTICAL);
        pembungkus.addView(isi);

        if (expanded) {
            pembungkus.setVisibility(View.VISIBLE);
            panah.setJenis(Theme.Ikon.PANAH_BAWAH);
        } else {
            pembungkus.setVisibility(View.GONE);
            panah.setJenis(Theme.Ikon.PANAH_KANAN);
        }

        kepala.setOnClickListener(new View.OnClickListener() {
            boolean terbuka = expanded;

            @Override
            public void onClick(View v) {
                terbuka = !terbuka;
                pembungkus.setVisibility(terbuka ? View.VISIBLE : View.GONE);
                panah.setJenis(terbuka ? Theme.Ikon.PANAH_BAWAH : Theme.Ikon.PANAH_KANAN);
                // Isi yang baru terlihat langsung disegarkan, tanpa menunggu detak.
                if (terbuka)
                    jalankanPemantau();
            }
        });

        section.addView(kepala);
        section.addView(pembungkus);
        linLayout.addView(section);
    }

    private static final class KartuPemain {
        final String nama;
        final String peran;
        final LinearLayout view;

        KartuPemain(String nama, String peran, LinearLayout view) {
            this.nama = nama;
            this.peran = peran;
            this.view = view;
        }
    }

    /** Keadaan satu daftar pemain: kartu yang sudah ada dan isi terakhirnya. */
    private static final class IsiDaftar {
        final Map<String, KartuPemain> kartu = new LinkedHashMap<String, KartuPemain>();
        String data;
        TextView kosong;
    }

    @SuppressLint("SetTextI18n")
    private void PlayerList(LinearLayout linLayout, final String featName) {
        LinearLayout container = new LinearLayout(getContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(2), dp(2), dp(2), dp(4));

        LinearLayout judulBaris = new LinearLayout(getContext);
        judulBaris.setOrientation(LinearLayout.HORIZONTAL);
        judulBaris.setGravity(Gravity.CENTER_VERTICAL);
        judulBaris.setBackground(Theme.baris(getContext));
        judulBaris.setPadding(0, 0, dp(5), 0);
        LinearLayout.LayoutParams paramsJudul = new LinearLayout.LayoutParams(MATCH_PARENT, dp(48));
        paramsJudul.setMargins(0, dp(4), 0, dp(4));
        judulBaris.setLayoutParams(paramsJudul);

        judulBaris.addView(new IkonFitur(getContext, IkonFitur.PEMAIN, true),
                new LinearLayout.LayoutParams(dp(IkonFitur.LEBAR_DP), MATCH_PARENT));

        TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        LinearLayout.LayoutParams paramsJudulTeks = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        paramsJudulTeks.setMargins(dp(12), 0, dp(6), 0);
        judulBaris.addView(judul, paramsJudulTeks);

        final TextView jumlah = Theme.teks(getContext, "", Theme.DESC_SP, Theme.MUTED);
        judulBaris.addView(jumlah);
        // Daftar dipantau tiap detik, jadi muat ulang cukup berupa tombol ikon
        // kecil di sudut header, sejajar dengan ikon pemain di sisi kiri.
        final IkonFitur muatUlang = new IkonFitur(getContext, IkonFitur.ULANG, false);
        muatUlang.setContentDescription("Muat ulang daftar");
        muatUlang.setClickable(true);
        muatUlang.setFocusable(true);
        LinearLayout.LayoutParams paramsMuat = new LinearLayout.LayoutParams(dp(38), dp(38));
        paramsMuat.setMargins(dp(8), 0, 0, 0);
        muatUlang.setLayoutParams(paramsMuat);
        container.addView(judulBaris);

        TextView petunjuk = Theme.teks(getContext,
                "Ketuk kartu untuk memilih. Salin skin atau tendang tamu dari sini.",
                Theme.DESC_SP, Theme.MUTED);
        petunjuk.setPadding(dp(4), dp(2), dp(4), dp(6));
        container.addView(petunjuk);

        final LinearLayout daftar = new LinearLayout(getContext);
        daftar.setOrientation(LinearLayout.VERTICAL);
        container.addView(daftar);

        muatUlang.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                IsiDaftar isi = (IsiDaftar) daftar.getTag();
                if (isi != null)
                    isi.data = null;
                isiDaftarPemain(daftar, jumlah);
                // Putaran penuh sebagai tanda daftar baru dibaca.
                v.animate().cancel();
                v.setRotation(0f);
                v.animate().rotationBy(360f).setDuration(420L)
                        .setInterpolator(new DecelerateInterpolator()).start();
            }
        });
        judulBaris.addView(muatUlang);
        pasangPenjelasan(judulBaris, 0, featName);

        linLayout.addView(container);
        isiDaftarPemain(daftar, jumlah);

        // Daftar ikut detak Menu: dibaca ulang tiap detik hanya saat section ini
        // benar-benar terlihat, dan langsung saat menu atau section dibuka.
        // Perubahan (masuk, keluar, ganti nama/peran) tampil tanpa tombol muat ulang.
        daftarkanPemantau(new Runnable() {
            @Override
            public void run() {
                if (daftar.isShown())
                    isiDaftarPemain(daftar, jumlah);
            }
        });
    }

    @SuppressLint("SetTextI18n")
    private void isiDaftarPemain(LinearLayout daftar, TextView jumlah) {
        String data = ambilDaftarPemain();

        IsiDaftar isi = (IsiDaftar) daftar.getTag();
        if (isi == null) {
            isi = new IsiDaftar();
            daftar.removeAllViews();
            daftar.setTag(isi);
        }
        if (data.equals(isi.data))
            return;
        isi.data = data;

        Map<String, String[]> baru = new LinkedHashMap<String, String[]>();
        for (String baris : data.split("\n")) {
            String[] bagian = baris.split("\t", 3);
            if (bagian.length < 3 || bagian[0].length() == 0 || bagian[1].length() == 0)
                continue;
            baru.put(bagian[1], new String[]{bagian[0], bagian[2]});
        }

        // Pemain yang keluar dibuang lebih dulu. Bila yang pergi adalah pilihan
        // sekarang, pilihannya dilepas supaya tidak menunjuk kunci yang hilang.
        Iterator<Map.Entry<String, KartuPemain>> it = isi.kartu.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, KartuPemain> entri = it.next();
            if (!baru.containsKey(entri.getKey())) {
                daftar.removeView(entri.getValue().view);
                if (entri.getKey().equals(kunciTerpilih))
                    kunciTerpilih = null;
                it.remove();
            }
        }

        if (isi.kosong != null) {
            daftar.removeView(isi.kosong);
            isi.kosong = null;
        }

        int indeks = 0;
        for (Map.Entry<String, String[]> entri : baru.entrySet()) {
            String kunci = entri.getKey();
            String nama = entri.getValue()[0];
            String peran = entri.getValue()[1];
            KartuPemain kartu = isi.kartu.get(kunci);
            if (kartu != null && kartu.nama.equals(nama) && kartu.peran.equals(peran)) {
                // Isi sama: kartunya dipakai lagi, hanya urutannya yang dirapikan.
                if (daftar.getChildAt(indeks) != kartu.view) {
                    daftar.removeView(kartu.view);
                    daftar.addView(kartu.view, Math.min(indeks, daftar.getChildCount()));
                }
            } else {
                LinearLayout view = barisPemain(isi, kunci, nama, peran);
                if (kartu != null)
                    daftar.removeView(kartu.view);
                daftar.addView(view, Math.min(indeks, daftar.getChildCount()));
                isi.kartu.put(kunci, new KartuPemain(nama, peran, view));
            }
            indeks++;
        }

        jumlah.setText(indeks == 0 ? "" : indeks + " pemain");
        if (indeks == 0) {
            TextView kosong = Theme.teks(getContext, "Tidak ada pemain di room ini",
                    Theme.DESC_SP, Theme.MUTED);
            kosong.setGravity(Gravity.CENTER);
            kosong.setPadding(0, dp(14), 0, dp(14));
            daftar.addView(kosong);
            isi.kosong = kosong;
        }
    }

    private String ambilDaftarPemain() {
        try {
            String data = DaftarPemain();
            return data == null ? "" : data;
        } catch (Throwable t) {
            return "";
        }
    }

    // Satu pemain menjadi satu kartu: nama dan lencana perannya di atas, aksi di
    // bawah. Kartu sendiri tidak punya aksi karena menyalin atau menendang diri
    // sendiri tidak berguna, dan tendang hanya ada untuk tamu.
    private LinearLayout barisPemain(final IsiDaftar isi, final String kunci,
                                     final String nama, String peran) {
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.VERTICAL);
        baris.setPadding(dp(10), dp(8), dp(10), dp(8));
        LinearLayout.LayoutParams paramsBaris =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsBaris.setMargins(0, 0, 0, dp(6));
        baris.setLayoutParams(paramsBaris);
        baris.setBackground(kunci.equals(kunciTerpilih)
                ? Theme.kartuTerpilih(getContext)
                : Theme.kartu(getContext));

        LinearLayout kepala = new LinearLayout(getContext);
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = Theme.teks(getContext, nama, Theme.ROW_SP, Theme.TEXT);
        label.setSingleLine(true);
        label.setEllipsize(TextUtils.TruncateAt.END);
        label.setLayoutParams(new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));
        kepala.addView(label);
        kepala.addView(lencanaPeran(peran));
        baris.addView(kepala);

        baris.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                kunciTerpilih = kunci.equals(kunciTerpilih) ? null : kunci;
                segarkanPilihan(isi);
            }
        });

        if ("kamu".equals(peran))
            return baris;

        LinearLayout aksi = new LinearLayout(getContext);
        aksi.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams paramsAksi =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsAksi.setMargins(0, dp(6), 0, 0);
        aksi.setLayoutParams(paramsAksi);
        aksi.addView(tombolKecil("Salin skin", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean diterima;
                try {
                    diterima = SalinSkinPemain(kunci, nama);
                } catch (Throwable t) {
                    diterima = false;
                }
                // Hasil sebenarnya dikabarkan toast dari thread game; di sini
                // hanya antrean yang bisa langsung ditolak.
                if (!diterima)
                    Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                            Toast.LENGTH_SHORT).show();
            }
        }));
        if ("tamu".equals(peran)) {
            aksi.addView(tombolKecil("Tendang", new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    boolean diterima;
                    try {
                        diterima = TendangPemain(kunci, nama);
                    } catch (Throwable t) {
                        diterima = false;
                    }
                    if (!diterima)
                        Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                Toast.LENGTH_SHORT).show();
                }
            }));
        }
        baris.addView(aksi);
        return baris;
    }

    private void segarkanPilihan(IsiDaftar isi) {
        for (Map.Entry<String, KartuPemain> entri : isi.kartu.entrySet()) {
            entri.getValue().view.setBackground(entri.getKey().equals(kunciTerpilih)
                    ? Theme.kartuTerpilih(getContext)
                    : Theme.kartu(getContext));
        }
    }

    private Button tombolKecil(String teks, View.OnClickListener pendengar) {
        Button tombol = new Button(getContext);
        tombol.setText(teks);
        tombol.setTextSize(13f);
        tombol.setAllCaps(false);
        tombol.setTextColor(Theme.TITLE_TEXT);
        tombol.setTypeface(Theme.font(getContext));
        tombol.setBackground(Theme.tombolKecil(getContext));
        tombol.setMinWidth(0);
        tombol.setMinimumWidth(0);
        tombol.setMinHeight(0);
        tombol.setMinimumHeight(0);
        tombol.setPadding(dp(8), dp(6), dp(8), dp(6));
        tombol.setStateListAnimator(null);
        tombol.setElevation(0f);
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        params.setMargins(0, 0, dp(6), 0);
        tombol.setLayoutParams(params);
        tombol.setOnClickListener(pendengar);
        return tombol;
    }

    @SuppressLint("SetTextI18n")
    private TextView lencanaPeran(String peran) {
        int warna;
        if ("host".equals(peran))
            warna = Theme.ACCENT;
        else if ("kamu".equals(peran))
            warna = Theme.ICON_BOX;
        else
            warna = Theme.TRACK;

        int warnaTeks = warna == Theme.ACCENT ? Theme.TEXT : Theme.ACTIVE_BORDER;
        TextView lencana = Theme.teks(getContext, peran, 12f, warnaTeks);
        lencana.setGravity(Gravity.CENTER);
        lencana.setPadding(dp(6), dp(2), dp(6), dp(2));
        lencana.setBackground(Theme.lencana(getContext, warna));
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(dp(6), 0, 0, 0);
        lencana.setLayoutParams(params);
        return lencana;
    }

    private void Category(LinearLayout linLayout, String text) {
        if (linLayout == mSettings) {
            // Setelan memakai judul kelompok yang ringan, bukan pita cokelat.
            TextView judul = Theme.teks(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
            judul.setPadding(dp(4), dp(12), dp(4), dp(2));
            linLayout.addView(judul);
            return;
        }
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.HORIZONTAL);
        baris.setGravity(Gravity.CENTER_VERTICAL);
        baris.setBackground(Theme.sectionHeader(getContext));
        baris.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(10), 0, dp(2));
        baris.setLayoutParams(params);

        TextView textView = Theme.teks(getContext, Html.fromHtml(text), Theme.SECTION_SP, Theme.TITLE_TEXT);
        textView.setGravity(Gravity.CENTER);
        textView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        baris.addView(textView);
        linLayout.addView(baris);
    }

    private void TextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.teks(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
        textView.setPadding(dp(10), dp(4), dp(10), dp(2));
        linLayout.addView(textView);
    }

    /**
     * RichWebView tidak lagi memakai WebView. Tag dasar (b, i, u, font, a, br)
     * dirender TextView biasa, dan tautan tetap bisa diketuk.
     */
    private void WebTextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.teks(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.TEXT);
        textView.setMovementMethod(LinkMovementMethod.getInstance());
        textView.setLinkTextColor(Theme.ICON_BOX);
        textView.setPadding(dp(10), dp(4), dp(10), dp(4));
        linLayout.addView(textView);
    }

    /**
     * Baris kontrol: kotak ikon menempel di tepi kiri (hanya sudut kiri yang
     * melengkung), label di tengah, dan kontrol di kanan. Tinggi minimum 48dp
     * supaya seluruh baris nyaman disentuh.
     */
    private LinearLayout baris(String[] pola, View isi, View kontrol) {
        LinearLayout baris = new LinearLayout(getContext);
        baris.setOrientation(LinearLayout.HORIZONTAL);
        baris.setGravity(Gravity.CENTER_VERTICAL);
        baris.setBackground(Theme.baris(getContext));
        baris.setMinimumHeight(dp(48));
        baris.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(4), 0, dp(4));
        baris.setLayoutParams(params);

        // Kotak ikon menempel penuh di tepi kiri dan setinggi baris, tanpa margin.
        baris.addView(new IkonFitur(getContext, pola, true),
                new LinearLayout.LayoutParams(dp(IkonFitur.LEBAR_DP), MATCH_PARENT));

        LinearLayout.LayoutParams isiParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        isiParams.setMargins(dp(12), dp(6), dp(8), dp(6));
        baris.addView(isi, isiParams);

        if (kontrol != null)
            baris.addView(kontrol, new LinearLayout.LayoutParams(dp(52), dp(48)));
        return baris;
    }

    /**
     * Baris posisi pemain: koordinat tile dan dunia dibaca ulang tiap detik
     * selama barisnya terlihat.
     */
    private void BarisPosisi(LinearLayout linLayout, String featName) {
        LinearLayout kolom = new LinearLayout(getContext);
        kolom.setOrientation(LinearLayout.VERTICAL);

        TextView judul = Theme.teks(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final TextView nilai = Theme.teks(getContext, "Menunggu posisi...", Theme.DESC_SP,
                Theme.ICON_BOX);
        kolom.addView(judul);
        kolom.addView(nilai);

        final LinearLayout baris = baris(IkonFitur.POSISI, kolom, null);
        pasangPenjelasan(baris, 0, featName);
        linLayout.addView(baris);

        daftarkanPemantau(new Runnable() {
            @Override
            public void run() {
                if (baris.isShown())
                    nilai.setText(PosisiTeks());
            }
        });
    }

    private String PosisiTeks() {
        String data;
        try {
            data = PosisiPemain();
        } catch (Throwable t) {
            data = null;
        }
        if (data == null || data.length() == 0)
            return "Posisi belum terbaca";

        String[] bagian = data.split("\t", -1);
        if (bagian.length < 4)
            return "Posisi belum terbaca";
        if (bagian[0].length() > 0 && bagian[1].length() > 0)
            return "Tile UI " + bagian[0] + ", " + bagian[1] + "   Dunia " + bagian[2] + ", "
                    + bagian[3];
        return "Dunia " + bagian[2] + ", " + bagian[3];
    }

    private Button teksTombol(CharSequence teks) {
        Button button = new Button(getContext);
        button.setText(teks);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(getContext));
        button.setAllCaps(false); //Disable caps to support html
        button.setGravity(Gravity.CENTER);
        button.setBackground(Theme.tekanOverlay(getContext));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(0);
        button.setMinimumHeight(0);
        button.setPadding(dp(6), 0, dp(6), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        return button;
    }

    private LinearLayout kotakTombol(String[] pola, View isi) {
        LinearLayout kotak = new LinearLayout(getContext);
        kotak.setOrientation(LinearLayout.HORIZONTAL);
        kotak.setGravity(Gravity.CENTER_VERTICAL);
        kotak.setBackground(Theme.tombol(getContext));
        kotak.setPadding(dp(3), dp(3), dp(8), dp(3));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, dp(50));
        params.setMargins(0, dp(4), 0, dp(4));
        kotak.setLayoutParams(params);

        // Ikon menempel penuh di dalam garis tombol.
        kotak.addView(new IkonFitur(getContext, pola, true),
                new LinearLayout.LayoutParams(dp(IkonFitur.LEBAR_DP - 3), MATCH_PARENT));

        LinearLayout.LayoutParams isiParams = new LinearLayout.LayoutParams(0, MATCH_PARENT, 1f);
        kotak.addView(isi, isiParams);
        return kotak;
    }

    /**
     * Tekan lama pada baris fitur membuka dialog penjelasan. Listener dipasang
     * di baris dan di kontrol yang bisa menelan sentuhan (tombol, sakelar,
     * spinner), supaya tekan lama tetap bekerja di bagian mana pun.
     */
    private void pasangPenjelasan(View tampilan, final int featNum, final String featName) {
        if (tampilan == null)
            return;
        tampilan.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                tampilkanPenjelasan(featNum, featName);
                return true;
            }
        });
    }

    private void tampilkanPenjelasan(int featNum, String featName) {
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext);
        builder.setTitle(Html.fromHtml(featName));
        builder.setMessage(Penjelasan.untuk(featNum, featName));
        builder.setPositiveButton("Mengerti", null);
        tampilkanDialog(builder);
    }

    private void tampilkanDialog(AlertDialog.Builder builder) {
        AlertDialog dialog = builder.create();
        if (overlayRequired) {
            Objects.requireNonNull(dialog.getWindow()).setType(
                    Build.VERSION.SDK_INT >= 26 ? 2038 : 2002);
        }
        dialog.show();
        if (dialog.getWindow() != null) {
            gayaDialog(dialog);
            Theme.applyFont(dialog.getWindow().getDecorView());
        }
    }

    /**
     * Dialog bawaan bertema putih dan tidak memakai palet kayu. Latar, judul,
     * tombol, dan kolom isian disamakan dengan panel menu; font tetap diurus
     * {@link Theme#applyFont(View)}.
     */
    private void gayaDialog(AlertDialog dialog) {
        Window window = dialog.getWindow();
        if (window == null)
            return;
        window.setBackgroundDrawable(Theme.dialogPanel(getContext));

        View akar = window.getDecorView();
        gayaIsiDialog(akar);

        int idJudul = getContext.getResources().getIdentifier("alertTitle", "id", "android");
        TextView judul = idJudul == 0 ? null : (TextView) dialog.findViewById(idJudul);
        if (judul == null)
            judul = (TextView) dialog.findViewById(android.R.id.title);
        if (judul != null) {
            judul.setTextColor(Theme.TITLE_TEXT);
            judul.setBackground(Theme.dialogTitle(getContext));
            judul.setPadding(dp(12), dp(8), dp(12), dp(8));
        }

        TextView pesan = (TextView) dialog.findViewById(android.R.id.message);
        if (pesan != null)
            pesan.setTextColor(Theme.TEXT);

        int[] jenis = {AlertDialog.BUTTON_POSITIVE, AlertDialog.BUTTON_NEGATIVE,
                AlertDialog.BUTTON_NEUTRAL};
        for (int i : jenis) {
            Button tombol = dialog.getButton(i);
            if (tombol == null)
                continue;
            tombol.setTextColor(Theme.TITLE_TEXT);
            tombol.setAllCaps(false);
            tombol.setBackground(Theme.tombolKecil(getContext));
            tombol.setPadding(dp(12), dp(8), dp(12), dp(8));
        }
    }

    private void gayaIsiDialog(View view) {
        if (view instanceof EditText) {
            EditText isian = (EditText) view;
            isian.setTextColor(Theme.TEXT);
            isian.setHintTextColor(Theme.MUTED);
            isian.setBackground(Theme.dialogInput(getContext));
            isian.setPadding(dp(10), dp(8), dp(10), dp(8));
        } else if (view instanceof Button) {
            Button tombol = (Button) view;
            tombol.setTextColor(Theme.TITLE_TEXT);
            tombol.setAllCaps(false);
            tombol.setBackground(Theme.tombolKecil(getContext));
        } else if (view instanceof TextView) {
            ((TextView) view).setTextColor(Theme.TEXT);
        }

        if (view instanceof ViewGroup) {
            ViewGroup grup = (ViewGroup) view;
            for (int i = 0; i < grup.getChildCount(); i++)
                gayaIsiDialog(grup.getChildAt(i));
        }
    }

    private boolean isViewCollapsed() {
        return rootFrame == null || mCollapsed.getVisibility() == View.VISIBLE;
    }

    private int dp(int i) {
        return (int) Theme.dp(getContext, i);
    }

    public void setVisibility(int view) {
        if (rootFrame != null) {
            rootFrame.setVisibility(view);
        }
    }

    public void onDestroy() {
        detakJalan = false;
        aturAnimasiIkon(false);
        handlerDetak.removeCallbacksAndMessages(null);
        if (rootFrame != null) {
            mWindowManager.removeView(rootFrame);
        }
    }
}
