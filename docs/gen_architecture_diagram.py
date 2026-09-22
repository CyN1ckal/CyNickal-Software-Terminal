#!/usr/bin/env python3
# Copyright 2026 CyNickal Software LLC
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

"""Render docs/architecture.svg and docs/architecture.png (UML-style)."""

from __future__ import annotations

import math
from pathlib import Path

import cairo

OUT_DIR = Path(__file__).resolve().parent
W, H = 1800, 3860
SCALE = 2

BG = (0.97, 0.968, 0.955)
INK = (0.13, 0.13, 0.13)
MUTED = (0.36, 0.36, 0.36)
LINE = (0.22, 0.22, 0.22)
PKG_FILL = (0.96, 0.95, 0.92)
PKG_LINE = (0.40, 0.38, 0.34)
EXEC_FILL = (1.00, 0.985, 0.88)
EXEC_HEAD = (0.93, 0.82, 0.40)
LIB_FILL = (0.89, 0.95, 0.89)
LIB_HEAD = (0.60, 0.78, 0.60)
COMMON_FILL = (0.93, 0.91, 0.97)
COMMON_HEAD = (0.72, 0.66, 0.86)
CLASS_FILL = (0.89, 0.94, 0.98)
CLASS_HEAD = (0.68, 0.82, 0.92)
PRIV_FILL = (0.935, 0.935, 0.935)
PRIV_HEAD = (0.74, 0.74, 0.74)
EXT_FILL = (0.97, 0.91, 0.90)
EXT_HEAD = (0.86, 0.62, 0.60)
ART_FILL = (0.945, 0.945, 0.91)
SEQ_HEAD = (0.90, 0.94, 0.98)
SEQ_ACT = (0.72, 0.84, 0.94)
RETURN = (0.20, 0.46, 0.26)
SYNC = (0.12, 0.28, 0.55)
ASYNC = (0.50, 0.24, 0.10)
NOTE = (1.0, 0.99, 0.84)
CHART_FILL = (0.86, 0.94, 0.95)
CHART_HEAD = (0.52, 0.76, 0.82)


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


def package(ctx: cairo.Context, x: float, y: float, w: float, h: float, title: str, stereo: str) -> None:
    tab_w = max(180.0, text_w(ctx, stereo + "  " + title, 11) + 24)
    tab_h = 22
    rect(ctx, x, y, tab_w, tab_h, PKG_FILL, PKG_LINE, 1.2)
    rect(ctx, x, y + tab_h - 1, w, h - tab_h + 1, PKG_FILL, PKG_LINE, 1.2)
    draw_text(ctx, x + 8, y + 15, stereo, 9, italic=True, color=MUTED)
    draw_text(ctx, x + 8 + text_w(ctx, stereo + "  ", 9, italic=True), y + 15, title, 12, True)


def component_icon(ctx: cairo.Context, x: float, y: float, fill: tuple[float, float, float]) -> None:
    ctx.set_line_width(1.05)
    ctx.rectangle(x + 7, y + 6, 15, 13)
    set_color(ctx, fill)
    ctx.fill_preserve()
    set_color(ctx, INK)
    ctx.stroke()
    for oy in (8.5, 13.5):
        ctx.rectangle(x + 3, y + oy, 8, 3.4)
        set_color(ctx, (0.98, 0.98, 0.98))
        ctx.fill_preserve()
        set_color(ctx, INK)
        ctx.stroke()


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
    rect(ctx, x, y, w, h, fill, LINE, 1.25)
    ctx.rectangle(x, y, w, head_h)
    set_color(ctx, head)
    ctx.fill()
    set_color(ctx, LINE)
    ctx.set_line_width(1.25)
    ctx.move_to(x, y + head_h)
    ctx.line_to(x + w, y + head_h)
    ctx.stroke()
    ctx.rectangle(x, y, w, h)
    ctx.stroke()
    component_icon(ctx, x + 6, y + 8, fill)
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
    rect(ctx, x, y, w, h, fill, LINE, 1.05)
    ctx.rectangle(x, y, w, head_h)
    set_color(ctx, head)
    ctx.fill()
    set_color(ctx, LINE)
    ctx.set_line_width(1.05)
    ctx.move_to(x, y + head_h)
    ctx.line_to(x + w, y + head_h)
    ctx.stroke()
    ctx.rectangle(x, y, w, h)
    ctx.stroke()
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

    package(ctx, 16, 94, 1104, 1248, "terminal", "«system»")
    draw_text(
        ctx,
        32,
        136,
        "root CMakeLists.txt   add_subdirectory(libs/market-data)  apps/ingest  apps/terminal",
        10,
        color=MUTED,
    )

    # --- terminal ---
    component(ctx, 32, 148, 684, 572, "terminal", "«executable»  apps/terminal", EXEC_FILL, EXEC_HEAD)
    inner(ctx, 46, 196, 656, 508, "Application", "«composition»", CLASS_FILL, CLASS_HEAD, head_h=20)

    specs = [
        ("Window", "GlfwContext  ·  VkSurface"),
        ("VulkanContext", "instance / device / queue"),
        ("VulkanSwapchain", "render + present"),
        ("ImGuiLayer", "Theme  ·  docking  ·  ImPlot"),
    ]
    pw, ph, gap = 151, 50, 8
    px, py = 58, 224
    for i, (name, detail) in enumerate(specs):
        inner(ctx, px + i * (pw + gap), py, pw, ph, name, None, CLASS_FILL, CLASS_HEAD, [detail], head_h=20)

    inner(ctx, 58, 284, 632, 404, "Workspace", "«composition»", CLASS_FILL, CLASS_HEAD, head_h=20)
    draw_text(
        ctx,
        374,
        316,
        "Chart menu  ·  DockSpace  ·  DATA left 30%  ·  remainder = chart_dock_id",
        10,
        color=MUTED,
        align="center",
    )
    inner(ctx, 72, 324, 604, 118, "InventoryPanel   DATA", "«composition»", EXEC_FILL, EXEC_HEAD, head_h=20)
    inner(
        ctx,
        86,
        350,
        282,
        80,
        "Store  Reader",
        "«object»",
        CLASS_FILL,
        CLASS_HEAD,
        ["busy_timeout = 0  ·  coverage queries", "ctor: one-shot Writer migrate"],
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
        CLASS_FILL,
        CLASS_HEAD,
        ["Writer + CurlClient  ·  enqueue", "80 ms pace  ·  GUI polls Snapshot"],
        head_h=20,
    )
    inner(
        ctx,
        72,
        450,
        604,
        222,
        "CChartBook",
        "«composition»",
        CHART_FILL,
        CHART_HEAD,
        head_h=20,
    )
    inner(
        ctx,
        86,
        478,
        186,
        178,
        "Store  Reader",
        "«object»",
        CLASS_FILL,
        CLASS_HEAD,
        ["own GUI Reader", "busy_timeout = 0", "declared after DATA", "so migrate runs first"],
        head_h=20,
    )
    inner(
        ctx,
        284,
        478,
        376,
        178,
        "CChartPane  ×N",
        "«composition»",
        CHART_FILL,
        CHART_HEAD,
        [
            "CChartSettings  ·  Chart Settings modal",
            "loadChartBars  ·  queryBars 1m  ·  transformChartBars",
            "drawCandlesticks  ·  1m/5m/15m/1h/1d  ·  Days to Load",
        ],
        head_h=20,
    )
    inner(
        ctx,
        296,
        568,
        352,
        76,
        "studies_  /  computed_",
        "«pane state»  cap 16",
        CHART_FILL,
        CHART_HEAD,
        [
            "CStudyInstance  ·  SMA v1  ·  not CChartSettings",
            "computeStudies  ·  CStudySettings  ·  drawStudyOverlays",
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
        ["Store Writer  ·  CurlClient", "loadMboumApiKey  ·  ingestSymbol", "CLI: SYMBOL  [--from --to --db]", "progress: std::clog per session"],
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
            "Charting is GUI-only",
            "src/chart/  ·  CChartSettings header-only.",
            "Charts never call MBoum or CurlClient.",
            "DATA row select does not set pane symbol.",
            "Studies are pane memory, not SQLite.",
            "CStudy.h has no Bar and no ImGui.",
        ],
    )

    # --- apps/common ---
    component(
        ctx,
        32,
        736,
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
        782,
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
        782,
        504,
        52,
        "RepoRoot",
        None,
        CLASS_FILL,
        CLASS_HEAD,
        ["findRepoRoot()   defaultMarketDataDbPath()   defaultSecretsPath()"],
        head_h=20,
    )

    # --- market-data ---
    component(
        ctx,
        32,
        866,
        1072,
        448,
        "market-data",
        "«library»  libs/market-data   static   PUBLIC include = src/",
        LIB_FILL,
        LIB_HEAD,
    )
    draw_text(ctx, 48, 922, "public API", 10.5, True, color=MUTED)

    cells = [
        ("ingestSymbol()", "requires HttpGet  ·  NYSE walk; skip complete"),
        ("Store", "queryBars / coverage  ·  Reader/Writer  ·  WAL"),
        ("Secrets", "loadMboumApiKey(secrets.json  \"mboum\")"),
        ("Schema", "schemaV1()  ·  user_version 1  ·  frozen"),
        ("Time", "IANA tzdb  ·  US RTH UTC windows"),
        ("NyseCalendar", "holidays  ·  observed New Year  ·  open?"),
        ("MboumJson", "v3 historical URL  ·  parse page"),
        ("MboumMap", "mapV3Bar  ·  RTH filter  ·  drop forming"),
    ]
    cw, ch = 256, 68
    for i, (name, detail) in enumerate(cells):
        col, row = i % 4, i // 4
        inner(
            ctx,
            48 + col * (cw + 8),
            930 + row * (ch + 8),
            cw,
            ch,
            name,
            None,
            CLASS_FILL,
            CLASS_HEAD,
            [detail],
            head_h=22,
        )

    draw_text(ctx, 48, 1094, "private  (PRIVATE include dir — apps never see sqlite3.h)", 10.5, True, color=MUTED)
    inner(
        ctx,
        48,
        1104,
        254,
        64,
        "SqliteDb / Stmt / Txn",
        "«internal»",
        PRIV_FILL,
        PRIV_HEAD,
        ["private/  ·  no sqlite3.h to apps"],
        head_h=22,
    )
    inner(
        ctx,
        310,
        1104,
        254,
        64,
        "terminal_sqlite3",
        "«library» PRIVATE",
        PRIV_FILL,
        PRIV_HEAD,
        ["deps/sqlite  ·  THREADSAFE=1"],
        head_h=22,
    )
    inner(
        ctx,
        572,
        1104,
        254,
        64,
        "schema/v1.sql",
        "«artifact» embedded",
        ART_FILL,
        PRIV_HEAD,
        ["instrument  bar  coverage_day  CA"],
        head_h=22,
    )
    inner(
        ctx,
        834,
        1104,
        254,
        64,
        "Types.h",
        None,
        CLASS_FILL,
        CLASS_HEAD,
        ["Bar  Coverage*  Instrument"],
        head_h=22,
    )
    draw_text(
        ctx,
        48,
        1186,
        "Store write APIs open one BEGIN IMMEDIATE.  ingestSession is the only txn and calls unlocked helpers.  Nested SqliteTxn throws.",
        10,
        color=MUTED,
    )
    draw_text(
        ctx,
        48,
        1202,
        "Charts read only (queryBars).  Studies are not Store rows.  Schema v1 stays frozen.  libs/market-data does not depend on ImGui or ImPlot.",
        10,
        color=MUTED,
    )

    # --- environment ---
    package(ctx, 1248, 94, 532, 1248, "Environment", "«external»")

    inner(ctx, 1266, 132, 496, 70, "Operator", "«actor»", ART_FILL, PRIV_HEAD, ["GO in DATA  ·  Chart menu  ·  ingest CLI"], head_h=22)

    ext = [
        (218, "GLFW 3.3", "«library»  window / Vulkan surface", EXT_FILL, EXT_HEAD),
        (280, "Vulkan", "«library»  instance, device, swapchain", EXT_FILL, EXT_HEAD),
        (342, "Dear ImGui", "«library»  deps/imgui  ·  docking", EXT_FILL, EXT_HEAD),
        (404, "ImPlot v1.0", "«library»  deps/implot  ·  no PlotCandlestick", CHART_FILL, CHART_HEAD),
        (466, "libcurl", "«library»  CURL::libcurl", EXT_FILL, EXT_HEAD),
        (542, "api.mboum.com", "«service»  GET /v3/markets/historical", EXT_FILL, EXT_HEAD),
        (618, "secrets.json", "«artifact»  gitignored  ·  key mboum", ART_FILL, PRIV_HEAD),
        (694, "data/market-data.sqlite", "«artifact»  WAL + SHM  ·  schema v1", ART_FILL, PRIV_HEAD),
        (786, "Catch2 tests", "chart_load / view / transform / study  ·  market_data", CLASS_FILL, CLASS_HEAD),
        (848, "clang-tidy", "first-party TUs  ·  warnings as errors", PRIV_FILL, PRIV_HEAD),
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
        (569, "HTTPS"),
        (645, "Secrets"),
        (721, "Store"),
    ]
    for ay, label in arrows:
        dashed = label != "HTTPS"
        color = ASYNC if label == "HTTPS" else SYNC
        line_arrow(ctx, gutter_x1, ay, gutter_x2, ay, color, dashed=dashed)
        draw_text(ctx, (gutter_x1 + gutter_x2) / 2, ay - 5, label, 8.5, italic=True, color=color, align="center")

    note_box(
        ctx,
        1266,
        916,
        496,
        400,
        [
            "Concurrency (K19) + charts (D4, D10)",
            "One Writer connection, N Readers.",
            "Never share sqlite3* across threads.",
            "GUI Readers: InventoryPanel + CChartBook.",
            "busy_timeout=0: keep last snapshot unless",
            "the error is not busy/locked.",
            "Worker / CLI Writer busy_timeout=5000.",
            "Chart load is GUI-thread, sync, ~2 s poll.",
            "Keep last bars on Busy only if settings match.",
            "",
            "Charts never talk to MBoum.",
            "DATA row select does not set chart symbol.",
            "HTTP is in-process libcurl, not a fork.",
            "Candles and study overlays are custom",
            "GetPlotDrawList() draws.  Index X.",
            "computeStudies is GUI-thread, never throws.",
            "Busy keep-candles does not recompute studies.",
            "Apply/OK recomputes without loadChartBars.",
        ],
    )


def draw_lifeline_head(ctx: cairo.Context, x: float, y: float, w: float, name: str) -> None:
    inner(ctx, x - w / 2, y, w, 34, name, None, SEQ_HEAD, CLASS_HEAD, head_h=34, name_size=10.5)


def draw_figure2(ctx: cairo.Context) -> None:
    y0 = 1370
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
    call(3, 5, y[3], "4  ensureInstrument  (fail if symbol matches >1)")
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
            "Readers: InventoryPanel + CChartBook (GUI thread).",
            "InventoryPanel ctor opens a one-shot Writer to migrate.",
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
            "Filled arrow = synchronous call.   Open dashed = return / async.",
            "Ball = provided interface.   Socket = required interface.",
            "Nested boxes = UML composition.   Dashed «use» = dependency.",
        ],
    )


def draw_figure3(ctx: cairo.Context) -> None:
    y = 2280
    draw_text(ctx, 28, y, "Figure 3.  GUI frame  (sequence)  —  Application::run", 13.5, True)

    names = [
        "Application",
        "Window",
        "ImGuiLayer",
        "Workspace",
        "InventoryPanel",
        "CChartBook",
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
    activation(4, m[4] - 8, m[4] + 12)
    activation(5, m[3] - 8, m[5] + 12)
    activation(2, m[6] - 8, m[7] + 12)
    activation(6, m[7] - 8, m[7] + 12)

    call(0, 1, m[0], "1  pollEvents()")
    call(0, 2, m[1], "2  newFrame()")
    call(0, 3, m[2], "3  draw()")
    call(3, 5, m[3], "4  drawMenu()  Chart >> New / Settings / Studies / Close")
    call(3, 4, m[4], "5  inventory.draw()  ·  pollWorker")
    call(3, 5, m[5], "6  charts.draw(chart_dock_id)")
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
        "CMake: terminal compiles CChart* plus CStudyCompute / CStudySettings / CStudyPlot and deps/implot (not implot_demo).  terminal_tests compile CChartLoad.cpp, CChartTransform.cpp, and CStudyCompute.cpp (no ImGui).",
        10.5,
        color=MUTED,
    )
    draw_text(
        ctx,
        28,
        y + 426,
        "Default ingest FROM/TO is the last 14 calendar days.  Chart Days to Load default is 14 sessions with bar_count > 0.  Grain is 1-minute as-traded OHLCV.",
        10.5,
        color=MUTED,
    )


def draw_figure4(ctx: cairo.Context) -> None:
    y0 = 2730
    draw_text(ctx, 28, y0, "Figure 4.  Chart load and draw  (sequence)", 13.5, True)
    draw_text(
        ctx,
        28,
        y0 + 18,
        "Day1 queryBars(id, 86400) when daily coverage exists; else 1m + transformChartBars.  Studies Apply/OK recomputes without assigning loaded_.  No MBoum.  Studies are not stored.",
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
    call(3, 4, y[4], "5  findInstrumentsBySymbol  ·  queryCoverageDays  ·  queryBars 1m")
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
            "ImGui id chart_N so dock layout survives symbol changes.",
            "CChartSettings are not persisted.  imgui.ini is gitignored.",
            "Studies are pane state.  They are not persisted.",
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
    y0 = 3340
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
            "v1 moving average",
            "Kind = Moving Average.  Method locked to Simple.",
            "Source Open/High/Low/Close.  Length 1..10000, default 20 C.",
            "EMA/WMA and Subgraph are reserved on the same types.",
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
            "CStudy.h is free of Bar and ImGui.  Colors: kAccent/kWarn/kOk/kDanger.",
            "Close Chart destroys the list.  A new pane starts empty.",
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


def paint(ctx: cairo.Context) -> None:
    set_color(ctx, BG)
    ctx.paint()
    draw_figure1(ctx)
    draw_figure2(ctx)
    draw_figure3(ctx)
    draw_figure4(ctx)
    draw_figure5(ctx)


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
