#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Hello KSP Launcher 版本号同步脚本。

用法：
    python sync_version.py <新版本号>      如  python sync_version.py 1.1.2

所有版本号均以 1.1.1 之类 X.Y.Z 形式给出。脚本会同步三处：
  1. CMakeLists.txt           project(HelloKSPLauncher VERSION x.y.z ...)
  2. src/appversion.h         #define HKSPL_APP_VERSION "x.y.z"
  3. README/功能更新.md        最新一条 "## 日期：标题" 记录补上 v 版本号

脚本把 功能更新.md 当作变更记录参照：每次发版升号时，把当前(最新)一条更新
记录统一标注上本次版本号，与记忆库中沉淀的功能更新版本号保持一致。
兼容 Windows / macOS / Linux，Python 3 直接运行。
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
CMAKE = os.path.join(ROOT, "CMakeLists.txt")
APPVER = os.path.join(ROOT, "src", "appversion.h")
CHANGELOG = os.path.join(ROOT, "README", "功能更新.md")

SEMVER = re.compile(r"^\d+\.\d+\.\d+$")


def _already_synced(message, ver):
    print("[sync] {} 已为目标版本 {}".format(message, ver))


def sync_cmake(ver):
    path = CMAKE
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    new = re.sub(
        r"(project\(HelloKSPLauncher\s+VERSION\s+)\d+\.\d+\.\d+",
        r"\g<1>" + ver,
        text,
        count=1,
    )
    if new == text:
        # 可能是目标版本恰为当前值（未产生改动），也可能是未匹配到
        if re.search(r"project\(HelloKSPLauncher\s+VERSION\s+" + re.escape(ver), text):
            _already_synced("CMakeLists.txt VERSION", ver)
            return
        raise RuntimeError("CMakeLists.txt 中未找到 project(HelloKSPLauncher VERSION x.y.z)")
    with open(path, "w", encoding="utf-8") as f:
        f.write(new)
    print("[sync] CMakeLists.txt VERSION -> {}".format(ver))


def sync_appversion(ver):
    path = APPVER
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    new = re.sub(
        r"(#define\s+HKSPL_APP_VERSION\s+\")[^\"]*(\")",
        r"\g<1>" + ver + r"\g<2>",
        text,
        count=1,
    )
    if new == text:
        if re.search(r"#define\s+HKSPL_APP_VERSION\s+\"" + re.escape(ver) + r"\"", text):
            _already_synced("src/appversion.h HKSPL_APP_VERSION", ver)
            return
        raise RuntimeError("src/appversion.h 中未找到 HKSPL_APP_VERSION 宏")
    with open(path, "w", encoding="utf-8") as f:
        f.write(new)
    print("[sync] src/appversion.h HKSPL_APP_VERSION -> {}".format(ver))


def sync_changelog(ver):
    path = CHANGELOG
    with open(path, "r", encoding="utf-8") as f:
        lines = f.read().splitlines(keepends=True)

    # 匹配最新(第一条)更新记录标题，形如 "## 2026-08-29：xxx" 或已带版本 "## ... v1.1.1：xxx"
    header = re.compile(
        r"^(## \d{4}-\d{2}-\d{2})\s*(?:v\d+\.\d+\.\d+)?\s*([：:])(.*)$"
    )
    done = False
    for i, line in enumerate(lines):
        m = header.match(line)
        if not m:
            continue
        prefix, colon, rest = m.group(1), m.group(2), m.group(3)
        # 已是目标版本则无需改动
        if line.startswith(prefix + " v" + ver + colon):
            print("[sync] 功能更新.md 最新记录已标注 v{}".format(ver))
            return
        lines[i] = "{prefix} v{ver}{colon}{rest}".format(
            prefix=prefix, ver=ver, colon=colon, rest=rest
        )
        done = True
        break

    if not done:
        raise RuntimeError("功能更新.md 中未找到任何 '## 日期：标题' 记录")
    with open(path, "w", encoding="utf-8") as f:
        f.writelines(lines)
    print("[sync] 功能更新.md 最新记录 -> v{}".format(ver))


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("用法：python sync_version.py <X.Y.Z>\n")
        sys.exit(2)
    ver = sys.argv[1]
    if not SEMVER.match(ver):
        sys.stderr.write("错误：版本号必须为 X.Y.Z 形式，收到：{}\n".format(ver))
        sys.exit(2)
    sync_cmake(ver)
    sync_appversion(ver)
    sync_changelog(ver)
    print("[sync] 完成：版本号已同步为 {}".format(ver))


if __name__ == "__main__":
    main()