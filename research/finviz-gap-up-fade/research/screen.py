# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Pull a Finviz universe for the gap-fade replication. No prices, no returns.

Small/micro book: US common stocks, market cap under $2B, price over $5,
average volume 100k to 1M, Finviz marks them shortable.
Mid book: the same filters with market cap $2B to $10B.

Tickers already used by small-cap-gap-up-fade are removed. The books are every
16th remaining small name and every 14th remaining mid name, in alphabetical
order. That stride was fixed before any of these names' returns were read.

    python research/finviz-gap-up-fade/research/screen.py
"""

from __future__ import annotations

import json
import re
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
UA = {"User-Agent": "Mozilla/5.0 (compatible; research-screen/1.0)"}
SMALL_URL = (
    "https://finviz.com/screener.ashx?v=411&f="
    "cap_smallunder,geo_usa,ind_stocksonly,sh_avgvol_100to1000,sh_opt_short,sh_price_o5"
)
MID_URL = (
    "https://finviz.com/screener.ashx?v=411&f="
    "cap_mid,geo_usa,ind_stocksonly,sh_avgvol_100to1000,sh_opt_short,sh_price_o5"
)
ALREADY_TESTED = {
    "ACCO", "AUDC", "BGS", "BOOM", "CHCT", "CLW", "CYH", "DSX", "ELME",
    "FNWD", "FSBW", "FXNC", "HDSN", "III", "IMMR", "JILL", "LMNR", "NAGE",
    "OSUR", "PTLO", "RM", "RWAY", "SMTI", "STRT", "VFF", "XPER", "ZUMZ",
    "AIV", "AVNW", "BNED", "CCCC", "CION", "CZFS", "EGAN", "FNKO", "FRAF",
    "FSTR", "GCO", "HLLY", "HRZN", "IIIV", "INGN", "LE", "LOVE", "OPI",
    "OVBC", "PERI", "RAIL", "RMNI", "SAR", "SPOK", "TBCH", "UIS", "VNDA", "ZH",
}
SMALL_STRIDE = 16
MID_STRIDE = 14


def fetch_tickers(url: str) -> list[str]:
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as resp:
        html = resp.read().decode("utf-8", "replace")
    seen: list[str] = []
    have: set[str] = set()
    for ticker in re.findall(r'data-boxover-ticker="([A-Z0-9.-]+)"', html):
        if ticker not in have:
            have.add(ticker)
            seen.append(ticker)
    if not seen:
        raise SystemExit(f"no tickers parsed from {url}")
    return seen


def eligible(tickers: list[str]) -> list[str]:
    out = []
    for ticker in tickers:
        if not re.fullmatch(r"[A-Z]{1,5}", ticker):
            continue
        if ticker in ALREADY_TESTED:
            continue
        out.append(ticker)
    return out


def main() -> None:
    small_all = fetch_tickers(SMALL_URL)
    mid_all = fetch_tickers(MID_URL)
    small = eligible(small_all)
    mid = eligible(mid_all)
    primary = small[::SMALL_STRIDE]
    cross = mid[::MID_STRIDE]
    payload = {
        "fetched_note": "Finviz free screener, ticker view, alphabetical. No performance column was used.",
        "small_url": SMALL_URL,
        "mid_url": MID_URL,
        "small_screen_n": len(small_all),
        "mid_screen_n": len(mid_all),
        "small_eligible_n": len(small),
        "mid_eligible_n": len(mid),
        "small_stride": SMALL_STRIDE,
        "mid_stride": MID_STRIDE,
        "excluded_already_tested": sorted(ALREADY_TESTED),
        "primary": primary,
        "cross": cross,
        "small_all": small_all,
        "mid_all": mid_all,
    }
    (HERE / "universe.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"small_screen {len(small_all)} eligible {len(small)} primary {len(primary)}")
    print("PRIMARY " + " ".join(primary))
    print(f"mid_screen {len(mid_all)} eligible {len(mid)} cross {len(cross)}")
    print("CROSS " + " ".join(cross))


if __name__ == "__main__":
    main()
