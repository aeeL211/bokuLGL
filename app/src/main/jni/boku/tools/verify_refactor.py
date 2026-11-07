#!/usr/bin/env python3
"""Compares the refactored source with the baseline in docs/baseline/.

Runs locally without the NDK: the script reads the C++ sources and checks that
the feature list, RegisterNatives table, and hook order did not change.
Compilation still happens in CI.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def find_root(start: Path) -> Path:
    for candidate in [start, *start.parents]:
        if (candidate / "docs" / "baseline" / "baseline_hooks.txt").is_file():
            return candidate
    raise SystemExit("docs/baseline not found")


def read_baseline_lists(path: Path) -> tuple[list[str], list[str]]:
    text = path.read_text(encoding="utf-8")
    parts = text.split("=== ")
    features: list[str] = []
    settings: list[str] = []
    for block in parts[1:]:
        title, _, body = block.partition("===\n")
        lines = [line for line in body.splitlines() if line.strip()]
        if title.startswith("GetFeatureList"):
            features = lines
        elif title.startswith("SettingsList"):
            settings = lines
    if not features or not settings:
        raise SystemExit("baseline list is incomplete")
    return features, settings


def source_list(path: Path, function: str) -> list[str]:
    text = path.read_text(encoding="utf-8")
    start = text.index(f"jobjectArray {function}(")
    body = text[start:]
    end = body.index("};")
    return re.findall(r'OBFUSCATE\("([^"]*)"\)', body[:end])


def jni_table(path: Path) -> dict[str, list[tuple[str, str]]]:
    text = path.read_text(encoding="utf-8")
    result: dict[str, list[tuple[str, str]]] = {}
    for klass in ("Menu", "Preferences", "Main"):
        start = text.index(f"int Register{klass}(")
        body = text[start:]
        end = body.index("return registerNative")
        class_name = re.search(r'registerNative\(env, OBFUSCATE\("([^"]+)"\)', body[end:])
        if not class_name:
            raise SystemExit(f"class {klass} not found")
        block = body[:end]
        pairs: list[tuple[str, str]] = []
        for method in re.finditer(
                r'\{OBFUSCATE\("([^"]+)"\),\s*OBFUSCATE\(((?:\s*"[^"]*")+)\)',
                block):
            signature = "".join(re.findall(r'"([^"]*)"', method.group(2)))
            pairs.append((method.group(1), signature))
        result[class_name.group(1).replace("/", ".")] = sorted(pairs)
    return result


def baseline_jni(path: Path) -> dict[str, list[tuple[str, str]]]:
    result: dict[str, list[tuple[str, str]]] = {}
    current: str | None = None
    for line in path.read_text(encoding="utf-8").splitlines():
        title = re.match(r"\s+(com\.android\.support\.\w+) \(", line)
        if title:
            current = title.group(1)
            result[current] = []
            continue
        entry = re.match(r"\s+(\w+)\s+(\S.*)$", line)
        if current and entry and entry.group(2).startswith("("):
            result[current].append((entry.group(1), entry.group(2).strip()))
    return {klass: sorted(method) for klass, method in result.items()}


def function_body(text: str, name: str) -> str:
    pattern = re.compile(r"void\s+" + re.escape(name) + r"\s*\([^)]*\)\s*\{")
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"function {name} not found")
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i]
    raise SystemExit(f"body {name} is not closed")


INSTALL_PATTERN = re.compile(
    r"installHook\(target\((T_[A-Z0-9_]+)\)"
    r"|installEndEat<(T_[A-Z0-9_]+)>\(\)"
    r"|(install[A-Z][A-Za-z0-9_]*)\(\)"
)


def collect_hooks(name: str, text: str, seen: set[str]) -> list[str]:
    if name in seen:
        raise SystemExit(f"recursive hook install: {name}")
    seen.add(name)
    result: list[str] = []
    for match in INSTALL_PATTERN.finditer(function_body(text, name)):
        direct, end_eat, helper = match.groups()
        if direct or end_eat:
            result.append(direct or end_eat)
        elif helper:
            result.extend(collect_hooks(helper, text, seen))
    seen.remove(name)
    return result


def hook_order(root: Path) -> list[str]:
    jni = root / "app" / "src" / "main" / "jni"
    main_text = (jni / "boku" / "main.cpp").read_text(encoding="utf-8")
    calls = re.findall(r"^(?:\s{4})(install[A-Z][A-Za-z0-9_]*)\(\);",
                       main_text, re.MULTILINE)
    if len(calls) != 21:
        raise SystemExit(f"installHookX count in main.cpp is {len(calls)}, not 21")
    result: list[str] = []
    for name in calls:
        for file in sorted((jni / "boku" / "hooks").glob("*.cpp")):
            text = file.read_text(encoding="utf-8")
            if re.search(r"void\s+" + re.escape(name) + r"\s*\(", text):
                result.extend(collect_hooks(name, text, set()))
                break
        else:
            raise SystemExit(f"function {name} not found in hooks/")
    return result


def baseline_hooks(path: Path) -> list[str]:
    result: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"\d{2}\|(T_[A-Z0-9_]+)\|", line)
        if match:
            result.append(match.group(1))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=None)
    args = parser.parse_args()
    root = args.root or find_root(Path(__file__).resolve())
    jni = root / "app" / "src" / "main" / "jni"

    failed = False

    feature_baseline, settings_baseline = read_baseline_lists(
        root / "docs" / "baseline" / "baseline_daftar.txt")
    feature_source = source_list(jni / "Menu" / "FeatureList.cpp", "featureList")
    settings_source = source_list(jni / "Menu" / "FeatureList.cpp", "settingList")
    for name, expected, actual in (
        ("GetFeatureList", feature_baseline, feature_source),
        ("SettingsList", settings_baseline, settings_source),
    ):
        if expected == actual:
            print(f"OK   list {name}: {len(actual)} identical entries")
        else:
            failed = True
            print(f"FAIL list {name}: baseline {len(expected)}, source {len(actual)}")
            for i, (a, b) in enumerate(zip(expected, actual)):
                if a != b:
                    print(f"     line {i + 1}: {a!r} != {b!r}")
                    break

    jni_baseline = baseline_jni(root / "docs" / "baseline" / "baseline_jni_symbols.txt")
    jni_source = jni_table(jni / "Menu" / "Setup.cpp")
    if jni_baseline == jni_source:
        total = sum(len(v) for v in jni_source.values())
        print(f"OK   JNI table: {total} methods in {len(jni_source)} identical classes")
    else:
        failed = True
        print("FAIL JNI table differs")
        for klass in sorted(set(jni_baseline) | set(jni_source)):
            if jni_baseline.get(klass) != jni_source.get(klass):
                print(f"     {klass}: {jni_baseline.get(klass)} != {jni_source.get(klass)}")

    expected_hooks = baseline_hooks(root / "docs" / "baseline" / "baseline_hooks.txt")
    actual_hooks = hook_order(root)
    if expected_hooks == actual_hooks:
        print(f"OK   hook order: {len(actual_hooks)} identical runtime installs")
    else:
        failed = True
        print(f"FAIL hook order: baseline {len(expected_hooks)}, source {len(actual_hooks)}")
        for i, (a, b) in enumerate(zip(expected_hooks, actual_hooks)):
            if a != b:
                print(f"     order {i + 1}: {a} != {b}")
                break

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
