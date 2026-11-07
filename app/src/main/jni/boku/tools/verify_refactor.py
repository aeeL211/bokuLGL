#!/usr/bin/env python3
"""Membandingkan hasil refaktor dengan baseline di docs/baseline/.

Dijalankan lokal tanpa NDK: skrip ini membaca sumber C++ dan memastikan daftar
fitur, tabel RegisterNatives, dan urutan hook tidak berubah. Kompilasi tetap
dilakukan di CI.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def cari_root(mulai: Path) -> Path:
    for kandidat in [mulai, *mulai.parents]:
        if (kandidat / "docs" / "baseline" / "baseline_hooks.txt").is_file():
            return kandidat
    raise SystemExit("docs/baseline tidak ditemukan")


def baca_baseline_daftar(path: Path) -> tuple[list[str], list[str]]:
    teks = path.read_text(encoding="utf-8")
    bagian = teks.split("=== ")
    fitur: list[str] = []
    setelan: list[str] = []
    for blok in bagian[1:]:
        judul, _, isi = blok.partition("===\n")
        baris = [b for b in isi.splitlines() if b.strip()]
        if judul.startswith("GetFeatureList"):
            fitur = baris
        elif judul.startswith("SettingsList"):
            setelan = baris
    if not fitur or not setelan:
        raise SystemExit("baseline daftar tidak lengkap")
    return fitur, setelan


def daftar_sumber(path: Path, fungsi: str) -> list[str]:
    teks = path.read_text(encoding="utf-8")
    awal = teks.index(f"jobjectArray {fungsi}(")
    badan = teks[awal:]
    akhir = badan.index("};")
    return re.findall(r'OBFUSCATE\("([^"]*)"\)', badan[:akhir])


def tabel_jni(path: Path) -> dict[str, list[tuple[str, str]]]:
    teks = path.read_text(encoding="utf-8")
    hasil: dict[str, list[tuple[str, str]]] = {}
    for kelas in ("Menu", "Preferences", "Main"):
        awal = teks.index(f"int Register{kelas}(")
        badan = teks[awal:]
        akhir = badan.index("return daftarkan")
        nama_kelas = re.search(r'daftarkan\(env, OBFUSCATE\("([^"]+)"\)', badan[akhir:])
        if not nama_kelas:
            raise SystemExit(f"kelas {kelas} tidak ditemukan")
        blok = badan[:akhir]
        pasangan: list[tuple[str, str]] = []
        for metode in re.finditer(
                r'\{OBFUSCATE\("([^"]+)"\),\s*OBFUSCATE\(((?:\s*"[^"]*")+)\)',
                blok):
            tanda = "".join(re.findall(r'"([^"]*)"', metode.group(2)))
            pasangan.append((metode.group(1), tanda))
        hasil[nama_kelas.group(1).replace("/", ".")] = sorted(pasangan)
    return hasil


def baseline_jni(path: Path) -> dict[str, list[tuple[str, str]]]:
    hasil: dict[str, list[tuple[str, str]]] = {}
    kelas: str | None = None
    for baris in path.read_text(encoding="utf-8").splitlines():
        judul = re.match(r"\s+(com\.android\.support\.\w+) \(", baris)
        if judul:
            kelas = judul.group(1)
            hasil[kelas] = []
            continue
        isi = re.match(r"\s+(\w+)\s+(\S.*)$", baris)
        if kelas and isi and isi.group(2).startswith("("):
            hasil[kelas].append((isi.group(1), isi.group(2).strip()))
    return {kelas: sorted(metode) for kelas, metode in hasil.items()}


def badan_fungsi(teks: str, nama: str) -> str:
    pola = re.compile(r"void\s+" + re.escape(nama) + r"\s*\([^)]*\)\s*\{")
    cocok = pola.search(teks)
    if not cocok:
        raise SystemExit(f"fungsi {nama} tidak ditemukan")
    mulai = cocok.end()
    kedalaman = 1
    for i in range(mulai, len(teks)):
        if teks[i] == "{":
            kedalaman += 1
        elif teks[i] == "}":
            kedalaman -= 1
            if kedalaman == 0:
                return teks[mulai:i]
    raise SystemExit(f"badan {nama} tidak tertutup")


POLA_PASANG = re.compile(
    r"pasangHook\(target\((T_[A-Z0-9_]+)\)"
    r"|pasangAkhirMakan<(T_[A-Z0-9_]+)>\(\)"
    r"|(pasang[A-Z][A-Za-z0-9_]*)\(\)"
)


def kumpulkan_hook(nama: str, teks: str, terlihat: set[str]) -> list[str]:
    if nama in terlihat:
        raise SystemExit(f"rekursi pasang hook: {nama}")
    terlihat.add(nama)
    hasil: list[str] = []
    for cocok in POLA_PASANG.finditer(badan_fungsi(teks, nama)):
        langsung, makan, bantuan = cocok.groups()
        if langsung or makan:
            hasil.append(langsung or makan)
        elif bantuan:
            hasil.extend(kumpulkan_hook(bantuan, teks, terlihat))
    terlihat.remove(nama)
    return hasil


def urutan_hook(root: Path) -> list[str]:
    jni = root / "app" / "src" / "main" / "jni"
    main_teks = (jni / "boku" / "main.cpp").read_text(encoding="utf-8")
    panggilan = re.findall(r"^(?:\s{4})(pasangHook[A-Z][A-Za-z0-9_]*)\(\);",
                           main_teks, re.MULTILINE)
    if len(panggilan) != 21:
        raise SystemExit(f"jumlah pasangHookX di main.cpp {len(panggilan)}, bukan 21")
    hasil: list[str] = []
    for nama in panggilan:
        for berkas in sorted((jni / "boku" / "hooks").glob("*.cpp")):
            teks = berkas.read_text(encoding="utf-8")
            if re.search(r"void\s+" + re.escape(nama) + r"\s*\(", teks):
                hasil.extend(kumpulkan_hook(nama, teks, set()))
                break
        else:
            raise SystemExit(f"fungsi {nama} tidak ditemukan di hooks/")
    return hasil


def baseline_hook(path: Path) -> list[str]:
    hasil: list[str] = []
    for baris in path.read_text(encoding="utf-8").splitlines():
        cocok = re.match(r"\d{2}\|(T_[A-Z0-9_]+)\|", baris)
        if cocok:
            hasil.append(cocok.group(1))
    return hasil


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=None)
    args = parser.parse_args()
    root = args.root or cari_root(Path(__file__).resolve())
    jni = root / "app" / "src" / "main" / "jni"

    gagal = False

    fitur_baseline, setelan_baseline = baca_baseline_daftar(
        root / "docs" / "baseline" / "baseline_daftar.txt")
    fitur_sumber = daftar_sumber(jni / "Menu" / "FeatureList.cpp", "daftarFitur")
    setelan_sumber = daftar_sumber(jni / "Menu" / "FeatureList.cpp", "daftarSetelan")
    for nama, harap, dapat in (
        ("GetFeatureList", fitur_baseline, fitur_sumber),
        ("SettingsList", setelan_baseline, setelan_sumber),
    ):
        if harap == dapat:
            print(f"OK   daftar {nama}: {len(dapat)} entri identik")
        else:
            gagal = True
            print(f"FAIL daftar {nama}: baseline {len(harap)}, sumber {len(dapat)}")
            for i, (a, b) in enumerate(zip(harap, dapat)):
                if a != b:
                    print(f"     baris {i + 1}: {a!r} != {b!r}")
                    break

    jni_baseline = baseline_jni(root / "docs" / "baseline" / "baseline_jni_symbols.txt")
    jni_sumber = tabel_jni(jni / "Menu" / "Setup.cpp")
    if jni_baseline == jni_sumber:
        total = sum(len(v) for v in jni_sumber.values())
        print(f"OK   tabel JNI: {total} metode di {len(jni_sumber)} kelas identik")
    else:
        gagal = True
        print("FAIL tabel JNI berbeda")
        for kelas in sorted(set(jni_baseline) | set(jni_sumber)):
            if jni_baseline.get(kelas) != jni_sumber.get(kelas):
                print(f"     {kelas}: {jni_baseline.get(kelas)} != {jni_sumber.get(kelas)}")

    harap_hook = baseline_hook(root / "docs" / "baseline" / "baseline_hooks.txt")
    dapat_hook = urutan_hook(root)
    if harap_hook == dapat_hook:
        print(f"OK   urutan hook: {len(dapat_hook)} pemasangan runtime identik")
    else:
        gagal = True
        print(f"FAIL urutan hook: baseline {len(harap_hook)}, sumber {len(dapat_hook)}")
        for i, (a, b) in enumerate(zip(harap_hook, dapat_hook)):
            if a != b:
                print(f"     urutan {i + 1}: {a} != {b}")
                break

    return 1 if gagal else 0


if __name__ == "__main__":
    sys.exit(main())
