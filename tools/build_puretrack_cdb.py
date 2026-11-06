#!/usr/bin/env python3
"""Build puretrack.cdb (aircraft labels for SkyView) from the PureTrack labels feed.

    python tools/build_puretrack_cdb.py [-o data/puretrack.cdb] [--json labels.json]

Fetches https://puretrack.io/api/labels.json (or reads --json) and writes a CDB file the
firmware's uCDB reader opens from SPIFFS as /puretrack.cdb:

    key   = 6-character uppercase hex address, as ESP32_DB_query() formats it ("%06X")
    value = label (UTF-8, at most 63 bytes, the size of the firmware's read buffer)

Entries with a hex address that is not 1-6 hex digits, or with an empty label, are skipped;
for duplicate addresses the first entry wins. Records are written in key order, so the same
input always gives a byte-identical file (the workflow only commits when the data changed).

Standard library only, so it runs on a bare GitHub Actions runner.
"""
import argparse
import json
import re
import struct
import sys
import urllib.request

URL = "https://puretrack.io/api/labels.json"
MAX_LABEL_BYTES = 63
HEX_RE = re.compile(r"[0-9A-F]{1,6}")


def djb_hash(data: bytes) -> int:
    h = 5381
    for b in data:
        h = (((h << 5) + h) ^ b) & 0xFFFFFFFF
    return h


def write_cdb(path, records):
    """records: list of (key bytes, value bytes). Standard cdb layout, little-endian."""
    tables = [[] for _ in range(256)]
    body = bytearray()
    pos = 2048
    for key, value in records:
        h = djb_hash(key)
        tables[h & 255].append((h, pos))
        rec = struct.pack("<II", len(key), len(value)) + key + value
        body += rec
        pos += len(rec)

    header = bytearray()
    hash_area = bytearray()
    for entries in tables:
        n = len(entries) * 2
        header += struct.pack("<II", pos, n)
        slots = [(0, 0)] * n
        for h, rpos in entries:
            i = (h >> 8) % n
            while slots[i][1]:
                i = (i + 1) % n
            slots[i] = (h, rpos)
        for h, rpos in slots:
            hash_area += struct.pack("<II", h, rpos)
        pos += n * 8

    with open(path, "wb") as f:
        f.write(header)
        f.write(body)
        f.write(hash_area)


def cdb_get(blob: bytes, key: bytes):
    """Minimal cdb lookup, used to check the written file."""
    h = djb_hash(key)
    tpos, n = struct.unpack_from("<II", blob, (h & 255) * 8)
    if not n:
        return None
    i = (h >> 8) % n
    for _ in range(n):
        sh, rpos = struct.unpack_from("<II", blob, tpos + i * 8)
        if not rpos:
            return None
        if sh == h:
            klen, vlen = struct.unpack_from("<II", blob, rpos)
            if blob[rpos + 8:rpos + 8 + klen] == key:
                return blob[rpos + 8 + klen:rpos + 8 + klen + vlen]
        i = (i + 1) % n
    return None


def truncate_utf8(s: str, limit: int) -> bytes:
    b = s.encode("utf-8")
    if len(b) <= limit:
        return b
    return b[:limit].decode("utf-8", "ignore").encode("utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("-o", "--output", default="data/puretrack.cdb")
    ap.add_argument("--json", help="read labels from this file instead of fetching them")
    args = ap.parse_args()

    if args.json:
        with open(args.json, encoding="utf-8") as f:
            data = json.load(f)
    else:
        req = urllib.request.Request(URL, headers={"User-Agent": "SkyView-puretrack-cdb-builder"})
        with urllib.request.urlopen(req, timeout=60) as r:
            data = json.load(r)

    labels, invalid, empty, dupes = {}, 0, 0, 0
    for entry in data:
        hex_id = str(entry.get("hex") or "").strip().upper()
        label = " ".join(str(entry.get("label") or "").split())
        if not HEX_RE.fullmatch(hex_id):
            invalid += 1
            continue
        if not label:
            empty += 1
            continue
        key = hex_id.zfill(6)
        if key in labels:
            dupes += 1
            continue
        labels[key] = label

    if len(labels) < 100:
        sys.exit(f"Only {len(labels)} usable labels - refusing to write a near-empty database")

    records = [(k.encode("ascii"), truncate_utf8(v, MAX_LABEL_BYTES)) for k, v in sorted(labels.items())]
    write_cdb(args.output, records)

    blob = open(args.output, "rb").read()
    bad = [k for k, v in records if cdb_get(blob, k) != v]
    if bad:
        sys.exit(f"Verification failed for {len(bad)} keys, e.g. {bad[:3]}")

    print(f"Fetched {len(data)} entries: {len(records)} written, {invalid} invalid hex, "
          f"{empty} empty labels, {dupes} duplicates")
    print(f"Wrote {args.output} ({len(blob)} bytes)")


if __name__ == "__main__":
    main()
