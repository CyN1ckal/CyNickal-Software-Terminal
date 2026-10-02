# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Download continuous futures daily bars from MBoum into data/raw/<symbol>.json.

The terminal's `ingest` cannot store futures (it confirms tickers with OpenFIGI and
files bars on NYSE sessions), so this study keeps its own copy of the vendor JSON
next to the code. It never writes to the repo's data/ store. One request per symbol;
every request URL is appended to data/requests.log. Existing files are not re-fetched.

    python research/micro-futures-trend/research/fetch.py [SYMBOL ...]
"""
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
RAW = HERE / "data" / "raw"
LOG = HERE / "data" / "requests.log"

SYMBOLS = [
    # equity index
    "ES=F", "NQ=F", "RTY=F", "YM=F",
    # micro Treasury yield futures, and the price-quoted notes and bonds
    "2YY=F", "5YY=F", "10Y=F", "30Y=F", "ZT=F", "ZF=F", "ZN=F", "ZB=F",
    # metals and energy
    "GC=F", "SI=F", "HG=F", "CL=F",
    # currencies
    "6E=F", "6J=F", "6B=F", "6A=F", "6C=F", "6S=F",
]


def user_agent() -> str:
    src = (ROOT / "apps" / "common" / "CurlClient.cpp").read_text()
    return re.search(r'kUserAgent\s*=\s*"([^"]+)"', src).group(1)


def fetch(symbol: str, key: str, ua: str) -> tuple[int, bytes]:
    query = urllib.parse.urlencode({"ticker": symbol, "interval": "1d"})
    url = "https://api.mboum.com/v1/markets/stock/history?" + query
    with LOG.open("a") as f:
        f.write(f"{datetime.now(timezone.utc).isoformat(timespec='seconds')} {url}\n")
    req = urllib.request.Request(url, headers={"Authorization": "Bearer " + key, "User-Agent": ua})
    try:
        with urllib.request.urlopen(req, timeout=90) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def main() -> None:
    RAW.mkdir(parents=True, exist_ok=True)
    key = json.loads((ROOT / "secrets.json").read_text())["mboum"]
    ua = user_agent()
    for symbol in sys.argv[1:] or SYMBOLS:
        out = RAW / (symbol.replace("=", "_") + ".json")
        if out.exists():
            print(f"{symbol:7} cached")
            continue
        status, body = fetch(symbol, key, ua)
        if status != 200:
            print(f"{symbol:7} HTTP {status} {body[:120]!r}")
            continue
        doc = json.loads(body)
        out.write_text(json.dumps(doc))
        meta = doc.get("meta", {})
        print(f"{symbol:7} {len(doc.get('body', {})):5d} bars  type={meta.get('instrumentType')} "
              f"exch={meta.get('exchangeName')}")
        time.sleep(0.2)


if __name__ == "__main__":
    main()
