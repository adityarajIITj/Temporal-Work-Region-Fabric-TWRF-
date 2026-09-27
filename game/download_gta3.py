#!/usr/bin/env python3
"""Downloader and unpacker for authentic Grand Theft Auto III PC release."""

from __future__ import annotations

import os
import sys
import time
import zipfile
import urllib.request

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

URL = "https://archive.org/download/grand-theft-auto-iii_202103/Grand%20Theft%20Auto%20III.zip"
TARGET_DIR = os.path.dirname(os.path.abspath(__file__))
ZIP_PATH = os.path.join(TARGET_DIR, "gta3_archive.zip")


def download_file(url: str, dest_path: str) -> None:
    print(f"[GTA3 DOWNLOAD] Fetching archive from: {url}")
    print(f"[GTA3 DOWNLOAD] Destination: {dest_path}")

    headers = {"User-Agent": "Mozilla/5.0"}
    existing_bytes = 0
    if os.path.exists(dest_path):
        existing_bytes = os.path.getsize(dest_path)
        print(f"[GTA3 DOWNLOAD] Existing partial download found: {existing_bytes / 1024 / 1024:.1f} MB")

    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req) as resp:
            total_bytes = int(resp.headers.get("Content-Length", 0))
    except Exception as e:
        print(f"[ERROR] Failed to query remote file size: {e}")
        return

    print(f"[GTA3 DOWNLOAD] Total file size: {total_bytes / 1024 / 1024:.2f} MB")

    if existing_bytes == total_bytes and total_bytes > 0:
        print("[GTA3 DOWNLOAD] Archive is already completely downloaded.")
        return

    req = urllib.request.Request(url, headers=headers)
    if existing_bytes > 0:
        req.headers["Range"] = f"bytes={existing_bytes}-"
        mode = "ab"
    else:
        mode = "wb"

    chunk_size = 1024 * 1024  # 1 MB
    downloaded = existing_bytes
    start_time = time.time()
    last_print = start_time

    with urllib.request.urlopen(req) as resp, open(dest_path, mode) as out:
        while True:
            chunk = resp.read(chunk_size)
            if not chunk:
                break
            out.write(chunk)
            downloaded += len(chunk)
            now = time.time()
            if now - last_print >= 5.0 or downloaded == total_bytes:
                elapsed = now - start_time
                speed_mb = ((downloaded - existing_bytes) / 1024 / 1024) / max(0.1, elapsed)
                pct = (downloaded / total_bytes) * 100.0 if total_bytes > 0 else 0.0
                print(f"[PROGRESS] {downloaded / 1024 / 1024:.1f} / {total_bytes / 1024 / 1024:.1f} MB ({pct:.1f}%) | Speed: {speed_mb:.2f} MB/s", flush=True)
                last_print = now

    print("[SUCCESS] Archive download completed successfully.")


def extract_archive(zip_path: str, extract_to: str) -> bool:
    print(f"[UNPACK] Extracting {zip_path} to {extract_to}...")
    try:
        with zipfile.ZipFile(zip_path, "r") as zf:
            total_members = len(zf.namelist())
            for idx, member in enumerate(zf.namelist()):
                zf.extract(member, extract_to)
                if idx % 100 == 0 or idx == total_members - 1:
                    print(f"[UNPACK] Extracted {idx + 1} / {total_members} files...", flush=True)
        print("[SUCCESS] All GTA 3 files extracted successfully.")
        return True
    except Exception as e:
        print(f"[ERROR] Extraction failed: {e}")
        return False


def verify_installation(game_dir: str) -> bool:
    found_exe = False
    found_models = False
    for root, dirs, files in os.walk(game_dir):
        for f in files:
            if f.lower() == "gta3.exe":
                found_exe = True
            if f.lower() == "gta3.img":
                found_models = True

    print(f"[VERIFY] gta3.exe found: {found_exe}")
    print(f"[VERIFY] models/gta3.img found: {found_models}")
    return found_exe and found_models


def main() -> None:
    download_file(URL, ZIP_PATH)
    if os.path.exists(ZIP_PATH):
        extract_archive(ZIP_PATH, TARGET_DIR)
        ok = verify_installation(TARGET_DIR)
        if ok:
            print("[READY] Authentic Rockstar Grand Theft Auto III is ready for TWRF Virtual GPU execution.")
        else:
            print("[WARN] Some files were not verified. Check folder contents.")


if __name__ == "__main__":
    main()
