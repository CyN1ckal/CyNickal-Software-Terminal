# Bloomberg Terminal theme for MyApp

Transfer the **look and feel** of the Bloomberg Terminal into this Dear ImGui application: black canvas, amber identity color, loud semantic greens and reds, extreme information density, no decorative chrome.

This is a visual-language spec for MyApp, not a clone of Bloomberg’s product, fonts, or functions. Bloomberg names the roles (amber text, black screen, green up, red down, red function toolbar) but **does not publish Terminal hex/RGB**. Official language is **amber**, not orange. Hex values in this file are working tokens for MyApp: brand scrapes, screenshot samples, and CVD chips — not a factory spec. Terminal users can also customize colors, so any one screenshot may differ from default.

MyApp today uses stock `ImGui::StyleColorsDark()` in `src/ImGuiLayer.cpp` and the example clear color `(0.45, 0.55, 0.60)` in `src/DemoUi.cpp`. Both fight the Terminal look. Docking and multi-viewport are already enabled, which is the right skeleton for a multi-panel workstation.

---

## 1. Feeling to hit

From across the room the app should read as **amber light on black**. Up close it should feel like a working instrument, not a consumer dashboard.

| Principle | Meaning in this app |
|---|---|
| Function over form | Every color encodes data or action. Nothing is decorative. |
| High contrast | One number must pop out of a wall of numbers. |
| Amber is identity | Amber is the default font / label color, protected as the house color. |
| Density | Tight padding, small type, many panels visible at once. No luxury whitespace. |
| Square | Radius 0. Hairline borders. No pills, no shadows, no gradients. |
| Instant | State changes snap. No eased motion. Optional 1-frame flash on data update. |

Walk-up test: if the window is recognizable as “that amber-and-black app” from several meters away, the theme is working.

### What this is not

- Not bloomberg.com. The consumer site used a black-and-amber look for only about four years and dropped it in 2010 for black text on white; that was never treated as the website’s heritage. Do not mix the two palettes.
- Not navy-noir “fintech dark mode” (`#0A0E1A` and similar). Real Terminal screens are **true black**.
- Not a CRT phosphor simulation (scanlines, glow, green-on-black). Modern Terminal is sharp TrueType on black.
- Not a monospace-only aesthetic. Bloomberg uses a custom proportional face with tabular numerals. We approximate that; we do not ship Bloomberg Prop.

---

## 2. Color tokens

Use these names in code (`Theme::kAmber`, etc.). Convert with `IM_COL32` / `ImVec4`.

### Surfaces

| Token | Hex | ImVec4 | Use |
|---|---|---|---|
| `Canvas` | `#000000` | `0, 0, 0, 1` | Swapchain clear, `WindowBg`, docking empty bg |
| `Panel` | `#0A0A0A` | `0.039, 0.039, 0.039, 1` | Child windows, table body |
| `Chrome` | `#1C1C1C` | `0.110, 0.110, 0.110, 1` | Grey mnemonic bar, inactive tabs, frames at rest |
| `ChromeHover` | `#2C2C2C` | `0.173, 0.173, 0.173, 1` | Hovered frames, inactive chrome |
| `Toolbar` | `#5A1C20` | `0.353, 0.110, 0.125, 1` | Red function toolbar (title, drop-downs, key tasks) |
| `Field` | `#1A1208` | `0.102, 0.071, 0.031, 1` | Editable fields (amber-tinted, not grey) |
| `Hairline` | `#3C3C3C` | `0.235, 0.235, 0.235, 1` | Borders, separators, table lines |

### Text

| Token | Hex | ImVec4 | Use |
|---|---|---|---|
| `Amber` | `#FFA028` | `1.000, 0.627, 0.157, 1` | Default text, labels, headings, identity |
| `Ink` | `#FFFFFF` | `1, 1, 1, 1` | Body copy when amber would be too loud; selected values |
| `Muted` | `#8C8C8C` | `0.549, 0.549, 0.549, 1` | Disabled, secondary, units, timestamps |
| `Highlight` | `#FCBC14` | `0.988, 0.737, 0.078, 1` | Selection, focused tab, “look here” |

`Amber` `#FFA028` is the MyApp working value (Bloomberg brand “Sunshade”). It sits next to the official CVD chip labeled “Bloomberg Default” (`#FCA42C`). Sampled Terminal-UI recreations often run darker/ochre (`#D39000`, `#F39000`, Berg `#F49F31`). Use `#FFA028` as the single source of truth in code so widgets cannot drift. Do not use consumer-site orange `#F05143`.

### Semantic (data)

| Token | Hex | ImVec4 | Meaning |
|---|---|---|---|
| `Up` | `#04841C` | `0.016, 0.518, 0.110, 1` | Positive / buy / price up |
| `Down` | `#A41C2C` | `0.643, 0.110, 0.173, 1` | Negative / sell / price down |
| `Series` | `#4DC7F9` | `0.302, 0.780, 0.976, 1` | Extra chart series, links, cross-highlight |
| `HeatUp` | `#044C0C` | `0.016, 0.298, 0.047, 1` | Green heatmap cell |
| `HeatDown` | `#7C0C24` | `0.486, 0.047, 0.141, 1` | Red heatmap cell |

These greens and reds are taken from official Launchpad / function screenshots. They are darker and more crimson than typical “trading green/red” (`#00FF00` / `#FF0000`). Do not neon them.

### Action (keyboard-inspired)

The Bloomberg keyboard is part of the color language. Map the same roles onto ImGui controls:

| Token | Hex | Role | ImGui use |
|---|---|---|---|
| `Go` | `#1C8C28` | Green action key (`GO`) | Primary button, confirm |
| `Cancel` | `#C41428` | Red stop key (`CANCEL`) | Destructive button, close |
| `Sector` | `#F0C400` | Yellow market-sector keys | Mode / asset-class chips |
| `PanelKey` | `#2A7FD4` | Blue `PANEL` key | Panel / docking affordance |

Default buttons stay on `Chrome` with `Amber` text. Colored action keys are for **explicit** GO / Cancel / sector controls, not every widget.

### CVD variants (optional later)

Bloomberg ships Terminal-wide schemes via `PDFU COLORS <GO>` (Deuteranopia and Protanomaly). Support there is partial: not every function, and Buy/Sell buttons may not follow. If MyApp adds a color-vision mode, keep amber for non-semantic text and swap only up/down:

| Mode | Up | Down | Default |
|---|---|---|---|
| Default | `Up` green | `Down` red | `Amber` |
| Deuteranopia | `#048CEC` | `#CC4C4C` | `Amber` |
| Protanomaly | `#048CEC` | `#FC5C2C` | `Highlight` gold |

---

## 3. How color is assigned

Color is a data encoding, not a skin.

1. **Amber** — “this is ordinary information” and “this is MyApp.” Labels, field names, default quotes, table headers. Editable fields are amber-tinted, not grey boxes.
2. **White** — long body text and values that would vibrate if everything were amber.
3. **Green / red (data)** — direction only: up/buy vs down/sell, including net-change columns and Launchpad heatmaps.
4. **Red (chrome)** — the function toolbar is red. That is chrome, not sentiment. Do not also paint every header red.
5. **Grey** — mnemonic / secondary toolbar, inactive chrome, everything that can be ignored.
6. **Cyan / blue** — a second encoding: extra series, hyperlinks, panel focus. Official Terminal docs do not name a primary-blue default; do not use `#0000FF`.
7. **Yellow / gold** — selection, focused tab, and keyboard-style sector chips.

Do not introduce extra hues. The working set is black, grey, white, amber, red, green, yellow, cyan/blue. Occasional magenta is allowed for a third series; it is not a brand color.

### Function-panel anatomy

Official Terminal help names the chrome of a function screen (no hex, named colors only). Map that onto each docked ImGui window:

| Band | Official role | MyApp token | ImGui |
|---|---|---|---|
| Top strip | Red function toolbar: title, drop-downs, key tasks | `Toolbar` | `TitleBgActive`, selected tab, window menu |
| Editables | Amber fields | `Field` + `Amber` text | `FrameBg`, inputs, combo boxes |
| Secondary strip | Grey mnemonic toolbar | `Chrome` | `MenuBarBg`, inactive tabs |
| Body | Black canvas, amber labels, green/red signed values | `Canvas` / `Amber` / `Up` / `Down` | window contents, tables, plots |

---

## 4. ImGui style mapping

Apply after `ImGui::CreateContext()`, **instead of** `ImGui::StyleColorsDark()`. Suggested home: `ImGuiLayer` constructor (where style is already scaled for DPI). Keep `style.ScaleAllSizes(main_scale)` after these assignments.

### Metrics

Terminals do not round. Density is the product.

```
style.WindowRounding    = 0
style.ChildRounding     = 0
style.FrameRounding     = 0
style.PopupRounding     = 0
style.ScrollbarRounding = 0
style.GrabRounding      = 0
style.TabRounding       = 0

style.WindowBorderSize  = 1
style.ChildBorderSize   = 1
style.FrameBorderSize   = 0          // frames sit on Chrome, not outlined
style.PopupBorderSize   = 1
style.TabBorderSize     = 0

style.WindowPadding     = (6, 4)
style.FramePadding      = (6, 3)
style.ItemSpacing       = (6, 4)
style.ItemInnerSpacing  = (4, 3)
style.CellPadding       = (4, 2)
style.IndentSpacing     = 12
style.ScrollbarSize     = 12
style.GrabMinSize       = 8

style.WindowTitleAlign  = (0, 0.5)   // left, like a function header
style.DisplaySafeAreaPadding = (0, 0)
```

Viewports: keep `WindowRounding = 0` and `WindowBg.w = 1` (already done).

### `ImGuiCol_*` assignments

| ImGui color | Token |
|---|---|
| `Text` | `Amber` |
| `TextDisabled` | `Muted` |
| `WindowBg` | `Canvas` |
| `ChildBg` | `Panel` |
| `PopupBg` | `Canvas` |
| `Border` | `Hairline` |
| `BorderShadow` | transparent |
| `FrameBg` | `Field` (amber-tinted editable) |
| `FrameBgHovered` | slightly lighter `Field` |
| `FrameBgActive` | slightly lighter `Field` |
| `TitleBg` | `Canvas` |
| `TitleBgActive` | `Toolbar` (red function toolbar) |
| `TitleBgCollapsed` | `Canvas` |
| `MenuBarBg` | `Chrome` (grey mnemonic bar) |
| `ScrollbarBg` | `Canvas` |
| `ScrollbarGrab` | `ChromeHover` |
| `CheckMark` | `Amber` |
| `SliderGrab` | `Amber` |
| `SliderGrabActive` | `Highlight` |
| `Button` | `Chrome` |
| `ButtonHovered` | `ChromeHover` |
| `ButtonActive` | `#3A3A3A` |
| `Header` / `HeaderHovered` / `HeaderActive` | `Chrome` / `ChromeHover` / `Toolbar` |
| `Separator*` | `Hairline` |
| `Tab` | `Chrome` |
| `TabHovered` | `Highlight` at ~0.35 alpha on black |
| `TabSelected` / `TabActive` | `Toolbar` |
| `TabDimmed` | `Canvas` |
| `DockingEmptyBg` | `Canvas` |
| `DockingPreview` | `Amber` at 0.25 alpha |
| `PlotLines` | `Amber` |
| `PlotLinesHovered` | `Series` |
| `PlotHistogram` | `Up` |
| `PlotHistogramHovered` | `Highlight` |
| `TableHeaderBg` | `Chrome` |
| `TableBorderStrong` | `Hairline` |
| `TableBorderLight` | `#2A2A2A` |
| `TableRowBg` | transparent |
| `TableRowBgAlt` | `#0C0C0C` (barely there) |
| `TextSelectedBg` | `Amber` at 0.35 alpha |
| `NavHighlight` | `Series` |
| `ModalWindowDimBg` | black 0.60 alpha |

Active title bars and the selected tab use the **red function toolbar**, not grey and not ImGui blue. Inactive titles stay black. Inputs sit on `Field` (dark amber), not on `Chrome` grey.

### Swapchain clear

`DemoUi::clear_color_` must become `Canvas` black, not the example gray-blue. Docked empty space and the OS window around panels should be the same black.

---

## 5. Typography

Bloomberg commissioned **Bloomberg Prop Unicode** (Matthew Carter, 2007): proportional + mono, tabular figures, finance fractions. We cannot ship that font.

Approximation, in order of preference:

1. A humanist sans with tabular lining figures, 12–14 px UI (e.g. IBM Plex Sans, Source Sans 3, Inter with `tnum`).
2. Pair a compact sans for labels with a tabular mono for numeric columns (IBM Plex Mono / JetBrains Mono at 13 px).
3. If only Proggy/ImGui default is available, still apply the color theme; density will suffer.

Rules:

- Body 13 px, never 16+ for working text.
- No display / hero sizes.
- Numeric columns right-aligned, same advance for `0–9`.
- Font atlas should include the glyphs we actually plot (currency, arrows, en-dash).

Load fonts in `ImGuiLayer` after context creation, before the first frame. Keep DPI scaling (`FontScaleDpi`, `ConfigDpiScaleFonts`) as it is.

---

## 6. Layout

Bloomberg’s workstation is a grid of function panels. MyApp already has docking; use it as the layout system.

- Default to a docked workspace filling the viewport. No floating demo windows as the primary UI.
- Multiple panels visible at once (4 is the historical Terminal; more is fine).
- Each panel: command/title strip on top, dense body, status on the bottom edge if needed.
- Tables, not cards. Hairline column rules, not whitespace gutters.
- Plot windows sit on the same black; series colors are `Amber`, `Series`, `Up`, `Down` — never a rainbow.

Avoid: rounded child windows, large hero headers, empty marketing space, illustrations, drop shadows, gradient fills.

---

## 7. Motion and feedback

- No eased animations.
- Hover is an instant color swap (`Chrome` → `ChromeHover`).
- Live values may flash `Highlight` for one frame on change, then return.
- Scrollbars are thin and quiet; they are not a design element.

---

## 8. Semantic helpers

Theme colors on `ImGuiCol_Text` handle 90% of the chrome. Data still needs explicit pushes:

```cpp
ImGui::TextColored(Theme::kUp(),   "+1.06%%");
ImGui::TextColored(Theme::kDown(), "-0.45%%");
ImGui::TextColored(Theme::kMuted(), "USD");
```

Heatmap cells: `ImGui::TableSetBgColor` with `HeatUp` / `HeatDown`.
Primary vs destructive buttons: `PushStyleColor` on `Button*` with `Go` / `Cancel`, text `Ink`.

Do not color-code meaning with amber vs white. Amber vs white is hierarchy (label vs value), not sentiment.

---

## 9. Implementation notes (this repo)

Suggested shape, when the theme is applied:

| File | Change |
|---|---|
| `src/Theme.h` (new) | Tokens as `constexpr ImVec4`, plus `ApplyBloombergStyle(ImGuiStyle&)` |
| `src/ImGuiLayer.cpp` | Call `ApplyBloombergStyle` instead of `StyleColorsDark`; optional font load |
| `src/DemoUi.cpp` | `clear_color_` = canvas black; demo content can stay until real panels exist |
| `CMakeLists.txt` | Add `src/Theme.cpp` if the apply function is not header-only |

Keep tokens in one header so plots, tables, and buttons cannot drift.

Do not restyle inside `deps/imgui/` sources. The bundled Dear ImGui tree stays upstream-clean.

---

## 10. Acceptance checks

The theme is right when all of the following hold:

1. The OS window and empty dock space are black, not gray-blue.
2. Default widget text is amber, not ImGui’s light gray.
3. Active title bar is the red function toolbar, not blue-gray. Inputs sit on a dark amber field, not a grey box.
4. Corner radius is 0 on windows, frames, tabs, and grabs.
5. Padding is tight enough that the stock demo looks slightly cramped — that is correct.
6. Green and red appear only on signed values / heatmaps / explicit GO-Cancel, never on chrome.
7. From a distance, the screen is amber-on-black, not “generic dark ImGui.”

---

## 11. Sources

- Ali Jeffery (Visual Design Lead) and Fahd Arshad (UX), [Bloomberg’s customer-centric design ethos](https://www.bloomberg.com/company/stories/bloombergs-customer-centric-design-ethos/) — black and amber as hallmark; amber as base font color; yellow keyboard keys as identity.
- Bloomberg, [Design At Bloomberg](https://www.youtube.com/watch?v=4-Mg2joHJZ8) — early monitors offered green, white, or amber; amber became the foreground because the text was clear.
- Bloomberg UX, [Designing the Terminal for Color Accessibility](https://www.bloomberg.com/company/stories/designing-the-terminal-for-color-accessibility/) — green = up, red = down; CVD blue/red schemes; amber retained for non-semantic data.
- Bloomberg Professional help (`STOP <GO>` / Cancel as a function) — named chrome: red function toolbar, amber editable fields, grey mnemonic toolbar; keyboard yellow / green / red / blue.
- Bloomberg, [How Terminal UX designers conceal complexity](https://www.bloomberg.com/company/stories/how-bloomberg-terminal-ux-designers-conceal-complexity/) — 2007 Matthew Carter font; users rejected a color shift; team toned the new color down.
- [Wikipedia: Bloomberg Terminal](https://en.wikipedia.org/wiki/Bloomberg_Terminal) — color edition 1991.
- Fast Company / archive, [How the Bloomberg Terminal Made History](https://web.archive.org/web/20200727012631/https://www.fastcompany.com/3051883/the-bloomberg-terminal) — default remains amber characters on black.
- Business Insider, [Bloomberg redesigns website, 2010](https://www.businessinsider.com/bloomberg-site-redesign-2010-4) — consumer site dropped black-and-amber; not the website’s heritage.
- John Cabot University, [Color Scheme Options for Bloomberg](https://johncabot.libguides.com/bloomberg/color-scheme) — `PDFU COLORS` coverage limits.

Working hex (not official Terminal RGB): brand scrape `#FFA028`; CVD-chip sample `#FCA42C`; vim-bloomberg sampled UI `#D39000` / `#F39000` / `#0B85DF`; Berg `#F49F31` on `#000000`. Navy `#0A0E1A` palettes are fan approximations and are not used here.
