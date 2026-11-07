//Please don't replace listeners with lambda!

package com.android.support;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
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
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;
import static android.view.WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS;
import static android.widget.RelativeLayout.ALIGN_PARENT_RIGHT;

public class Menu {
    public static final String TAG = BKLog.TAG; //Tag for logcat

    // Tampilan panel kayu. Semua warna ada di Theme.
    int MENU_WIDTH = 340;
    int MENU_HEIGHT = 210;
    int POS_X = 0;
    int POS_Y = 100;
    int ICON_SIZE = 45; //Change both width and height of image
    float ICON_ALPHA = 0.7f; //Transparent

    // Selang menunggu library game dimuat (ms), dan kunci setelan tampilan
    // yang disimpan di preferensi yang sama dengan fitur.
    private static final long WAIT_INITIAL_MS = 500L;
    private static final long WAIT_LIB_MS = 600L;
    private static final String KEY_X = "ui_posisi_x";
    private static final String KEY_Y = "ui_posisi_y";
    private static final String KEY_FOLD = "ui_lipat_";
    // Chip angka slider: tinggi gambar 26dp di dalam area sentuh 48dp.
    private static final float BUTTON_SMALL_INSET_DP = 9f;

    RelativeLayout mCollapsed, mRootContainer;
    LinearLayout mExpanded, mods, mSettings, mCollapse;
    WindowManager mWindowManager;
    WindowManager.LayoutParams vmParams;
    ImageView startimage;
    ImageView iconImage;
    Drawable iconDrawable;
    String iconSource = "";
    String sourceInstalled;
    boolean imageReady;
    FrameLayout rootFrame;
    ScrollView scrollView;
    boolean stopChecking, overlayRequired;
    Context getContext;
    private MenuDialog menuDialog;
    private boolean hancur;

    //initialize methods from the native library
    native void Init(Context context, TextView title, TextView subTitle);

    native String Icon();

    native String IconWebViewData();

    native String[] GetFeatureList();

    native String[] SettingsList();

    native boolean IsGameLibLoaded();

    // Section Room: daftar tamu berformat "nama<TAB>kunci<TAB>peran" per baris.
    native String PlayerList();

    // Posisi pemain lokal: "tileX<TAB>tileY<TAB>duniaX<TAB>duniaY". Bagian tile
    // kosong bila game tidak menyediakan field tile.
    native String PlayerPosition();

    native boolean KickPlayer(String key, String name, boolean permanent);

    native boolean KickHost(String name);

    native boolean KickAll();

    native boolean CopyPlayerSkin(String key, String name);

    // Kunci pemain yang kartunya sedang dipilih. Pilihan disimpan sebagai teks
    // kunci, bukan indeks, jadi tetap menunjuk pemain yang sama saat daftar
    // berubah urutan dan otomatis dilepas saat pemain itu keluar.
    String keySelected;

    // Pengembali nilai bawaan untuk setelan tampilan (ukuran, opasitas, gambar ikon).
    private final List<Runnable> setterReload = new ArrayList<Runnable>();

    // Satu detak milik Menu untuk semua isi yang harus tetap segar (daftar
    // pemain, posisi). Detak berjalan selama menu terbuka, tidak bergantung
    // pada urusan attach/detach tiap view, dan langsung jalan sekali saat menu
    // dibuka supaya isi tidak basi.
    private static final long TICK_MS = 1000L;
    private final Handler handlerTick = new Handler(android.os.Looper.getMainLooper());
    private final List<Runnable> pemantau = new ArrayList<Runnable>();
    private boolean tickRun;
    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (!tickRun)
                return;
            if (mExpanded != null && mExpanded.getVisibility() == View.VISIBLE)
                jalankanPemantau();
            handlerTick.postDelayed(this, TICK_MS);
        }
    };

    private void jalankanPemantau() {
        // Tanpa salinan daftar tiap detak. Pemantau hanya bertambah, dan yang
        // mendaftar saat putaran berjalan baru ikut putaran berikutnya.
        int count = pemantau.size();
        for (int i = 0; i < count; i++) {
            try {
                pemantau.get(i).run();
            } catch (Throwable t) {
                Log.d(TAG, "Pemantau gagal: " + t);
            }
        }
    }

    private void daftarkanPemantau(Runnable r) {
        pemantau.add(r);
    }

    private void startTicker() {
        if (tickRun)
            return;
        tickRun = true;
        handlerTick.postDelayed(tick, TICK_MS);
    }

    private void openMenu() {
        setAnimationIcon(false);
        mCollapsed.setVisibility(View.GONE);
        mExpanded.setVisibility(View.VISIBLE);
        Transition.muncul(mExpanded, 1f);
        jalankanPemantau();
    }

    /**
     * Menyamakan ukuran, opasitas, dan sumber ikon collapsed dengan setelan.
     * Ikon bawaan tetap di startimage. Sumber dari pengguna (URL, GIF) didekode
     * IkonGambar menjadi Drawable dan ditampilkan di ImageView biasa, tanpa WebView.
     */
    private void applyIcon() {
        int sisi = dp(ICON_SIZE);
        for (View icon : new View[]{startimage, iconImage}) {
            ViewGroup.LayoutParams params = icon.getLayoutParams();
            params.width = sisi;
            params.height = sisi;
            icon.setLayoutParams(params);
        }
        mCollapsed.setAlpha(ICON_ALPHA);

        String source = iconSource == null ? "" : iconSource.trim();
        if (source.length() == 0)
            source = IconWebViewData();
        if (source == null || source.length() == 0) {
            sourceInstalled = null;
            releaseImage();
            showIcon();
            return;
        }

        // Sumber yang sama tidak dimuat lagi, jadi geseran slider ukuran dan
        // opasitas tidak memicu unduhan atau dekode ulang.
        if (!source.equals(sourceInstalled)) {
            sourceInstalled = source;
            releaseImage();
            // Ikon bawaan tetap tampil sampai gambarnya siap. Bila gagal, tetap
            // dipakai dan dicoba lagi saat menu dibuat berikutnya.
            final String target = source;
            IconCache.load(getContext, target, new IconCache.Pendengar() {
                @Override
                public void ready(Drawable icon) {
                    if (!target.equals(sourceInstalled))
                        return;
                    iconDrawable = icon;
                    iconImage.setLayerType(IconImage.needSoftware(icon)
                            ? View.LAYER_TYPE_SOFTWARE : View.LAYER_TYPE_NONE, null);
                    iconImage.setImageDrawable(icon);
                    imageReady = true;
                    showIcon();
                    setAnimationIcon(isViewCollapsed());
                }

                @Override
                public void fail() {
                    Log.d(TAG, "Ikon gagal dimuat, memakai ikon bawaan");
                }
            });
        }
        showIcon();
    }

    private void releaseImage() {
        setAnimationIcon(false);
        iconImage.setImageDrawable(null);
        iconDrawable = null;
        imageReady = false;
    }

    private void showIcon() {
        iconImage.setVisibility(imageReady ? View.VISIBLE : View.GONE);
        startimage.setVisibility(imageReady ? View.GONE : View.VISIBLE);
    }

    /** GIF hanya berjalan saat ikon kecil terlihat; saat menu terbuka dihentikan supaya hemat CPU. */
    private void setAnimationIcon(boolean run) {
        if (iconDrawable != null)
            IconImage.jalankan(iconDrawable, run);
    }

    //Here we write the code for our Menu
    // Reference: https://www.androidhive.info/2016/11/android-floating-widget-like-facebook-chat-head/
    public Menu(Context context) {

        getContext = context;
        Preferences.context = context;
        menuDialog = new MenuDialog(context);
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
        String iconData = Icon();
        if (iconData != null && iconData.length() > 0) {
            byte[] decode = Base64.decode(iconData, 0);
            startimage.setImageBitmap(BitmapFactory.decodeByteArray(decode, 0, decode.length));
        }
        startimage.setContentDescription("Buka menu");
        ((ViewGroup.MarginLayoutParams) startimage.getLayoutParams()).topMargin = dp(10);
        //Initialize event handlers for buttons, etc.
        startimage.setOnTouchListener(onTouchListener());
        startimage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                openMenu();
            }
        });

        //********** Ikon dari sumber pilihan pengguna (URL atau GIF) **********
        iconImage = new ImageView(context);
        iconImage.setLayoutParams(new RelativeLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));
        iconImage.getLayoutParams().height = dp(ICON_SIZE);
        iconImage.getLayoutParams().width = dp(ICON_SIZE);
        ((ViewGroup.MarginLayoutParams) iconImage.getLayoutParams()).topMargin = dp(10);
        iconImage.setScaleType(ImageView.ScaleType.FIT_CENTER);
        iconImage.setVisibility(View.GONE);
        iconImage.setOnTouchListener(onTouchListener());
        iconImage.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                openMenu();
            }
        });

        //********** Settings **********
        mSettings = new LinearLayout(context);
        mSettings.setOrientation(LinearLayout.VERTICAL);
        mSettings.setPadding(dp(10), dp(6), dp(10), dp(8));
        fillSettings(SettingsList());

        //********** Header panel: judul di tengah, hanya tombol minimize di kanan **********
        RelativeLayout header = new RelativeLayout(context);
        header.setBackground(Theme.header(context));
        header.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(56)));

        RelativeLayout.LayoutParams rlTitle = new RelativeLayout.LayoutParams(MATCH_PARENT, MATCH_PARENT);
        final TextView title = Theme.text(context, "", Theme.TITLE_SP, Theme.TITLE_TEXT);
        title.setGravity(Gravity.CENTER);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        title.setPadding(dp(48), 0, dp(48), 0);
        title.setLayoutParams(rlTitle);

        // Tombol sudut bergaya game: kotak hitam, garis putih, segitiga pixel
        // menghadap kanan. Gambarnya 34dp, area sentuhnya 48dp.
        CornerButton minimal = new CornerButton(context, IconPattern.TRIANGLE_LEFT);
        RelativeLayout.LayoutParams rlMinimal = new RelativeLayout.LayoutParams(
                dp(Theme.TARGET_DP), dp(Theme.TARGET_DP));
        rlMinimal.addRule(ALIGN_PARENT_RIGHT);
        rlMinimal.addRule(RelativeLayout.CENTER_VERTICAL);
        rlMinimal.rightMargin = dp(1);
        minimal.setLayoutParams(rlMinimal);
        minimal.setContentDescription("Kecilkan menu");
        minimal.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(ICON_ALPHA);
                mExpanded.setVisibility(View.GONE);
                setAnimationIcon(true);
            }
        });

        header.addView(title);
        header.addView(minimal);

        //********** Sub title **********
        final TextView subTitle = Theme.text(context, "", Theme.DESC_SP, Theme.MUTED);
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
        final Button settingBtn = buttonText("Setelan");
        settingBtn.setBackground(Theme.pressButton(context));
        LinearLayout.LayoutParams paramsSetting = new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f);
        paramsSetting.setMargins(0, 0, dp(6), 0);
        settingBtn.setLayoutParams(paramsSetting);
        settingBtn.setOnClickListener(new View.OnClickListener() {
            CharSequence titleOrigin = "";
            CharSequence subTitleOrigin = "";
            int scrollOrigin;

            @Override
            public void onClick(View v) {
                try {
                    if (mSettings.getParent() == scrollView) {
                        scrollView.removeView(mSettings);
                        scrollView.addView(mods);
                        title.setText(titleOrigin);
                        subTitle.setText(subTitleOrigin);
                        settingBtn.setText("Setelan");
                        Transition.muncul(scrollView, 1f);
                        // Posisi gulir daftar fitur dikembalikan setelah layout selesai.
                        scrollView.post(new Runnable() {
                            @Override
                            public void run() {
                                scrollView.scrollTo(0, scrollOrigin);
                            }
                        });
                    } else {
                        scrollOrigin = scrollView.getScrollY();
                        titleOrigin = title.getText();
                        subTitleOrigin = subTitle.getText();
                        scrollView.removeView(mods);
                        scrollView.addView(mSettings);
                        scrollView.scrollTo(0, 0);
                        // Judul utama tetap "BOKU Menu"; hanya subjudul yang
                        // menandai halaman setelan.
                        subTitle.setText("Menu \\ Setelan");
                        settingBtn.setText("Kembali");
                        Transition.muncul(scrollView, 1f);
                    }
                } catch (IllegalStateException ignored) {
                }
            }
        });
        footer.addView(settingBtn);

        final Button hideBtn = buttonText("Sembunyikan");
        hideBtn.setBackground(Theme.pressButton(context));
        hideBtn.setLayoutParams(new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f));
        hideBtn.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                mCollapsed.setVisibility(View.VISIBLE);
                mCollapsed.setAlpha(0);
                mExpanded.setVisibility(View.GONE);
                setAnimationIcon(false);
                Toast.makeText(view.getContext(), "Ikon disembunyikan. Ingat posisi ikon tersembunyi", Toast.LENGTH_LONG).show();
            }
        });
        footer.addView(hideBtn);

        //********** Adding view components **********
        mRootContainer.addView(mCollapsed);
        mRootContainer.addView(mExpanded);
        mCollapsed.addView(startimage);
        mCollapsed.addView(iconImage);
        mExpanded.addView(header);
        Theme.Zigzag zigTop = new Theme.Zigzag(context, Theme.HEADER, true);
        zigTop.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigTop, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(subTitle, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        scrollView.addView(mods);
        mExpanded.addView(scrollView);
        Theme.Zigzag zigBottom = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBottom.setBackgroundColor(Theme.BODY);
        mExpanded.addView(zigBottom, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));
        mExpanded.addView(footer);

        applyIcon();
        Init(context, title, subTitle);
        Theme.applyFont(mExpanded);
        Theme.applyFont(mSettings);
        startTicker();
    }

    public void ShowMenu() {
        if (mRootContainer.getParent() == null)
            rootFrame.addView(mRootContainer);

        // Handler milik Menu (Looper utama) dipakai ulang, jadi onDestroy
        // cukup membatalkan semuanya sekaligus dan tidak ada pemeriksa yang
        // terus berulang setelah menu dihancurkan.
        final Runnable pemuat = new Runnable() {
            boolean viewLoaded = false;

            @Override
            public void run() {
                if (hancur)
                    return;
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
                    handlerTick.postDelayed(this, WAIT_LIB_MS);
                } else {
                    mods.removeAllViews();
                    String[] list = GetFeatureList();
                    if (list != null)
                        featureList(list, mods);
                    Theme.applyFont(mods);
                }
            }
        };
        handlerTick.postDelayed(pemuat, WAIT_INITIAL_MS);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerWindowService() {
        restorePosition();
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
        menuDialog.setOverlay(true);
    }

    @SuppressLint("WrongConstant")
    public void SetWindowManagerActivity() {
        restorePosition();
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

    /**
     * Posisi terakhir ikon dipulihkan dan dijepit ke layar, supaya ikon tidak
     * hilang di luar layar bila orientasi atau ukuran layar berubah.
     */
    private void restorePosition() {
        try {
            Preferences prefs = Preferences.with(getContext);
            DisplayMetrics dm = getContext.getResources().getDisplayMetrics();
            int maxX = Math.max(0, dm.widthPixels - dp(ICON_SIZE));
            int maxY = Math.max(0, dm.heightPixels - dp(ICON_SIZE));
            POS_X = Math.max(0, Math.min(maxX, prefs.readInt(KEY_X, POS_X)));
            POS_Y = Math.max(0, Math.min(maxY, prefs.readInt(KEY_Y, POS_Y)));
        } catch (Throwable t) {
            Log.d(TAG, "Posisi tersimpan gagal dibaca: " + t);
        }
    }

    private void storePosition() {
        try {
            Preferences prefs = Preferences.with(getContext);
            prefs.writeInt(KEY_X, vmParams.x);
            prefs.writeInt(KEY_Y, vmParams.y);
        } catch (Throwable t) {
            Log.d(TAG, "Posisi gagal disimpan: " + t);
        }
    }

    private View.OnTouchListener onTouchListener() {
        // Ambang geser mengikuti kepadatan layar, bukan 10 piksel tetap.
        final int thresholdDrag = ViewConfiguration.get(getContext).getScaledTouchSlop();
        return new View.OnTouchListener() {
            private float initialTouchX, initialTouchY;
            private int initialX, initialY;
            private boolean bergeser;

            public boolean onTouch(View view, MotionEvent motionEvent) {
                int action = motionEvent.getActionMasked();
                switch (action) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = vmParams.x;
                        initialY = vmParams.y;
                        initialTouchX = motionEvent.getRawX();
                        initialTouchY = motionEvent.getRawY();
                        bergeser = false;
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        mExpanded.setAlpha(1f);
                        mCollapsed.setAlpha(ICON_ALPHA);
                        if (bergeser) {
                            storePosition();
                        } else if (action == MotionEvent.ACTION_UP && isViewCollapsed()) {
                            //Jari nyaris tidak bergeser, jadi ini ketukan: menu dibuka.
                            //Versi lama membandingkan selisih bertanda (< 10), sehingga
                            //geseran ke kiri atau ke atas ikut dihitung sebagai ketukan.
                            openMenu();
                        }
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        float dx = motionEvent.getRawX() - initialTouchX;
                        float dy = motionEvent.getRawY() - initialTouchY;
                        if (!bergeser && Math.abs(dx) < thresholdDrag && Math.abs(dy) < thresholdDrag)
                            return true;
                        bergeser = true;
                        mExpanded.setAlpha(0.5f);
                        mCollapsed.setAlpha(0.5f);
                        //Calculate the X and Y coordinates of the view.
                        vmParams.x = initialX + (int) dx;
                        vmParams.y = initialY + (int) dy;
                        //Update the layout with new X & Y coordinate
                        if (mWindowManager != null && !hancur) {
                            try {
                                mWindowManager.updateViewLayout(rootFrame, vmParams);
                            } catch (IllegalArgumentException e) {
                                Log.d(TAG, "Jendela menu sudah dilepas: " + e);
                            }
                        }
                        return true;
                    default:
                        return false;
                }
            }
        };
    }

    private static final String GROUP_PUBLIC = "Umum";
    private static final String GROUP_ICON = "Ikon melayang";

    /**
     * Halaman setelan: fitur dikelompokkan dengan judul kecil (Umum, Ikon
     * melayang) dan diakhiri tombol setel ulang tampilan. Pengelompokan hanya
     * dipakai bila semua entri punya nomor fitur dan native tidak membuat
     * kategori sendiri; selain itu daftar dari native dipakai apa adanya.
     */
    private void fillSettings(String[] list) {
        if (list == null)
            return;
        featureList(groupSetting(list), mSettings);

        if (!setterReload.isEmpty()) {
            final Button repeat = buttonText("Setel ulang tampilan");
            repeat.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    for (Runnable r : setterReload)
                        r.run();
                    Toast.makeText(getContext, "Tampilan disetel ulang", Toast.LENGTH_SHORT).show();
                }
            });
            LinearLayout box = boxButton(IconPattern.RELOAD, repeat);
            ((LinearLayout.LayoutParams) box.getLayoutParams()).topMargin = dp(12);
            mSettings.addView(box);
        }
    }

    private String[] groupSetting(String[] list) {
        List<String> general = new ArrayList<String>();
        List<String> icon = new ArrayList<String>();
        for (String entry : list) {
            Integer number = numberEntry(entry);
            if (number == null)
                return list;
            if (number == -4 || number == -5 || number == -6)
                icon.add(entry);
            else
                general.add(entry);
        }
        if (general.isEmpty() || icon.isEmpty())
            return list;

        List<String> result = new ArrayList<String>();
        result.add("Category_" + GROUP_PUBLIC);
        result.addAll(general);
        result.add("Category_" + GROUP_ICON);
        result.addAll(icon);
        return result.toArray(new String[0]);
    }

    /** Nomor fitur di awal entri, atau null bila entri tidak bernomor (kategori, teks, tautan). */
    private Integer numberEntry(String entry) {
        String f = entry.replaceFirst("_True", "").replaceFirst("CollapseAdd_", "");
        String initial = f.split("_")[0];
        if (initial.length() > 0 && (TextUtils.isDigitsOnly(initial) || initial.matches("-[0-9]+"))) {
            try {
                return Integer.parseInt(initial);
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
            // Satu entri rusak dari native tidak boleh menjatuhkan seluruh menu.
            try {
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
                        RowPosition(linearLayout, strSplit[1]);
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
            } catch (RuntimeException e) {
                Log.e(TAG, "Entri fitur dilewati: " + listFT[i], e);
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

        TextView label = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), label, switchR);
        row.setBackground(Theme.rowPress(getContext));
        row.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                switchR.toggle();
            }
        });
        installExplanation(row, featNum, featName);
        installExplanation(switchR, featNum, featName);
        linLayout.addView(row);
    }

    private void SeekBar(LinearLayout linLayout, final int featNum, final String featName,
                         final int min, final int max, final int def, final int desimal) {
        // Nilai 0 adalah nilai sah untuk gravitasi, jadi default hanya dipakai
        // bila fitur ini belum pernah disimpan.
        boolean saved = Preferences.alreadySaved(featName, featNum);
        int initialValue = saved ? Preferences.loadPrefInt(featName, featNum) : def;
        if (initialValue < min)
            initialValue = min;
        if (initialValue > max)
            initialValue = max;
        if (!saved)
            Preferences.changeFeatureInt(featName, featNum, initialValue);

        // Opasitas dan ukuran ikon langsung dipakai saat slider dibaca.
        if (featNum == -4)
            ICON_ALPHA = initialValue / 100f;
        else if (featNum == -5)
            ICON_SIZE = initialValue;

        final String unit = unitFeature(featNum);

        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_VERTICAL);

        // Baris judul: nama fitur di kiri, angka di kanan. Ketuk angka untuk
        // mengembalikan slider ke nilai bawaan.
        LinearLayout titleRow = new LinearLayout(getContext);
        titleRow.setOrientation(LinearLayout.HORIZONTAL);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);

        final TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        titleRow.addView(title, new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));

        final TextView valueText = Theme.text(getContext, textValue(initialValue, desimal) + unit,
                Theme.DESC_SP, Theme.TITLE_TEXT);
        valueText.setGravity(Gravity.CENTER);
        valueText.setSingleLine(true);
        valueText.setMinWidth(dp(44));
        valueText.setPadding(dp(8), dp(2), dp(8), dp(2));
        valueText.setBackground(Theme.smallButton(getContext));
        valueText.setContentDescription(featName + ", ketuk untuk reset");
        titleRow.addView(valueText, new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT));

        final SeekBar seekBar = new SeekBar(getContext);
        seekBar.setProgressDrawable(Theme.seekbarProgress(getContext));
        seekBar.setThumb(Theme.seekbarThumb(getContext));
        seekBar.setThumbOffset(0);
        // Padding mendatar setengah lebar thumb supaya thumb tidak terpotong di ujung.
        seekBar.setPadding(dp(5), 0, dp(5), 0);
        // Rentang digeser ke nol supaya setMin yang hanya ada di Oreo ke atas
        // tidak dibutuhkan.
        seekBar.setMax(max - min);
        seekBar.setProgress(initialValue - min);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            public void onProgressChanged(SeekBar seekBar, int i, boolean z) {
                int value = i + min;
                if (value < min)
                    value = min;
                if (value > max)
                    value = max;
                Preferences.changeFeatureInt(featName, featNum, value);
                valueText.setText(textValue(value, desimal) + unit);
                if (featNum == -4 || featNum == -5) {
                    if (featNum == -4)
                        ICON_ALPHA = value / 100f;
                    else
                        ICON_SIZE = value;
                    applyIcon();
                }
            }
        });

        valueText.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Lewat jalur geseran yang sama, jadi native ikut menerima nilai bawaan.
                seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
            }
        });

        if (featNum == -4 || featNum == -5) {
            setterReload.add(new Runnable() {
                @Override
                public void run() {
                    seekBar.setProgress(Math.max(0, Math.min(max, def) - min));
                }
            });
        }

        column.addView(titleRow, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        column.addView(seekBar, new LinearLayout.LayoutParams(MATCH_PARENT, dp(Theme.SLIDER_HEIGHT_DP)));
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), column, null);
        installExplanation(row, featNum, featName);
        linLayout.addView(row);
    }

    /** Satuan angka slider: pengali kecepatan, persen opasitas, atau dp ikon. */
    private String unitFeature(int featNum) {
        if (featNum == 23 || featNum == 24)
            return "x";
        if (featNum == -4)
            return "%";
        if (featNum == -5)
            return "dp";
        return "";
    }

    /** Angka slider ditampilkan dengan jumlah desimal yang ditulis di feature list. */
    private String textValue(int value, int desimal) {
        if (desimal <= 0)
            return String.valueOf(value);
        return String.format(Locale.US, "%." + desimal + "f", value / 10f);
    }

    private void Button(LinearLayout linLayout, final int featNum, final String featName) {
        final Button button = buttonText(Html.fromHtml(featName));
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

        LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    private void ButtonLink(LinearLayout linLayout, final String featName, final String url) {
        final Button button = buttonText(Html.fromHtml(featName));
        button.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                try {
                    Intent intent = new Intent(Intent.ACTION_VIEW);
                    intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setData(Uri.parse(url));
                    getContext.startActivity(intent);
                } catch (RuntimeException e) {
                    // Tidak ada aplikasi yang bisa membuka tautan.
                    Toast.makeText(getContext, "Tautan tidak bisa dibuka", Toast.LENGTH_SHORT).show();
                }
            }
        });
        final LinearLayout box = boxButton(IconPattern.TELEGRAM, button);
        installExplanation(box, -100, featName);
        installExplanation(button, -100, featName);
        linLayout.addView(box);
    }

    private void ButtonOnOff(LinearLayout linLayout, final int featNum, String featName, boolean switchedOn) {
        final Button button = buttonText("");
        final String finalfeatName = featName.replace("OnOff_", "");
        boolean isOn = Preferences.loadPrefBool(featName, featNum, switchedOn);
        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        if (isOn) {
            button.setText(Html.fromHtml(finalfeatName + ": ON"));
            box.setBackground(Theme.activeButton(getContext));
            isOn = false;
        } else {
            button.setText(Html.fromHtml(finalfeatName + ": OFF"));
            box.setBackground(Theme.offButton(getContext));
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
                    box.setBackground(Theme.activeButton(getContext));
                    isOn = false;
                } else {
                    button.setText(Html.fromHtml(finalfeatName + ": OFF"));
                    box.setBackground(Theme.offButton(getContext));
                    isOn = true;
                }
            }
        });
        installExplanation(box, featNum, finalfeatName);
        installExplanation(button, featNum, finalfeatName);
        linLayout.addView(box);
    }

    private void Spinner(LinearLayout linLayout, final int featNum, final String featName, final String list) {
        final List<String> lists = new ArrayList<>(Arrays.asList(list.split(",")));

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
        int selected = Preferences.loadPrefInt(featName, featNum);
        spinner.setSelection(selected >= 0 && selected < lists.size() ? selected : 0);
        if (spinner.getBackground() != null)
            spinner.getBackground().setColorFilter(Theme.TEXT, PorterDuff.Mode.SRC_ATOP);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parentView, View selectedItemView, int position, long id) {
                Preferences.changeFeatureInt(spinner.getSelectedItem().toString(), featNum, position);
                View anak = parentView.getChildAt(0);
                if (anak instanceof TextView)
                    ((TextView) anak).setTextColor(Theme.TEXT);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), spinner, null);
        installExplanation(row, featNum, featName);
        installExplanation(spinner, featNum, featName);
        linLayout.addView(row);
    }

    private void InputNum(LinearLayout linLayout, int featNum, String featName, int maxValue) {
        InputNumber(linLayout, featNum, featName, maxValue, false);
    }

    private void InputLNum(LinearLayout linLayout, int featNum, String featName, long maxValue) {
        InputNumber(linLayout, featNum, featName, maxValue, true);
    }

    /** Tombol angka: Int dan Long memakai satu kode, hanya batas digit dan jalur simpannya berbeda. */
    private void InputNumber(LinearLayout linLayout, final int featNum, final String featName,
                            final long maxValue, final boolean length) {
        final Button button = buttonText("");
        long num = length ? Preferences.loadPrefLong(featName, featNum)
                : Preferences.loadPrefInt(featName, featNum);
        button.setText(Html.fromHtml(featName + ": " + num));
        final String hint = maxValue != 0 ? "Nilai maksimum: " + maxValue : null;
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Masukkan angka", hint, null, true, length ? 20 : 10,
                        new MenuDialog.Receiver() {
                            @Override
                            public void diterima(String text) {
                                long value;
                                try {
                                    String masukan = text.isEmpty() ? "0" : text;
                                    value = length ? Long.parseLong(masukan) : Integer.parseInt(masukan);
                                    if (maxValue != 0 && value >= maxValue)
                                        value = maxValue;
                                } catch (NumberFormatException ex) {
                                    if (maxValue != 0)
                                        value = maxValue;
                                    else
                                        value = length ? Long.MAX_VALUE : Integer.MAX_VALUE;
                                }

                                button.setText(Html.fromHtml(featName + ": " + value));
                                if (length)
                                    Preferences.changeFeatureLong(featName, featNum, value);
                                else
                                    Preferences.changeFeatureInt(featName, featNum, (int) value);
                            }
                        }, null, null);
            }
        });

        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    private void InputText(LinearLayout linLayout, final int featNum, final String featName) {
        String string = Preferences.loadPrefString(featName, featNum);
        if (featNum == -6) {
            InputImage(linLayout, featNum, featName, string);
            return;
        }

        final Button button = buttonText("");
        button.setText(Html.fromHtml(featName + ": " + string));
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Masukkan teks", null, null, false, 0, new MenuDialog.Receiver() {
                    @Override
                    public void diterima(String str) {
                        button.setText(Html.fromHtml(featName + ": " + str));
                        Preferences.changeFeatureString(featName, featNum, str);
                    }
                }, null, null);
            }
        });

        final LinearLayout box = boxButton(IconPattern.pattern(featNum, featName), button);
        installExplanation(box, featNum, featName);
        installExplanation(button, featNum, featName);
        linLayout.addView(box);
    }

    /**
     * Setelan gambar ikon: baris dua tingkat (nama di atas, sumber saat ini
     * di bawah dengan warna redup). Ketuk baris untuk mengubah, atau pilih
     * Bawaan di dialog untuk kembali ke ikon bawaan.
     */
    private void InputImage(LinearLayout linLayout, final int featNum, final String featName,
                             String initial) {
        iconSource = initial == null ? "" : initial;

        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);

        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        column.addView(title);

        final TextView value = Theme.text(getContext, summarySource(iconSource), Theme.DESC_SP,
                Theme.MUTED);
        value.setSingleLine(true);
        value.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        column.addView(value);

        final LinearLayout row = row(IconPattern.pattern(featNum, featName), column, null);
        row.setBackground(Theme.rowPress(getContext));
        installExplanation(row, featNum, featName);
        row.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                menuDialog.input("Sumber gambar ikon", "URL gambar atau GIF, kosongkan untuk bawaan",
                        iconSource, false, 0, new MenuDialog.Receiver() {
                            @Override
                            public void diterima(String text) {
                                setIconSource(text, featNum, featName, value);
                            }
                        }, "Bawaan", new Runnable() {
                            @Override
                            public void run() {
                                setIconSource("", featNum, featName, value);
                            }
                        });
            }
        });

        setterReload.add(new Runnable() {
            @Override
            public void run() {
                if (iconSource.length() > 0)
                    setIconSource("", featNum, featName, value);
            }
        });

        linLayout.addView(row);
    }

    private void setIconSource(String source, int featNum, String featName, TextView value) {
        source = source == null ? "" : source.trim();
        iconSource = source;
        value.setText(summarySource(source));
        applyIcon();
        Preferences.changeFeatureString(featName, featNum, source);
    }

    private String summarySource(String source) {
        return source.length() == 0 ? "Bawaan" : source;
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
                Preferences.changeFeatureBool(featName, featNum, isChecked);
            }
        });
        final LinearLayout row = row(IconPattern.pattern(featNum, featName), checkBox, null);
        installExplanation(row, featNum, featName);
        installExplanation(checkBox, featNum, featName);
        linLayout.addView(row);
    }

    @SuppressLint("SetTextI18n")
    private void RadioButton(LinearLayout linLayout, final int featNum, String featName, final String list) {
        //Credit: LoraZalora
        final List<String> lists = new ArrayList<>(Arrays.asList(list.split(",")));

        final TextView textView = Theme.text(getContext, featName + ":", Theme.ROW_SP, Theme.TEXT);

        final RadioGroup radioGroup = new RadioGroup(getContext);
        radioGroup.setPadding(dp(10), dp(6), dp(10), dp(6));
        radioGroup.setOrientation(LinearLayout.VERTICAL);
        radioGroup.setBackground(Theme.row(getContext));
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
                            + Theme.colorHex(Theme.ICON_BOX) + "'>" + radioName));
                    Preferences.changeFeatureInt(finalfeatName, featNum, radioGroup.indexOfChild(Radioo));
                }
            };
            Radioo.setText(lists.get(i));
            Radioo.setTextColor(Theme.TEXT);
            Radioo.setTextSize(Theme.ROW_SP);
            Radioo.setTypeface(Theme.font(getContext));
            Radioo.setButtonTintList(ColorStateList.valueOf(Theme.ICON_BOX));
            Radioo.setOnClickListener(first_radio_listener);
            installExplanation(Radioo, featNum, featName);
            radioGroup.addView(Radioo);
        }

        int index = Preferences.loadPrefInt(featName, featNum);
        if (index > 0 && index <= lists.size()) { //Preventing it to get an index less than 1. below 1 = null = crash
            textView.setText(Html.fromHtml(featName + ": <font color='" + Theme.colorHex(Theme.ICON_BOX)
                    + "'>" + lists.get(index - 1)));
            ((RadioButton) radioGroup.getChildAt(index)).setChecked(true);
        }
        installExplanation(radioGroup, featNum, featName);
        linLayout.addView(radioGroup);
    }

    private boolean readOpen(String key, boolean fallback) {
        try {
            return Preferences.with(getContext).readBoolean(key, fallback);
        } catch (Throwable t) {
            return fallback;
        }
    }

    private void saveOpen(String key, boolean opened) {
        try {
            Preferences.with(getContext).writeBoolean(key, opened);
        } catch (Throwable t) {
            Log.d(TAG, "Keadaan section gagal disimpan: " + t);
        }
    }

    /**
     * Section lipat. Buka-tutup dianimasikan dan keadaannya disimpan per judul
     * section, jadi urutan terbuka yang terakhir dipakai kembali saat menu dibuka lagi.
     */
    @SuppressLint("SetTextI18n")
    private void Collapse(LinearLayout linLayout, final String text, final boolean expanded) {
        final String key = KEY_FOLD + text;
        final boolean initialOpen = readOpen(key, expanded);

        LinearLayout section = new LinearLayout(getContext);
        section.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams sectionParams = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        sectionParams.setMargins(0, dp(10), 0, 0);
        section.setLayoutParams(sectionParams);

        LinearLayout kepala = new LinearLayout(getContext);
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);
        kepala.setBackground(Theme.sectionHeaderPress(getContext));
        kepala.setPadding(dp(14), 0, dp(10), 0);
        kepala.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, dp(Theme.TARGET_DP)));

        final TextView title = Theme.text(getContext, text, Theme.SECTION_SP, Theme.TITLE_TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        kepala.addView(title, titleParams);

        final Theme.Icon arrow = Theme.Icon.polos(getContext,
                initialOpen ? Theme.Icon.ARROW_DOWN : Theme.Icon.ARROW_RIGHT,
                Theme.TITLE_TEXT, 20);
        kepala.addView(arrow, new LinearLayout.LayoutParams(dp(20), dp(20)));

        final LinearLayout content = new LinearLayout(getContext);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(0, dp(4), 0, dp(2));
        content.setBackground(Theme.sectionBody(getContext));
        mCollapse = content;

        final LinearLayout pembungkus = new LinearLayout(getContext);
        pembungkus.setOrientation(LinearLayout.VERTICAL);
        pembungkus.addView(content);
        pembungkus.setVisibility(initialOpen ? View.VISIBLE : View.GONE);

        kepala.setContentDescription(text);
        kepala.setOnClickListener(new View.OnClickListener() {
            boolean opened = initialOpen;

            @Override
            public void onClick(View v) {
                opened = !opened;
                Transition.fold(pembungkus, opened);
                arrow.setJenis(opened ? Theme.Icon.ARROW_DOWN : Theme.Icon.ARROW_RIGHT);
                saveOpen(key, opened);
                // Isi yang baru terlihat langsung disegarkan, tanpa menunggu detak.
                if (opened)
                    jalankanPemantau();
            }
        });

        section.addView(kepala);
        section.addView(pembungkus);
        linLayout.addView(section);
    }

    private static final class CardPlayer {
        final String name;
        final String role;
        final LinearLayout view;

        CardPlayer(String name, String role, LinearLayout view) {
            this.name = name;
            this.role = role;
            this.view = view;
        }
    }

    /** Keadaan satu daftar pemain: kartu yang sudah ada dan isi terakhirnya. */
    private static final class ContentList {
        final Map<String, CardPlayer> card = new LinkedHashMap<String, CardPlayer>();
        String data;
        TextView kosong;
    }

    @SuppressLint("SetTextI18n")
    private void PlayerList(LinearLayout linLayout, final String featName) {
        LinearLayout container = new LinearLayout(getContext);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setPadding(dp(2), dp(2), dp(2), dp(4));

        LinearLayout titleRow = new LinearLayout(getContext);
        titleRow.setOrientation(LinearLayout.HORIZONTAL);
        titleRow.setGravity(Gravity.CENTER_VERTICAL);
        titleRow.setBackground(Theme.row(getContext));
        titleRow.setPadding(0, 0, dp(5), 0);
        titleRow.setMinimumHeight(dp(Theme.TARGET_DP));
        LinearLayout.LayoutParams paramsTitle = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsTitle.setMargins(0, dp(4), 0, dp(4));
        titleRow.setLayoutParams(paramsTitle);

        titleRow.addView(new FeatureIcon(getContext, IconPattern.PLAYER, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP), MATCH_PARENT));

        // Nama fitur dengan jumlah pemain di bawahnya, jadi tombol muat ulang
        // di kanan punya ruang untuk labelnya.
        LinearLayout columnTitle = new LinearLayout(getContext);
        columnTitle.setOrientation(LinearLayout.VERTICAL);
        columnTitle.setGravity(Gravity.CENTER_VERTICAL);
        columnTitle.setPadding(0, dp(4), 0, dp(4));
        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        title.setSingleLine(true);
        title.setEllipsize(TextUtils.TruncateAt.END);
        columnTitle.addView(title, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        final TextView count = Theme.text(getContext, "", Theme.DESC_SP, Theme.MUTED);
        count.setSingleLine(true);
        columnTitle.addView(count, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        LinearLayout.LayoutParams paramsColumn = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        paramsColumn.setMargins(dp(12), 0, dp(8), 0);
        titleRow.addView(columnTitle, paramsColumn);
        container.addView(titleRow);

        TextView petunjuk = Theme.text(getContext,
                "Ketuk kartu untuk memilih. Ketuk Kick untuk sementara, tahan untuk permanen. "
                        + "Kick host ada di kartu host.",
                Theme.DESC_SP, Theme.MUTED);
        petunjuk.setPadding(dp(4), dp(2), dp(4), dp(6));
        container.addView(petunjuk);

        final LinearLayout list = new LinearLayout(getContext);
        list.setOrientation(LinearLayout.VERTICAL);
        container.addView(list);

        // Daftar sudah dibaca ulang tiap detik. Tombol ini membaca seketika dan
        // memberi tanda jelas (memuat, lalu "Diperbarui") bahwa daftarnya baru.
        Button kickAll = smallButton("Kick semua", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                konfirmasi("Kick semua", "Tendang semua tamu secara permanen? Host dan kamu "
                                + "tidak ikut.", "Kick semua", new Runnable() {
                    @Override
                    public void run() {
                        boolean diterima;
                        try {
                            diterima = KickAll();
                        } catch (Throwable t) {
                            diterima = false;
                        }
                        if (!diterima)
                            Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                    Toast.LENGTH_SHORT).show();
                    }
                });
            }
        }, false);
        kickAll.setContentDescription("Kick semua tamu");
        titleRow.addView(kickAll);

        ReloadButton reload = new ReloadButton(getContext, IconPattern.RELOAD, new Runnable() {
            @Override
            public void run() {
                ContentList content = (ContentList) list.getTag();
                if (content != null)
                    content.data = null;
                fillPlayerList(list, count);
            }
        });
        titleRow.addView(reload, new LinearLayout.LayoutParams(WRAP_CONTENT, dp(Theme.TARGET_DP)));
        installExplanation(titleRow, 0, featName);

        linLayout.addView(container);
        fillPlayerList(list, count);

        // Daftar ikut detak Menu: dibaca ulang tiap detik hanya saat section ini
        // benar-benar terlihat, dan langsung saat menu atau section dibuka.
        // Perubahan (masuk, keluar, ganti nama/peran) tampil tanpa tombol muat ulang.
        daftarkanPemantau(new Runnable() {
            @Override
            public void run() {
                if (list.isShown())
                    fillPlayerList(list, count);
            }
        });
    }

    @SuppressLint("SetTextI18n")
    private void fillPlayerList(LinearLayout list, TextView count) {
        String data = getPlayerList();

        ContentList content = (ContentList) list.getTag();
        if (content == null) {
            content = new ContentList();
            list.removeAllViews();
            list.setTag(content);
        }
        if (data.equals(content.data))
            return;
        content.data = data;

        Map<String, String[]> fresh = new LinkedHashMap<String, String[]>();
        for (String row : data.split("\n")) {
            String[] part = row.split("\t", 3);
            if (part.length < 3 || part[0].length() == 0 || part[1].length() == 0)
                continue;
            fresh.put(part[1], new String[]{part[0], part[2]});
        }

        // Pemain yang keluar dibuang lebih dulu. Bila yang pergi adalah pilihan
        // sekarang, pilihannya dilepas supaya tidak menunjuk kunci yang hilang.
        Iterator<Map.Entry<String, CardPlayer>> it = content.card.entrySet().iterator();
        while (it.hasNext()) {
            Map.Entry<String, CardPlayer> entry = it.next();
            if (!fresh.containsKey(entry.getKey())) {
                list.removeView(entry.getValue().view);
                if (entry.getKey().equals(keySelected))
                    keySelected = null;
                it.remove();
            }
        }

        if (content.kosong != null) {
            list.removeView(content.kosong);
            content.kosong = null;
        }

        int indeks = 0;
        for (Map.Entry<String, String[]> entry : fresh.entrySet()) {
            String key = entry.getKey();
            String name = entry.getValue()[0];
            String role = entry.getValue()[1];
            CardPlayer card = content.card.get(key);
            if (card != null && card.name.equals(name) && card.role.equals(role)) {
                // Isi sama: kartunya dipakai lagi, hanya urutannya yang dirapikan.
                if (list.getChildAt(indeks) != card.view) {
                    list.removeView(card.view);
                    list.addView(card.view, Math.min(indeks, list.getChildCount()));
                }
            } else {
                LinearLayout view = playerRow(content, key, name, role);
                if (card != null)
                    list.removeView(card.view);
                list.addView(view, Math.min(indeks, list.getChildCount()));
                content.card.put(key, new CardPlayer(name, role, view));
            }
            indeks++;
        }

        count.setText(indeks == 0 ? "" : indeks + " pemain");
        if (indeks == 0) {
            TextView kosong = Theme.text(getContext, "Tidak ada pemain di room ini",
                    Theme.DESC_SP, Theme.MUTED);
            kosong.setGravity(Gravity.CENTER);
            kosong.setPadding(0, dp(14), 0, dp(14));
            list.addView(kosong);
            content.kosong = kosong;
        }
    }

    private String getPlayerList() {
        try {
            String data = PlayerList();
            return data == null ? "" : data;
        } catch (Throwable t) {
            return "";
        }
    }

    // Satu pemain menjadi satu kartu: nama dan lencana perannya di atas, aksi di
    // bawah. Kartu sendiri tidak punya aksi karena menyalin atau menendang diri
    // sendiri tidak berguna, dan tendang hanya ada untuk tamu.
    private LinearLayout playerRow(final ContentList content, final String key,
                                     final String name, String role) {
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(dp(10), dp(8), dp(10), dp(8));
        LinearLayout.LayoutParams paramsRow =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsRow.setMargins(0, 0, 0, dp(6));
        row.setLayoutParams(paramsRow);
        row.setBackground(key.equals(keySelected)
                ? Theme.cardSelected(getContext)
                : Theme.card(getContext));

        LinearLayout kepala = new LinearLayout(getContext);
        kepala.setOrientation(LinearLayout.HORIZONTAL);
        kepala.setGravity(Gravity.CENTER_VERTICAL);

        TextView label = Theme.text(getContext, name, Theme.ROW_SP, Theme.TEXT);
        label.setSingleLine(true);
        label.setEllipsize(TextUtils.TruncateAt.END);
        label.setLayoutParams(new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f));
        kepala.addView(label);
        kepala.addView(badgeRole(role));
        row.addView(kepala);

        row.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                keySelected = key.equals(keySelected) ? null : key;
                refreshChoice(content);
            }
        });

        if ("kamu".equals(role))
            return row;

        LinearLayout action = new LinearLayout(getContext);
        action.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams paramsAction =
                new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        paramsAction.setMargins(0, dp(6), 0, 0);
        action.setLayoutParams(paramsAction);
        action.addView(smallButton("Salin skin", new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                boolean diterima;
                try {
                    diterima = CopyPlayerSkin(key, name);
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
        if ("host".equals(role)) {
            action.addView(smallButton("Kick host", new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    konfirmasiKickHost(name);
                }
            }));
        } else if ("tamu".equals(role)) {
            Button kick = smallButton("Kick", new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    boolean diterima;
                    try {
                        diterima = KickPlayer(key, name, false);
                    } catch (Throwable t) {
                        diterima = false;
                    }
                    if (!diterima)
                        Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                Toast.LENGTH_SHORT).show();
                }
            });
            kick.setContentDescription("Kick sementara " + name);
            kick.setOnLongClickListener(new View.OnLongClickListener() {
                @Override
                public boolean onLongClick(View v) {
                    boolean diterima;
                    try {
                        diterima = KickPlayer(key, name, true);
                    } catch (Throwable t) {
                        diterima = false;
                    }
                    Toast.makeText(Menu.this.getContext, diterima
                                    ? "Kick permanen diantrekan"
                                    : "Antrean penuh, coba lagi",
                            Toast.LENGTH_SHORT).show();
                    return true;
                }
            });
            action.addView(kick);
        }
        row.addView(action);
        return row;
    }

    private void konfirmasiKickHost(final String name) {
        konfirmasi("Kick host", "Paket kick akan dikirim berulang ke host. Host bisa lag lalu "
                + "keluar dari room, dan room akan ditutup. Lanjutkan?", "Kick host",
                new Runnable() {
                    @Override
                    public void run() {
                        boolean diterima;
                        try {
                            diterima = KickHost(name);
                        } catch (Throwable t) {
                            diterima = false;
                        }
                        if (!diterima)
                            Toast.makeText(Menu.this.getContext, "Antrean penuh, coba lagi",
                                    Toast.LENGTH_SHORT).show();
                    }
                });
    }

    private void konfirmasi(String title, String message, String label, final Runnable action) {
        menuDialog.display(title, message, null, null,
                new MenuDialog.Button(label, action), null,
                new MenuDialog.Button("Batal", null), true);
    }

    private void refreshChoice(ContentList content) {
        for (Map.Entry<String, CardPlayer> entry : content.card.entrySet()) {
            entry.getValue().view.setBackground(entry.getKey().equals(keySelected)
                    ? Theme.cardSelected(getContext)
                    : Theme.card(getContext));
        }
    }

    private Button smallButton(String text, View.OnClickListener pendengar) {
        return smallButton(text, pendengar, true);
    }

    private Button smallButton(String text, View.OnClickListener pendengar, boolean weight) {
        Button button = new Button(getContext);
        button.setText(text);
        button.setTextSize(13f);
        button.setAllCaps(false);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTypeface(Theme.font(getContext));
        // Tinggi sentuh 48dp, kotak yang terlihat tetap sekecil sebelumnya.
        button.setBackground(Theme.smallButton(getContext, BUTTON_SMALL_INSET_DP));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(dp(Theme.TARGET_DP));
        button.setMinimumHeight(dp(Theme.TARGET_DP));
        button.setPadding(dp(8), 0, dp(8), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        LinearLayout.LayoutParams params = weight
                ? new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f)
                : new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(0, 0, dp(6), 0);
        button.setLayoutParams(params);
        button.setOnClickListener(pendengar);
        return button;
    }

    @SuppressLint("SetTextI18n")
    private TextView badgeRole(String role) {
        int color;
        if ("host".equals(role))
            color = Theme.ACCENT;
        else if ("kamu".equals(role))
            color = Theme.ICON_BOX;
        else
            color = Theme.TRACK;

        int textColor = color == Theme.ACCENT ? Theme.TEXT : Theme.ACTIVE_BORDER;
        TextView badge = Theme.text(getContext, role, 12f, textColor);
        badge.setGravity(Gravity.CENTER);
        badge.setPadding(dp(6), dp(2), dp(6), dp(2));
        badge.setBackground(Theme.badge(getContext, color));
        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(WRAP_CONTENT, WRAP_CONTENT);
        params.setMargins(dp(6), 0, 0, 0);
        badge.setLayoutParams(params);
        return badge;
    }

    private void Category(LinearLayout linLayout, String text) {
        if (linLayout == mSettings) {
            // Setelan memakai judul kelompok yang ringan, bukan pita cokelat.
            TextView title = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
            title.setPadding(dp(4), dp(12), dp(4), dp(2));
            linLayout.addView(title);
            return;
        }
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackground(Theme.sectionHeader(getContext));
        row.setPadding(dp(12), dp(10), dp(12), dp(10));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(10), 0, dp(2));
        row.setLayoutParams(params);

        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.SECTION_SP, Theme.TITLE_TEXT);
        textView.setGravity(Gravity.CENTER);
        textView.setLayoutParams(new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        row.addView(textView);
        linLayout.addView(row);
    }

    private void TextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.MUTED);
        textView.setPadding(dp(10), dp(4), dp(10), dp(2));
        linLayout.addView(textView);
    }

    /**
     * RichWebView tidak lagi memakai WebView. Tag dasar (b, i, u, font, a, br)
     * dirender TextView biasa, dan tautan tetap bisa diketuk.
     */
    private void WebTextView(LinearLayout linLayout, String text) {
        TextView textView = Theme.text(getContext, Html.fromHtml(text), Theme.DESC_SP, Theme.TEXT);
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
    private LinearLayout row(String[] pattern, View content, View control) {
        LinearLayout row = new LinearLayout(getContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackground(Theme.row(getContext));
        row.setMinimumHeight(dp(Theme.TARGET_DP));
        row.setPadding(0, 0, dp(8), 0);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT);
        params.setMargins(0, dp(4), 0, dp(4));
        row.setLayoutParams(params);

        // Kotak ikon menempel penuh di tepi kiri dan setinggi baris, tanpa margin.
        row.addView(new FeatureIcon(getContext, pattern, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP), MATCH_PARENT));

        LinearLayout.LayoutParams fillParams = new LinearLayout.LayoutParams(0, WRAP_CONTENT, 1f);
        fillParams.setMargins(dp(12), dp(6), dp(8), dp(6));
        row.addView(content, fillParams);

        if (control != null)
            row.addView(control, new LinearLayout.LayoutParams(dp(52), dp(Theme.TARGET_DP)));
        return row;
    }

    /**
     * Baris posisi pemain: koordinat tile dan dunia dibaca ulang tiap detik
     * selama barisnya terlihat.
     */
    private void RowPosition(LinearLayout linLayout, String featName) {
        LinearLayout column = new LinearLayout(getContext);
        column.setOrientation(LinearLayout.VERTICAL);

        TextView title = Theme.text(getContext, featName, Theme.ROW_SP, Theme.TEXT);
        final TextView value = Theme.text(getContext, "Menunggu posisi...", Theme.DESC_SP,
                Theme.ICON_BOX);
        column.addView(title);
        column.addView(value);

        final LinearLayout row = row(IconPattern.POSITION, column, null);
        installExplanation(row, 0, featName);
        linLayout.addView(row);

        daftarkanPemantau(new Runnable() {
            @Override
            public void run() {
                if (row.isShown())
                    value.setText(PositionText());
            }
        });
    }

    private String PositionText() {
        String data;
        try {
            data = PlayerPosition();
        } catch (Throwable t) {
            data = null;
        }
        if (data == null || data.length() == 0)
            return "Posisi belum terbaca";

        String[] part = data.split("\t", -1);
        if (part.length < 4)
            return "Posisi belum terbaca";
        if (part[0].length() > 0 && part[1].length() > 0)
            return "Tile UI " + part[0] + ", " + part[1] + "   Dunia " + part[2] + ", "
                    + part[3];
        return "Dunia " + part[2] + ", " + part[3];
    }

    private Button buttonText(CharSequence text) {
        Button button = new Button(getContext);
        button.setText(text);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(getContext));
        button.setAllCaps(false); //Disable caps to support html
        button.setGravity(Gravity.CENTER);
        button.setBackground(Theme.pressOverlay(getContext));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(0);
        button.setMinimumHeight(0);
        button.setPadding(dp(6), 0, dp(6), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        return button;
    }

    private LinearLayout boxButton(String[] pattern, View content) {
        LinearLayout box = new LinearLayout(getContext);
        box.setOrientation(LinearLayout.HORIZONTAL);
        box.setGravity(Gravity.CENTER_VERTICAL);
        box.setBackground(Theme.button(getContext));
        box.setPadding(dp(3), dp(3), dp(8), dp(3));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(MATCH_PARENT, dp(50));
        params.setMargins(0, dp(4), 0, dp(4));
        box.setLayoutParams(params);

        // Ikon menempel penuh di dalam garis tombol.
        box.addView(new FeatureIcon(getContext, pattern, true),
                new LinearLayout.LayoutParams(dp(FeatureIcon.WIDTH_DP - 3), MATCH_PARENT));

        LinearLayout.LayoutParams fillParams = new LinearLayout.LayoutParams(0, MATCH_PARENT, 1f);
        box.addView(content, fillParams);
        return box;
    }

    /**
     * Tekan lama pada baris fitur membuka dialog penjelasan. Listener dipasang
     * di baris dan di kontrol yang bisa menelan sentuhan (tombol, sakelar,
     * spinner), supaya tekan lama tetap bekerja di bagian mana pun.
     */
    private void installExplanation(View tampilan, final int featNum, final String featName) {
        if (tampilan == null)
            return;
        tampilan.setOnLongClickListener(new View.OnLongClickListener() {
            @Override
            public boolean onLongClick(View v) {
                showExplanation(featNum, featName);
                return true;
            }
        });
    }

    private void showExplanation(int featNum, String featName) {
        menuDialog.explanation(featNum, featName);
    }

    private boolean isViewCollapsed() {
        return rootFrame == null || mCollapsed.getVisibility() == View.VISIBLE;
    }

    private int dp(float value) {
        return (int) Theme.dp(getContext, value);
    }

    public void setVisibility(int view) {
        if (rootFrame != null) {
            rootFrame.setVisibility(view);
        }
    }

    public void onDestroy() {
        hancur = true;
        tickRun = false;
        sourceInstalled = null;
        handlerTick.removeCallbacksAndMessages(null);
        menuDialog.close();
        if (iconImage != null)
            releaseImage();
        if (rootFrame != null && mWindowManager != null) {
            try {
                mWindowManager.removeView(rootFrame);
            } catch (IllegalArgumentException e) {
                // Jendela memang belum atau sudah dilepas.
                Log.d(TAG, "removeView dilewati: " + e);
            }
        }
    }
}
