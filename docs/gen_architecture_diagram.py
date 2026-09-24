#!/usr/bin/env python3
# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

"""Render docs/architecture.svg and docs/architecture.png (UML-style).

Colors are the Stratum dark palette in apps/terminal/src/ui/Theme.h
(kPalDark). Slate surfaces separate nesting. Accent, amber, and green
mark synchronous calls, async calls, and returns. Brick is unused:
this figure has no error state. Do not add hues.
"""

from __future__ import annotations

import math
from pathlib import Path

import cairo

OUT_DIR = Path(__file__).resolve().parent
W, H = 1800, 6720
SCALE = 2


def _rgb(hex_color: str) -> tuple[float, float, float]:
    value = hex_color.removeprefix("#")
    return tuple(int(value[i : i + 2], 16) / 255.0 for i in (0, 2, 4))


# Byte-identical to Theme.h. kLine (#262e3a) and kLine2 (#313848) are the
# in-app hairlines; at figure scale they vanish on these fills, so strokes
# use the faint text token. Brick #b5544e is reserved and unused.
BG0 = _rgb("0d1116")
BG1 = _rgb("12171e")
BG2 = _rgb("171d26")
BG3 = _rgb("1e2530")
TEXT = _rgb("ccd3dd")
TEXT_DIM = _rgb("828c9b")
TEXT_FAINT = _rgb("586273")
ACCENT = _rgb("6f97c9")
WARN = _rgb("c08552")
OK = _rgb("5f8a63")

BG = BG0
INK = TEXT
MUTED = TEXT_DIM
LINE = TEXT_FAINT

PKG_FILL = BG1
PKG_LINE = TEXT_FAINT
PANEL_FILL = BG2
PANEL_HEAD = BG3
WELL_FILL = BG0
WELL_HEAD = BG3

EXEC_FILL = PANEL_FILL
EXEC_HEAD = ACCENT
LIB_FILL = PANEL_FILL
LIB_HEAD = PANEL_HEAD
COMMON_FILL = PANEL_FILL
COMMON_HEAD = PANEL_HEAD
CLASS_FILL = WELL_FILL
CLASS_HEAD = WELL_HEAD
PRIV_FILL = WELL_FILL
PRIV_HEAD = BG2
EXT_FILL = PANEL_FILL
EXT_HEAD = PANEL_HEAD
ART_FILL = WELL_FILL
SEQ_ACT = ACCENT
RETURN = OK
SYNC = ACCENT
ASYNC = WARN
NOTE = BG3
CHART_FILL = WELL_FILL
CHART_HEAD = ACCENT

_ROLE = (ACCENT, WARN, OK)


def set_color(ctx: cairo.Context, rgb: tuple[float, float, float], a: float = 1.0) -> None:
    ctx.set_source_rgba(*rgb, a)


def font(ctx: cairo.Context, size: float, bold: bool = False, italic: bool = False) -> None:
    slant = cairo.FONT_SLANT_ITALIC if italic else cairo.FONT_SLANT_NORMAL
    weight = cairo.FONT_WEIGHT_BOLD if bold else cairo.FONT_WEIGHT_NORMAL
    ctx.select_font_face("DejaVu Sans", slant, weight)
    ctx.set_font_size(size)


def text_w(ctx: cairo.Context, s: str, size: float, bold: bool = False, italic: bool = False) -> float:
    font(ctx, size, bold, italic)
    return ctx.text_extents(s).x_advance


def draw_text(
    ctx: cairo.Context,
    x: float,
    y: float,
    s: str,
    size: float,
    bold: bool = False,
    italic: bool = False,
    color: tuple[float, float, float] = INK,
    align: str = "left",
) -> None:
    font(ctx, size, bold, italic)
    set_color(ctx, color)
    tw = ctx.text_extents(s).x_advance
    if align == "center":
        x -= tw / 2
    elif align == "right":
        x -= tw
    ctx.move_to(x, y)
    ctx.show_text(s)


def rect(
    ctx: cairo.Context,
    x: float,
    y: float,
    w: float,
    h: float,
    fill: tuple[float, float, float],
    stroke: tuple[float, float, float] = LINE,
    lw: float = 1.15,
    dash: list[float] | None = None,
) -> None:
    ctx.set_line_width(lw)
    ctx.set_dash(dash or [])
    ctx.rectangle(x, y, w, h)
    set_color(ctx, fill)
    ctx.fill_preserve()
    set_color(ctx, stroke)
    ctx.stroke()
    ctx.set_dash([])


def package(
    ctx: cairo.Context,
    x: float,
    y: float,
    w: float,
    h: float,
    title: str,
    stereo: str,
    mark: tuple[float, float, float] | None = None,
) -> None:
    tab_w = max(180.0, text_w(ctx, stereo + "  " + title, 11) + 24)
    tab_h = 22
    rect(ctx, x, y, tab_w, tab_h, PKG_FILL, PKG_LINE, 1.2)
    rect(ctx, x, y + tab_h - 1, w, h - tab_h + 1, PKG_FILL, PKG_LINE, 1.2)
    if mark is not None:
        ctx.rectangle(x, y, tab_w, 3)
        set_color(ctx, mark)
        ctx.fill()
    draw_text(ctx, x + 8, y + 15, stereo, 9, italic=True, color=MUTED)
    draw_text(ctx, x + 8 + text_w(ctx, stereo + "  ", 9, italic=True), y + 15, title, 12, True)


def component_icon(ctx: cairo.Context, x: float, y: float) -> None:
    ctx.set_line_width(1.05)
    ctx.rectangle(x + 7, y + 6, 15, 13)
    set_color(ctx, BG0)
    ctx.fill_preserve()
    set_color(ctx, TEXT)
    ctx.stroke()
    for oy in (8.5, 13.5):
        ctx.rectangle(x + 3, y + oy, 8, 3.4)
        set_color(ctx, BG0)
        ctx.fill_preserve()
        set_color(ctx, TEXT)
        ctx.stroke()


def framed(
    ctx: cairo.Context,
    x: float,
    y: float,
    w: float,
    h: float,
    fill: tuple[float, float, float],
    head: tuple[float, float, float],
    head_h: float,
    lw: float,
) -> None:
    mark = head if head in _ROLE else None
    band = BG3 if mark is not None else head
    rect(ctx, x, y, w, h, fill, LINE, lw)
    ctx.rectangle(x, y, w, head_h)
    set_color(ctx, band)
    ctx.fill()
    set_color(ctx, LINE)
    ctx.set_line_width(lw)
    ctx.move_to(x, y + head_h)
    ctx.line_to(x + w, y + head_h)
    ctx.stroke()
    ctx.rectangle(x, y, w, h)
    ctx.stroke()
    if mark is not None:
        ctx.rectangle(x, y, w, 3)
        set_color(ctx, mark)
        ctx.fill()


def component(
    ctx: cairo.Context,
    x: float,
    y: float,
    w: float,
    h: float,
    name: str,
    stereo: str,
    fill: tuple[float, float, float],
    head: tuple[float, float, float],
    head_h: float = 38,
) -> None:
    framed(ctx, x, y, w, h, fill, head, head_h, 1.25)
    component_icon(ctx, x + 6, y + 8)
    draw_text(ctx, x + w / 2, y + 15, stereo, 9, italic=True, color=MUTED, align="center")
    draw_text(ctx, x + w / 2, y + 30, name, 13, True, align="center")


def inner(
    ctx: cairo.Context,
    x: float,
    y: float,
    w: float,
    h: float,
    name: str,
    stereo: str | None,
    fill: tuple[float, float, float],
    head: tuple[float, float, float],
    lines: list[str] | None = None,
    head_h: float = 20,
    name_size: float = 11,
) -> None:
    framed(ctx, x, y, w, h, fill, head, head_h, 1.05)
    label = f"{stereo}  {name}" if stereo else name
    draw_text(ctx, x + w / 2, y + head_h - 5, label, name_size, True, align="center")
    if lines:
        yy = y + head_h + 14
        for line in lines:
            draw_text(ctx, x + w / 2, yy, line, 10, color=MUTED, align="center")
            yy += 13


def arrow_head(ctx: cairo.Context, x: float, y: float, angle: float, size: float = 8, open_head: bool = False) -> None:
    a1 = angle + math.pi - 0.40
    a2 = angle + math.pi + 0.40
    x1, y1 = x + size * math.cos(a1), y + size * math.sin(a1)
    x2, y2 = x + size * math.cos(a2), y + size * math.sin(a2)
    ctx.set_line_width(1.15)
    if open_head:
        ctx.move_to(x1, y1)
        ctx.line_to(x, y)
        ctx.line_to(x2, y2)
        ctx.stroke()
    else:
        ctx.move_to(x, y)
        ctx.line_to(x1, y1)
        ctx.line_to(x2, y2)
        ctx.close_path()
        ctx.fill()


def line_arrow(
    ctx: cairo.Context,
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    color: tuple[float, float, float] = LINE,
    dashed: bool = False,
    open_head: bool = False,
    lw: float = 1.2,
) -> None:
    set_color(ctx, color)
    ctx.set_line_width(lw)
    ctx.set_dash([5, 3.5] if dashed else [])
    ctx.move_to(x1, y1)
    ctx.line_to(x2, y2)
    ctx.stroke()
    ctx.set_dash([])
    arrow_head(ctx, x2, y2, math.atan2(y2 - y1, x2 - x1), 8, open_head)


def note_box(ctx: cairo.Context, x: float, y: float, w: float, h: float, lines: list[str]) -> None:
    fold = 10
    ctx.move_to(x, y)
    ctx.line_to(x + w - fold, y)
    ctx.line_to(x + w, y + fold)
    ctx.line_to(x + w, y + h)
    ctx.line_to(x, y + h)
    ctx.close_path()
    set_color(ctx, NOTE)
    ctx.fill_preserve()
    set_color(ctx, LINE)
    ctx.set_line_width(1.0)
    ctx.stroke()
    ctx.move_to(x + w - fold, y)
    ctx.line_to(x + w - fold, y + fold)
    ctx.line_to(x + w, y + fold)
    ctx.stroke()
    yy = y + 14
    for i, line in enumerate(lines):
        draw_text(ctx, x + 8, yy, line, 9.5, bold=(i == 0), color=INK if i == 0 else MUTED)
        yy += 13


def draw_figure1(ctx: cairo.Context) -> None:
    draw_text(ctx, 28, 34, "terminal superproject — component interactions", 22, True)
    draw_text(
        ctx,
        28,
        54,
        "UML 2 component diagram  ·  CMake targets  ·  composition by nesting  ·  namespace terminal",
        11.5,
        color=MUTED,
    )
    draw_text(ctx, 28, 80, "Figure 1.  Components and assembly", 13.5, True)

    package(ctx, 16, 94, 1104, 1332, "terminal", "«system»", mark=ACCENT)
    draw_text(
        ctx,
        32,
        136,
        "root CMakeLists.txt   add_subdirectory(libs/market-data)  apps/ingest  apps/terminal",
        10,
        color=MUTED,
    )

    # --- terminal ---
    component(ctx, 32, 148, 684, 668, "terminal", "«executable»  apps/terminal", EXEC_FILL, EXEC_HEAD)
    inner(ctx, 46, 196, 656, 604, "Application", "«composition»", CLASS_FILL, CLASS_HEAD, head_h=20)

    specs = [
        ("Window", "GlfwContext  ·  VkSurface"),
        ("VulkanContext", "instance / device / queue"),
        ("VulkanSwapchain", "render + present"),
        ("ImGuiLayer", "Theme  ·  docking  ·  ImPlot"),
    ]
    pw, ph, gap = 151, 50, 8
    px, py = 58, 224
    for i, (name, detail) in enumerate(specs):
        inner(ctx, px + i * (pw + gap), py, pw, ph, name, None, PANEL_FILL, PANEL_HEAD, [detail], head_h=20)

    inner(ctx, 58, 284, 632, 500, "Workspace", "«composition»", PANEL_FILL, PANEL_HEAD, head_h=20)
    draw_text(
        ctx,
        374,
        316,
        "TitleBar  ·  StatusRail  ·  DATA left 30%  ·  one visible chartbook",
        10,
        color=MUTED,
        align="center",
    )
    inner(ctx, 72, 324, 604, 118, "InventoryPanel   DATA", "«composition»", WELL_FILL, ACCENT, head_h=20)
    inner(
        ctx,
        86,
        350,
        282,
        80,
        "Store  Reader",
        "«object»",
        PANEL_FILL,
        PANEL_HEAD,
        ["busy_timeout = 0  ·  1m + 1d coverage", "ctor: one-shot Writer migrate to v3"],
        head_h=20,
    )
    inner(
        ctx,
        380,
        350,
        282,
        80,
        "IngestWorker",
        "«active»",
        PANEL_FILL,
        PANEL_HEAD,
        ["Writer + CurlClient on the worker thread", "bars, splits, statements, or one chain slice"],
        head_h=20,
    )
    inner(
        ctx,
        72,
        450,
        604,
        318,
        "ChartbookHost",
        "«composition»",
        CHART_FILL,
        CHART_HEAD,
        head_h=20,
    )
    inner(
        ctx,
        86,
        478,
        176,
        108,
        "Store  Reader",
        "«object»",
        PANEL_FILL,
        PANEL_HEAD,
        ["own GUI Reader", "busy_timeout = 0", "after DATA migrates"],
        head_h=20,
    )
    inner(
        ctx,
        86,
        594,
        176,
        158,
        "CChartBook",
        "«composition»",
        PANEL_FILL,
        PANEL_HEAD,
        ["panes_", "financials_", "options_", "closed ones drop on export"],
        head_h=20,
    )
    inner(
        ctx,
        270,
        478,
        390,
        86,
        "CChartPane  ×N",
        "«composition»",
        PANEL_FILL,
        ACCENT,
        [
            "1d stored · 5m/15m/1h composite · split-adjust 1d",
            "StudyRegistry: moving_average, bollinger, volume",
            "settings and studies persist in the chartbook",
        ],
        head_h=20,
    )
    inner(
        ctx,
        270,
        572,
        390,
        86,
        "FinancialsPanel  ×N",
        "«composition»",
        PANEL_FILL,
        ACCENT,
        [
            "View > New Financials · dock id financials:<id>",
            "Income, Balance, Cash Flow · Yearly or Quarterly",
            "reads statement_* · enqueues ingestStatement",
        ],
        head_h=20,
    )
    inner(
        ctx,
        270,
        666,
        390,
        86,
        "OptionsChainPanel  ×N",
        "«composition»",
        PANEL_FILL,
        ACCENT,
        [
            "View > New Options Chain · dock id options:<id>",
            "calls left of the strike · puts right",
            "reads option_* · enqueues ingestOptions",
        ],
        head_h=20,
    )

    # --- ingest ---
    component(ctx, 732, 148, 372, 176, "ingest", "«executable»  apps/ingest", EXEC_FILL, EXEC_HEAD)
    inner(
        ctx,
        746,
        196,
        344,
        112,
        "main",
        None,
        CLASS_FILL,
        CLASS_HEAD,
        ["Store Writer  ·  CurlClient", "ingestSymbol / ingestDailySymbol", "--timeframe 1m|1d  ·  IngestDefaults", "progress: std::clog per session"],
        head_h=20,
    )
    note_box(
        ctx,
        732,
        340,
        372,
        84,
        [
            "Shared sources — not a CMake target",
            "Both executables compile CurlClient.cpp",
            "and include apps/common (RepoRoot.h).",
            "Repo root = CMakeLists.txt + libs/market-data/.",
        ],
    )
    note_box(
        ctx,
        732,
        450,
        372,
        124,
        [
            "GUI panes do not call HTTP",
            "Charts, financials, and chains enqueue.",
            "1d bars are stored; 5m/15m/1h are not.",
            "Daily plot uses adjustBarsForSplits.",
            "Books: data/chartbooks/*.chartbook.json",
            "Panes, studies, financials, chains persist.",
        ],
    )

    # --- apps/common ---
    component(
        ctx,
        32,
        832,
        1072,
        114,
        "apps/common",
        "«compilation unit»  compiled into terminal and ingest",
        COMMON_FILL,
        COMMON_HEAD,
    )
    inner(
        ctx,
        48,
        878,
        520,
        52,
        "CurlClient",
        "«realizes» HttpGet",
        CLASS_FILL,
        CLASS_HEAD,
        ["one libcurl easy handle   HTTPS-only   getWithRetry 0 / 429 / 5xx"],
        head_h=20,
    )
    inner(
        ctx,
        584,
        878,
        504,
        52,
        "RepoRoot",
        None,
        CLASS_FILL,
        CLASS_HEAD,
        ["findRepoRoot()  ·  db/secrets paths  ·  IngestDefaults 14d / 5y"],
        head_h=20,
    )

    # --- market-data ---
    component(
        ctx,
        32,
        962,
        1072,
        448,
        "market-data",
        "«library»  libs/market-data   static   PUBLIC include = src/",
        LIB_FILL,
        LIB_HEAD,
    )
    draw_text(ctx, 48, 1018, "public API", 10.5, True, color=MUTED)

    cells = [
        ("ingestSymbol()", "1m RTH sessions  ·  skip complete"),
        ("ingestDailySymbol()", "daily bars + coverage  ·  ingestSplits"),
        ("ingestStatement()", "GET /v1 modules  ·  replaceStatement"),
        ("ingestOptions()", "GET /v3/markets/options"),
        ("Store", "WAL · FIGI identity · user_version 4"),
        ("replaceStatement()", "one grid  ·  omitted cells are deleted"),
        ("replaceOptionChain()", "one slice  ·  calendar may drop dates"),
        ("Schema", "schemaV4 baseline  ·  v1–v3 refused"),
        ("Identity / OpenFigi", "ensureInstrument  ·  verifyIdentities"),
        ("Secrets", "\"mboum\"  ·  optional \"openfigi\""),
        ("MboumJson / Map", "historical, modules, options  ·  nlohmann"),
        ("Adjust / Time / NYSE", "split adjust  ·  RTH windows  ·  holidays"),
    ]
    cw, ch = 256, 62
    for i, (name, detail) in enumerate(cells):
        col, row = i % 4, i // 4
        inner(
            ctx,
            48 + col * (cw + 8),
            1026 + row * (ch + 6),
            cw,
            ch,
            name,
            None,
            CLASS_FILL,
            CLASS_HEAD,
            [detail],
            head_h=20,
        )

    draw_text(ctx, 48, 1236, "private  (PRIVATE include dir — apps never see sqlite3.h)", 10.5, True, color=MUTED)
    private = [
        ("SqliteDb / Stmt / Txn", "no sqlite3.h in apps"),
        ("terminal_sqlite3", "deps/sqlite · THREADSAFE=1"),
        ("schema/v4.sql", "baseline  ·  instrument_listing"),
        ("fixtures/openfigi", "recorded OpenFIGI answers"),
    ]
    pw = 200
    for i, (name, detail) in enumerate(private):
        inner(
            ctx,
            48 + i * (pw + 10),
            1246,
            pw,
            58,
            name,
            None,
            PRIV_FILL if i < 2 else ART_FILL,
            PRIV_HEAD,
            [detail],
            head_h=20,
            name_size=10,
        )
    draw_text(
        ctx,
        48,
        1324,
        "An empty file runs v4.sql and is stamped 4.  Files stamped 1 to 3 are refused with the reset message.  A newer database is refused.",
        10,
        color=MUTED,
    )
    draw_text(
        ctx,
        48,
        1340,
        "replaceStatement replaces one grid.  replaceOptionChain replaces one expiration slice.  A missing vendor key is not stored as zero.  Studies are not Store rows.",
        10,
        color=MUTED,
    )

    # --- environment ---
    package(ctx, 1248, 94, 532, 1332, "Environment", "«external»")

    inner(ctx, 1266, 132, 496, 70, "Operator", "«actor»", PANEL_FILL, PANEL_HEAD, ["GO in DATA  ·  File/Chart menus  ·  ingest CLI"], head_h=22)

    ext = [
        (218, "GLFW 3.3", "«library»  window / Vulkan surface", EXT_FILL, EXT_HEAD),
        (280, "Vulkan", "«library»  instance, device, swapchain", EXT_FILL, EXT_HEAD),
        (342, "Dear ImGui", "«library»  deps/imgui  ·  docking", EXT_FILL, EXT_HEAD),
        (404, "ImPlot v1.0", "«library»  deps/implot  ·  no PlotCandlestick", EXT_FILL, EXT_HEAD),
        (466, "libcurl", "«library»  CURL::libcurl", EXT_FILL, EXT_HEAD),
        (528, "api.mboum.com", "«service»  historical, modules, options", EXT_FILL, EXT_HEAD),
        (590, "api.openfigi.com", "«service»  POST /v3/mapping  ·  ticker ↔ FIGI", EXT_FILL, EXT_HEAD),
        (652, "secrets.json", "«artifact»  gitignored  ·  mboum, openfigi", PANEL_FILL, PANEL_HEAD),
        (714, "data/market-data.sqlite", "«artifact»  WAL  ·  schema user_version 4", PANEL_FILL, PANEL_HEAD),
        (786, "Catch2 tests", "option_tests · statement_tests · chart_*", PANEL_FILL, PANEL_HEAD),
        (848, "clang-tidy", "first-party TUs  ·  warnings as errors", PANEL_FILL, PANEL_HEAD),
    ]
    for ey, name, detail, fill, head in ext:
        inner(ctx, 1266, ey, 496, 54, name, None, fill, head, [detail], head_h=20)

    gutter_x1 = 1120
    gutter_x2 = 1266
    arrows = [
        (245, "Window"),
        (307, "Vulkan*"),
        (369, "ImGuiLayer"),
        (431, "CChartPlot"),
        (493, "CurlClient"),
        (555, "HTTPS"),
        (617, "HTTPS POST"),
        (679, "Secrets"),
        (741, "Store"),
    ]
    for ay, label in arrows:
        dashed = not label.startswith("HTTPS")
        color = ASYNC if label.startswith("HTTPS") else SYNC
        line_arrow(ctx, gutter_x1, ay, gutter_x2, ay, color, dashed=dashed)
        draw_text(ctx, (gutter_x1 + gutter_x2) / 2, ay - 5, label, 8.5, italic=True, color=color, align="center")

    note_box(
        ctx,
        1266,
        916,
        496,
        490,
        [
            "Connections",
            "GUI Readers: InventoryPanel, ChartbookHost.",
            "Separate connections. busy_timeout = 0.",
            "InventoryPanel's ctor opens a Writer, which",
            "creates schema v4, then closes it.",
            "IngestWorker opens its own Writer (5000 ms).",
            "ChartbookHost's Reader serves chart panes,",
            "financials, and option chains.",
            "A busy coverage read leaves the DATA rows in place.",
            "",
            "Who calls MBoum and OpenFIGI",
            "No pane calls HTTP. Each GO enqueues a job.",
            "Every job confirms its ticker with OpenFIGI first.",
            "Bars call ingestSymbol or ingestDailySymbol.",
            "Statements call ingestStatement.",
            "Option chains call ingestOptions.",
            "A splits-only job calls ingestSplits.",
            "80 ms between HTTP calls inside one job.",
            "runJob checks options, then statements,",
            "then splits, then 1d, then 1m.",
            "",
            "Where rows live",
            "SQLite holds 1m and 1d bars, coverage,",
            "splits, statement grids, and chain snapshots.",
            "5m, 15m, and 1h are built at chart load.",
            "Studies are not Store rows. A chartbook file",
            "stores panes, studies, financials, and chains.",
            "DATA row select does not set the chart symbol.",
        ],
    )


def draw_lifeline_head(ctx: cairo.Context, x: float, y: float, w: float, name: str) -> None:
    inner(ctx, x - w / 2, y, w, 34, name, None, PANEL_FILL, PANEL_HEAD, head_h=34, name_size=10.5)


def draw_figure2(ctx: cairo.Context) -> None:
    y0 = 1454
    draw_text(ctx, 28, y0, "Figure 2.  Ingest interaction  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "DATA  GO  and  ingest CLI share this path.  enqueue is asynchronous; 1m calls ingestSymbol, 1d calls ingestDailySymbol (paged interval=daily).",
        11,
        color=MUTED,
    )

    names = [
        "Operator",
        "InventoryPanel",
        "IngestWorker",
        "ingestSymbol",
        "CurlClient",
        "Store",
        "SQLite WAL",
        "MBoum API",
    ]
    xs = [100, 300, 520, 760, 980, 1180, 1390, 1620]
    top = y0 + 40
    life_top = top + 34
    bottom = y0 + 780

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 168, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False, asyn: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else (ASYNC if asyn else SYNC)
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret or asyn, open_head=ret or asyn)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    def self_call(i: int, y: float, h: float, label: str) -> None:
        x = xs[i]
        set_color(ctx, SYNC)
        ctx.set_line_width(1.15)
        ctx.move_to(x + 6, y)
        ctx.line_to(x + 52, y)
        ctx.line_to(x + 52, y + h)
        ctx.line_to(x + 6, y + h)
        ctx.stroke()
        arrow_head(ctx, x + 6, y + h, math.pi, 7)
        draw_text(ctx, x + 58, y + h - 2, label, 9.5, color=SYNC)

    y = [
        top + 60,   # 1
        top + 102,  # 2
        top + 144,  # 3
        top + 186,  # 4
        top + 228,  # 5 self
        top + 276,  # 6
        top + 318,  # 7
        top + 360,  # 8
        top + 402,  # 9
        top + 444,  # 10 self
        top + 492,  # 11
        top + 534,  # 12
        top + 576,  # 13
        top + 618,  # 14
        top + 660,  # 15
        top + 702,  # 16
        top + 744,  # 17
    ]

    activation(1, y[0] - 8, y[16] + 10)
    activation(2, y[1] - 8, y[14] + 10)
    activation(3, y[2] - 8, y[13] + 10)
    activation(4, y[5] - 8, y[8] + 10)
    activation(5, y[3] - 8, y[16] + 10)
    activation(6, y[11] - 8, y[12] + 10)
    activation(7, y[6] - 8, y[7] + 10)

    call(0, 1, y[0], "1  GO(symbol, from, to)", asyn=True)
    call(1, 2, y[1], "2  enqueue(Job)  «async»", asyn=True)
    call(2, 3, y[2], "3  ingestSymbol(store, get, range, on_day)")
    call(3, 5, y[3], "4  ensureInstrument  (OpenFIGI when new or 24 h stale)")
    self_call(3, y[4] - 10, 18, "5  NYSE walk: holiday → complete 0/0; skip complete")
    call(3, 4, y[5], "6  get(v3 historical URL)")
    call(4, 7, y[6], "7  HTTPS GET + Bearer  ·  retry 0/429/5xx  ·  80 ms")
    call(7, 4, y[7], "8  HttpResponse {status, body}", ret=True)
    call(4, 3, y[8], "9  body  (401/403 throw)", ret=True)
    self_call(3, y[9] - 10, 18, "10  parseMboumV3  ·  mapV3Bar  ·  RTH  ·  drop forming")
    call(3, 5, y[10], "11  ingestSession(bars, 60s, expected=390)")
    call(5, 6, y[11], "12  BEGIN IMMEDIATE  ·  upsert bar + coverage")
    call(6, 5, y[12], "13  status from bar_count", ret=True)
    call(3, 2, y[13], "14  on_day → Snapshot.dirty", ret=True)
    call(2, 1, y[14], "15  snapshot() polled each frame", ret=True)
    call(1, 5, y[15], "16  queryCoverageSummaries / Days  (Reader)")
    call(5, 1, y[16], "17  CoverageSummary[]  ·  keep snapshot if busy", ret=True)

    ny = y0 + 800
    note_box(
        ctx,
        28,
        ny,
        560,
        86,
        [
            "Same collaboration from ingest CLI",
            "main replaces InventoryPanel + IngestWorker:",
            "loadMboumApiKey → CurlClient → Store Writer →",
            "ingestSymbol.  Progress is std::clog per day.",
        ],
    )
    note_box(
        ctx,
        608,
        ny,
        560,
        86,
        [
            "Store modes",
            "Writer: ingest CLI and IngestWorker thread.",
            "Readers: DATA, plus charts, financials, and chains.",
            "Writer creates schema v4; v1 to v3 files are refused.",
        ],
    )
    note_box(
        ctx,
        1188,
        ny,
        584,
        86,
        [
            "Legend",
            "Solid blue = synchronous call.   Dashed amber = async.   Dashed green = return.",
            "Accent bar = system, executables, DATA, and charts.   Nested boxes = composition.",
            "Ball = provided interface.   Socket = required.   Dashed «use» = dependency.",
        ],
    )


def draw_figure3(ctx: cairo.Context) -> None:
    y = 2364
    draw_text(ctx, 28, y, "Figure 3.  GUI frame  (sequence)  —  Application::run", 13.5, True)

    names = [
        "Application",
        "Window",
        "ImGuiLayer",
        "Workspace",
        "TitleBar",
        "ChartbookHost",
        "VulkanSwapchain",
    ]
    xs = [110, 360, 610, 870, 1130, 1390, 1650]
    top = y + 28
    life_top = top + 34
    bottom = y + 360

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 170, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, yy: float, label: str, ret: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else SYNC
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, yy, x2 - dx, yy, color, dashed=ret, open_head=ret)
        draw_text(ctx, (x1 + x2) / 2, yy - 5, label, 10, color=color, align="center")

    m = [top + 56, top + 96, top + 136, top + 176, top + 216, top + 256, top + 296, top + 336]
    activation(0, m[0] - 8, m[6] + 12)
    activation(1, m[0] - 8, m[0] + 12)
    activation(2, m[1] - 8, m[1] + 12)
    activation(3, m[2] - 8, m[5] + 12)
    activation(4, m[3] - 8, m[3] + 12)
    activation(5, m[4] - 8, m[5] + 12)
    activation(2, m[6] - 8, m[7] + 12)
    activation(6, m[7] - 8, m[7] + 12)

    call(0, 1, m[0], "1  pollEvents()")
    call(0, 2, m[1], "2  newFrame()")
    call(0, 3, m[2], "3  draw(window)")
    call(3, 4, m[3], "4  drawTitleBar  File / Chart / View + tabs")
    call(3, 5, m[4], "5  drawStatusRail  ingest · focused chart · NY clock")
    call(3, 5, m[5], "6  drawSpace  DATA, financials, options, panes")
    call(0, 2, m[6], "7  render(clear = Theme::kCanvas)")
    call(2, 6, m[7], "8  render + present")

    draw_text(
        ctx,
        28,
        y + 390,
        "Rebuild: Application.rebuildSwapchainIfNeeded → VulkanSwapchain.resize when the framebuffer size changes or the swapchain flags rebuild.",
        10.5,
        color=MUTED,
    )
    draw_text(
        ctx,
        28,
        y + 408,
        "terminal_gui compiles FinancialsPanel, OptionsChainPanel, ChartbookHost, and the chart TUs.  terminal_core compiles StatementSheet and IngestWorker.",
        10.5,
        color=MUTED,
    )
    draw_text(
        ctx,
        28,
        y + 426,
        "View menu: DATA, New Financials, Close Financials, New Options Chain, Close Options Chain.  Stored grain is 1m and 1d.  5m/15m/1h are load-time composites.",
        10.5,
        color=MUTED,
    )


def draw_figure4(ctx: cairo.Context) -> None:
    y0 = 2814
    draw_text(ctx, 28, y0, "Figure 4.  Chart load and draw  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "1d calls queryBars(id, 86400) and adjustBarsForSplits.  5m/15m/1h call queryBars(id, 60) then transformChartBars.  Apply/OK recomputes studies without assigning loaded_.  No MBoum.",
        11,
        color=MUTED,
    )

    names = ["Operator", "CChartBook", "CChartPane", "loadChartBars", "Store", "ImPlot"]
    xs = [120, 400, 700, 1020, 1320, 1600]
    top = y0 + 40
    life_top = top + 34
    bottom = y0 + 450

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 168, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False, asyn: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else (ASYNC if asyn else SYNC)
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret or asyn, open_head=ret or asyn)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    def self_call(i: int, y: float, h: float, label: str) -> None:
        x = xs[i]
        set_color(ctx, SYNC)
        ctx.set_line_width(1.15)
        ctx.move_to(x + 6, y)
        ctx.line_to(x + 52, y)
        ctx.line_to(x + 52, y + h)
        ctx.line_to(x + 6, y + h)
        ctx.stroke()
        arrow_head(ctx, x + 6, y + h, math.pi, 7)
        draw_text(ctx, x + 58, y + h - 2, label, 9.5, color=SYNC)

    y = [top + 56 + 40 * i for i in range(9)]
    activation(1, y[0] - 8, y[8] + 10)
    activation(2, y[1] - 8, y[8] + 10)
    activation(3, y[3] - 8, y[5] + 10)
    activation(4, y[3] - 8, y[5] + 10)
    activation(5, y[7] - 8, y[8] + 10)

    call(0, 1, y[0], "1  Chart >> New Chart")
    call(1, 2, y[1], "2  addPane()  unique_ptr<CChartPane>")
    call(0, 2, y[2], "3  Chart Settings  OK / Apply  (draft → settings)", asyn=True)
    call(2, 3, y[3], "4  loadChartBars(store, settings)  GUI thread")
    call(3, 4, y[4], "5  queryCoverageDays  ·  queryBars 1m or 1d")
    call(3, 2, y[5], "6  ChartLoadResult", ret=True)
    self_call(2, y[6] - 8, 16, "7  studiesForLoad (not keep-candles)")
    call(2, 5, y[7], "8  drawCandlesticks + overlays")
    self_call(5, y[8] - 8, 16, "9  drawStudyOverlays")

    note_box(
        ctx,
        28,
        y0 + 464,
        560,
        86,
        [
            "Window identity",
            "Dock ids are pane:<id>, financials:<id>, options:<id>.",
            "The chartbook file stores settings, studies, sheets, and chains.",
            "A closed window is omitted on export.  imgui.ini is gitignored.",
        ],
    )
    note_box(
        ctx,
        608,
        y0 + 464,
        560,
        86,
        [
            "Load gate",
            "1m/5m/15m/1h/1d candlesticks, Days to Load. Higher TFs composite from 1m.",
            "Reload: studiesForLoad when loaded_ is assigned, not on keep-candles.",
            "Studies Apply/OK also calls studiesForLoad. It does not assign loaded_.",
            "Composites are not stored.  Other bar types and limiters are rejected.",
        ],
    )
    note_box(
        ctx,
        1188,
        y0 + 464,
        584,
        86,
        [
            "Scale / interaction",
            "Automatic scale includes overlay values.  Constant Range and",
            "User Defined do not.  Wheel = spacing; drag plot pans.",
            "Y drag is Range or Move.  Pan does not recompute studies.",
        ],
    )


def draw_figure5(ctx: cairo.Context) -> None:
    y0 = 3424
    draw_text(ctx, 28, y0, "Figure 5.  Study compute and overlay  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "Studies are value types on CChartPane, not CChartSettings and not SQLite.  Chart Settings and Studies modals are mutually exclusive.",
        11,
        color=MUTED,
    )

    names = [
        "Operator",
        "CChartBook",
        "CChartPane",
        "CStudySettings",
        "computeStudies",
        "ImPlot",
    ]
    xs = [120, 400, 700, 1000, 1300, 1600]
    top = y0 + 40
    life_top = top + 34
    bottom = y0 + 390

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 168, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False, asyn: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else (ASYNC if asyn else SYNC)
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret or asyn, open_head=ret or asyn)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    def self_call(i: int, y: float, h: float, label: str) -> None:
        x = xs[i]
        set_color(ctx, SYNC)
        ctx.set_line_width(1.15)
        ctx.move_to(x + 6, y)
        ctx.line_to(x + 52, y)
        ctx.line_to(x + 52, y + h)
        ctx.line_to(x + 6, y + h)
        ctx.stroke()
        arrow_head(ctx, x + 6, y + h, math.pi, 7)
        draw_text(ctx, x + 58, y + h - 2, label, 9.5, color=SYNC)

    y = [top + 56 + 38 * i for i in range(9)]
    activation(1, y[0] - 8, y[1] + 10)
    activation(2, y[0] - 8, y[8] + 10)
    activation(3, y[2] - 8, y[3] + 10)
    activation(4, y[5] - 8, y[6] + 10)
    activation(5, y[7] - 8, y[8] + 10)

    call(0, 1, y[0], "1  Chart >> Studies  (disabled if settings_open)")
    call(1, 2, y[1], "2  openStudies()  studies_ → study_draft_")
    call(2, 3, y[2], "3  drawStudyDraftBody  Add / Remove / Length")
    call(3, 2, y[3], "4  Enter / Apply  (modal stays open)", ret=True)
    self_call(2, y[4] - 8, 16, "5  applyStudyDraft  studies_ = study_draft_  ·  no loadChartBars")
    call(2, 4, y[5], "6  studiesForLoad(loaded_, studies_)  Ready + non-empty bars")
    call(4, 2, y[6], "7  CStudySeries[]  (disabled omitted; warmup = NaN)", ret=True)
    call(2, 5, y[7], "8  drawCandlesticks then drawStudyOverlays")
    self_call(5, y[8] - 8, 16, "9  polylines · no PlotLine · Overlay")

    note_box(
        ctx,
        28,
        y0 + 404,
        560,
        86,
        [
            "Registered studies",
            "moving_average is always simple. Length 1..10000.",
            "Method tokens stay in the file and are not shown.",
            "bollinger draws upper, middle, lower. volume is a histogram.",
        ],
    )
    note_box(
        ctx,
        608,
        y0 + 404,
        560,
        86,
        [
            "Ownership",
            "CChartPane owns studies_, study_draft_, computed_.  Cap 16.",
            "Each saved study has kind, options, and outputs.",
            "Close Chart drops that pane on the next export.",
        ],
    )
    note_box(
        ctx,
        1188,
        y0 + 404,
        584,
        86,
        [
            "When compute runs",
            "reload assigns computed_ when it assigns loaded_.",
            "Busy/Error keep-candles does not call studiesForLoad.",
            "Pan, wheel, and Y-scale drag do not recompute.",
        ],
    )


def draw_figure6(ctx: cairo.Context) -> None:
    y0 = 3984
    draw_text(ctx, 28, y0, "Figure 6.  Market-data store  (schema v4)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "SQLite WAL at data/market-data.sqlite.  kSchemaUserVersion = 4.  Bars stay as-traded.  Statements and option chains are further tables in the same Store.",
        11,
        color=MUTED,
    )

    inner(
        ctx,
        28,
        y0 + 40,
        220,
        86,
        "instrument",
        "«table» v4",
        PANEL_FILL,
        PANEL_HEAD,
        ["PK id  ·  composite FIGI when present", "open symbol in instrument_listing"],
        head_h=22,
    )
    inner(
        ctx,
        264,
        y0 + 40,
        220,
        86,
        "bar",
        "«table» v1",
        PANEL_FILL,
        PANEL_HEAD,
        ["1m (60) and 1d (86400)", "as-traded OHLCV  ·  WITHOUT ROWID"],
        head_h=22,
    )
    inner(
        ctx,
        500,
        y0 + 40,
        220,
        86,
        "coverage_day",
        "«table» v1",
        PANEL_FILL,
        PANEL_HEAD,
        ["per instrument / timeframe / session", "complete / partial / missing / error"],
        head_h=22,
    )
    inner(
        ctx,
        736,
        y0 + 40,
        220,
        86,
        "corporate_action",
        "«table» v1",
        PANEL_FILL,
        PANEL_HEAD,
        ["splits + dividends as-stored", "adjustBarsForSplits at 1d read"],
        head_h=22,
    )
    inner(
        ctx,
        992,
        y0 + 40,
        380,
        86,
        "statement_snapshot",
        "«table» v2",
        PANEL_FILL,
        PANEL_HEAD,
        ["PK (instrument, statement, timeframe)", "income | balance | cashflow  ×  annually | quarterly | trailing"],
        head_h=22,
    )
    inner(
        ctx,
        992,
        y0 + 138,
        380,
        100,
        "statement_cell",
        "«table» v2  CASCADE from snapshot",
        PANEL_FILL,
        PANEL_HEAD,
        ["PK + line_item + period_end (YYYY-MM-DD or TTM)", "value_kind int | real | text  ·  exactly one value_*", "omitted vendor key → no row (never zero)"],
        head_h=22,
    )

    line_arrow(ctx, 1182, y0 + 138, 1182, y0 + 126, SYNC, dashed=False, lw=1.1)
    draw_text(ctx, 1194, y0 + 134, "FK CASCADE", 8.5, italic=True, color=SYNC)

    inner(
        ctx,
        28,
        y0 + 252,
        570,
        112,
        "option_underlying",
        "«table» v3",
        PANEL_FILL,
        PANEL_HEAD,
        [
            "PK instrument_id · FK instrument RESTRICT",
            "HV30, 1y IV rank, next earnings, ex-dividend",
            "one row per instrument · source mboum",
        ],
        head_h=22,
    )
    inner(
        ctx,
        614,
        y0 + 252,
        570,
        112,
        "option_expiry",
        "«table» v3",
        PANEL_FILL,
        PANEL_HEAD,
        [
            "PK (instrument, expiration, weekly|monthly)",
            "average_iv and fetched_at empty until quotes",
            "a calendar refresh drops dates no longer listed",
        ],
        head_h=22,
    )
    inner(
        ctx,
        1200,
        y0 + 252,
        570,
        112,
        "option_quote",
        "«table» v3  CASCADE from expiry",
        PANEL_FILL,
        PANEL_HEAD,
        [
            "PK (instrument_id, vendor_symbol)",
            "trade_date and trade_minute are exclusive",
            "percents stored as fractions · not a time series",
        ],
        head_h=22,
    )
    draw_text(
        ctx,
        28,
        y0 + 382,
        "option_quote references option_expiry ON DELETE CASCADE.  option_underlying and option_expiry reference instrument ON DELETE RESTRICT.",
        10,
        color=MUTED,
    )

    names = ["Caller", "Store", "statement_snapshot", "statement_cell", "SQLite WAL"]
    xs = [120, 420, 780, 1140, 1500]
    top = y0 + 408
    life_top = top + 34
    bottom = y0 + 660

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 176, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else SYNC
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret, open_head=ret)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    y = [top + 52 + 36 * i for i in range(6)]
    activation(1, y[0] - 8, y[5] + 10)
    activation(2, y[1] - 8, y[1] + 12)
    activation(3, y[2] - 8, y[3] + 12)
    activation(4, y[0] - 8, y[4] + 12)

    call(0, 1, y[0], "1  replaceStatement(snapshot, cells)  BEGIN IMMEDIATE")
    call(1, 2, y[1], "2  UPSERT snapshot  (instrument, statement, timeframe)")
    call(1, 3, y[2], "3  DELETE cells for that grid")
    call(1, 3, y[3], "4  INSERT each cell  (int XOR real XOR text)")
    call(1, 4, y[4], "5  COMMIT")
    call(1, 0, y[5], "6  findStatementSnapshot · queryStatementCells", ret=True)

    draw_text(ctx, 28, y0 + 684, "replaceOptionChain  —  one BEGIN IMMEDIATE transaction", 12, True)
    names = ["Caller", "Store", "option_underlying", "option_expiry", "option_quote"]
    xs = [140, 500, 900, 1280, 1620]
    top = y0 + 704
    life_top = top + 34
    bottom = y0 + 980
    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 176, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    y = [top + 48 + 36 * i for i in range(6)]
    activation(1, y[0] - 8, y[5] + 10)
    activation(2, y[1] - 8, y[1] + 12)
    activation(3, y[2] - 8, y[4] + 12)
    activation(4, y[3] - 8, y[3] + 12)

    call(0, 1, y[0], "1  replaceOptionChain(write)")
    call(1, 2, y[1], "2  UPSERT when has_underlying")
    call(1, 3, y[2], "3  UPSERT each quote slice")
    call(1, 4, y[3], "4  DELETE that slice, INSERT quotes")
    call(1, 3, y[4], "5  replace_calendar drops dates not kept")
    call(1, 0, y[5], "6  COMMIT", ret=True)

    note_box(
        ctx,
        28,
        y0 + 1000,
        560,
        108,
        [
            "Open",
            "An empty file runs v4.sql, the baseline, and is stamped 4.",
            "Files stamped 1 to 3 predate the FIGI identity and are refused.",
            "user_version > 4 is refused.  v4.sql is frozen; v5 migrates.",
            "Studies are not rows in this database.",
        ],
    )
    note_box(
        ctx,
        608,
        y0 + 1000,
        560,
        108,
        [
            "Statements",
            "income, balance, cashflow × annually, quarterly, trailing.",
            "The three timeframes are separate series, not views of each other.",
            "The pane offers Yearly and Quarterly.  Trailing remains stored.",
            "HTTP 200 with no grid still writes a snapshot and no cells.",
        ],
    )
    note_box(
        ctx,
        1188,
        y0 + 1000,
        584,
        108,
        [
            "Option chain",
            "One quote row is one contract.  The slice is not a time series.",
            "Replacing one date and type leaves every other slice.",
            "An unknown ticker writes nothing.",
            "Contract id is BASE|YYYYMMDD|STRIKE[W]C/P.",
        ],
    )


def draw_figure7(ctx: cairo.Context) -> None:
    y0 = 5120
    draw_text(ctx, 28, y0, "Figure 7.  Financials pane  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "View > New Financials docks a FinancialsPanel.  The panel reads the ChartbookHost Reader.  GO enqueues ingestStatement.  The pane does not call HTTP.",
        11,
        color=MUTED,
    )

    names = [
        "Operator",
        "ChartbookHost",
        "FinancialsPanel",
        "IngestWorker",
        "ingestStatement",
        "Store",
        "MBoum API",
    ]
    xs = [100, 340, 600, 880, 1160, 1420, 1680]
    top = y0 + 42
    life_top = top + 34
    bottom = y0 + 620

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 168, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False, asyn: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else (ASYNC if asyn else SYNC)
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret or asyn, open_head=ret or asyn)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    def self_call(i: int, y: float, h: float, label: str) -> None:
        x = xs[i]
        set_color(ctx, SYNC)
        ctx.set_line_width(1.15)
        ctx.move_to(x + 6, y)
        ctx.line_to(x + 46, y)
        ctx.line_to(x + 46, y + h)
        ctx.line_to(x + 6, y + h)
        ctx.stroke()
        arrow_head(ctx, x + 6, y + h, math.pi, 7)
        draw_text(ctx, x + 52, y + h - 2, label, 9.5, color=SYNC)

    y = [top + 48 + 32 * i for i in range(15)]
    activation(1, y[0] - 8, y[1] + 10)
    activation(2, y[1] - 8, y[14] + 10)
    activation(5, y[3] - 8, y[4] + 10)
    activation(3, y[5] - 8, y[11] + 10)
    activation(4, y[6] - 8, y[10] + 10)
    activation(6, y[7] - 8, y[8] + 10)
    activation(5, y[9] - 8, y[13] + 10)

    call(0, 1, y[0], "1  View > New Financials")
    call(1, 2, y[1], "2  addFinancials()  dock financials:<id>")
    call(0, 2, y[2], "3  SYMBOL, statement, period, GO", asyn=True)
    call(2, 5, y[3], "4  findStatementSnapshot · queryStatementCells")
    call(5, 2, y[4], "5  cells, or no snapshot", ret=True)
    call(2, 3, y[5], "6  enqueue(statements=true)  «async»", asyn=True)
    call(3, 4, y[6], "7  ingestStatement(store, get, symbol, kind, tf)")
    call(4, 6, y[7], "8  GET /v1/markets/stock/modules")
    call(6, 4, y[8], "9  HTTP 200 body", ret=True)
    call(4, 5, y[9], "10  replaceStatement  BEGIN IMMEDIATE")
    call(5, 4, y[10], "11  COMMIT", ret=True)
    call(3, 2, y[11], "12  snapshot().finished_serial polled", ret=True)
    call(2, 5, y[12], "13  queryStatementCells")
    call(5, 2, y[13], "14  StatementCell[]", ret=True)
    self_call(2, y[14] - 8, 16, "15  buildStatementSheet")

    note_box(
        ctx,
        28,
        y0 + 640,
        560,
        112,
        [
            "When it fetches",
            "GO sets fetch_now_ and enqueues with force.",
            "No snapshot also enqueues, unless that key failed or is in flight.",
            "The period combo is Yearly or Quarterly.  Trailing stays in the store.",
            "HTTP 200 with no grid writes an empty snapshot, so it is not fetched again.",
        ],
    )
    note_box(
        ctx,
        608,
        y0 + 640,
        560,
        112,
        [
            "The sheet",
            "buildStatementSheet keeps the four newest period ends.",
            "TTM, fiscalYear, and fiscalQuarter rows are omitted.",
            "Modules: income-statement-v2, balance-sheet-v2, cashflow-statement-v2.",
            "A closed panel is omitted on export.  Format stays 1.",
        ],
    )
    note_box(
        ctx,
        1188,
        y0 + 640,
        584,
        112,
        [
            "Guards",
            "A stored FIGI wins over the symbol; a renamed ticker follows.",
            "A busy or locked read sets needs_reload_ and keeps a same-key sheet.",
            "ChartbookHost passes its Reader and InventoryPanel's worker.",
            "The saved sheet is symbol, statement, and timeframe.",
        ],
    )


def draw_figure8(ctx: cairo.Context) -> None:
    y0 = 5920
    draw_text(ctx, 28, y0, "Figure 8.  Option chain  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "View > New Options Chain docks an OptionsChainPanel.  GO enqueues ingestOptions for one expiration.  expiration 0 lets the server choose the date.  The pane does not call HTTP.",
        11,
        color=MUTED,
    )

    names = [
        "Operator",
        "ChartbookHost",
        "OptionsChainPanel",
        "IngestWorker",
        "ingestOptions",
        "Store",
        "MBoum API",
    ]
    xs = [100, 340, 610, 890, 1160, 1420, 1680]
    top = y0 + 42
    life_top = top + 34
    bottom = y0 + 620

    for name, x in zip(names, xs):
        draw_lifeline_head(ctx, x, top, 176, name)
        set_color(ctx, MUTED)
        ctx.set_line_width(1.0)
        ctx.set_dash([3, 3.5])
        ctx.move_to(x, life_top)
        ctx.line_to(x, bottom)
        ctx.stroke()
        ctx.set_dash([])

    def activation(i: int, y1: float, y2: float) -> None:
        x = xs[i]
        rect(ctx, x - 5, y1, 10, max(12.0, y2 - y1), SEQ_ACT, SYNC, 0.9)

    def call(i: int, j: int, y: float, label: str, *, ret: bool = False, asyn: bool = False) -> None:
        x1, x2 = xs[i], xs[j]
        color = RETURN if ret else (ASYNC if asyn else SYNC)
        dx = 6 if x2 > x1 else -6
        line_arrow(ctx, x1 + dx, y, x2 - dx, y, color, dashed=ret or asyn, open_head=ret or asyn)
        draw_text(ctx, (x1 + x2) / 2, y - 5, label, 9.5, color=color, align="center")

    def self_call(i: int, y: float, h: float, label: str) -> None:
        x = xs[i]
        set_color(ctx, SYNC)
        ctx.set_line_width(1.15)
        ctx.move_to(x + 6, y)
        ctx.line_to(x + 46, y)
        ctx.line_to(x + 46, y + h)
        ctx.line_to(x + 6, y + h)
        ctx.stroke()
        arrow_head(ctx, x + 6, y + h, math.pi, 7)
        draw_text(ctx, x + 52, y + h - 2, label, 9.5, color=SYNC)

    y = [top + 48 + 32 * i for i in range(15)]
    activation(1, y[0] - 8, y[1] + 10)
    activation(2, y[1] - 8, y[14] + 10)
    activation(5, y[3] - 8, y[4] + 10)
    activation(3, y[5] - 8, y[11] + 10)
    activation(4, y[6] - 8, y[10] + 10)
    activation(6, y[7] - 8, y[8] + 10)
    activation(5, y[9] - 8, y[13] + 10)

    call(0, 1, y[0], "1  View > New Options Chain")
    call(1, 2, y[1], "2  addOptions()  dock options:<id>")
    call(0, 2, y[2], "3  SYMBOL, expiration, GO", asyn=True)
    call(2, 5, y[3], "4  queryOptionExpiries · Underlying · Quotes")
    call(5, 2, y[4], "5  slice, or not fetched", ret=True)
    call(2, 3, y[5], "6  enqueue(options=true)  «async»", asyn=True)
    call(3, 4, y[6], "7  ingestOptions(store, get, symbol, expiration)")
    call(4, 6, y[7], "8  GET /v3/markets/options")
    call(6, 4, y[8], "9  HTTP 200 body", ret=True)
    call(4, 5, y[9], "10  replaceOptionChain  BEGIN IMMEDIATE")
    call(5, 4, y[10], "11  COMMIT", ret=True)
    call(3, 2, y[11], "12  snapshot().finished_serial polled", ret=True)
    call(2, 5, y[12], "13  queryOptionQuotes for the slice")
    call(5, 2, y[13], "14  OptionQuote[]", ret=True)
    self_call(2, y[14] - 8, 16, "15  drawChain")

    note_box(
        ctx,
        28,
        y0 + 640,
        560,
        112,
        [
            "When it fetches",
            "expiration 0 omits the date, and the server chooses one.",
            "An unknown ticker returns before replaceOptionChain.",
            "A finished fetch with no slice sets failed_key_.",
            "That key is not enqueued again until GO.",
        ],
    )
    note_box(
        ctx,
        608,
        y0 + 640,
        560,
        112,
        [
            "How the slice is chosen",
            "A missing symbol is looked up again as $SYMBOL.",
            "One match replaces the active symbol.",
            "No chosen date uses the newest fetched_at.  Monthly wins a tie.",
            "Calls sit left of the strike and puts sit right.",
        ],
    )
    note_box(
        ctx,
        1188,
        y0 + 640,
        584,
        112,
        [
            "What is saved and drawn",
            "The file stores symbol, expiration, and weekly or monthly.",
            "A closed chain is omitted on export.  Format stays 1.",
            "Call moneyness > 0 is in the money.  Put moneyness < 0 is in the money.",
            "Hover on last shows mid, theta, vega, rho, open-interest change, and trade time.",
        ],
    )


def paint(ctx: cairo.Context) -> None:
    set_color(ctx, BG)
    ctx.paint()
    draw_figure1(ctx)
    draw_figure2(ctx)
    draw_figure3(ctx)
    draw_figure4(ctx)
    draw_figure5(ctx)
    draw_figure6(ctx)
    draw_figure7(ctx)
    draw_figure8(ctx)


def main() -> None:
    svg_path = OUT_DIR / "architecture.svg"
    png_path = OUT_DIR / "architecture.png"

    svg = cairo.SVGSurface(str(svg_path), W, H)
    svg.set_document_unit(cairo.SVG_UNIT_PX)
    ctx = cairo.Context(svg)
    paint(ctx)
    svg.finish()

    img = cairo.ImageSurface(cairo.FORMAT_ARGB32, W * SCALE, H * SCALE)
    ctx = cairo.Context(img)
    ctx.scale(SCALE, SCALE)
    paint(ctx)
    img.write_to_png(str(png_path))
    print(f"wrote {svg_path}")
    print(f"wrote {png_path}")


if __name__ == "__main__":
    main()
