# Stratum theme for terminal

The terminal uses the Stratum dark palette: slate surfaces, cool blue accent, muted status colors, no decorative chrome. Hex values are copied from `kPalDark` in the CyNickal Software Monorepo (`shared/GUI/StratumPalette.h`). That header is the source of truth for every CyNickal product and for the website stylesheet. Do not invent a second palette here.

The terminal is a data workstation. It keeps Stratum's colors and uses tighter metrics than the product shell: corner radius stays 0, padding stays tight, type stays small. From across the room the screen should read as cool blue on dark slate.

---

## 1. Feeling

| Principle | Meaning in this app |
|---|---|
| Function over form | Color encodes data, focus, or action. Nothing is decorative. |
| Cool blue is the accent | `#6f97c9` marks focus, links, the crosshair, selection, and the primary button. Body text stays steel (`#ccd3dd`). |
| Density | Tight padding, 13 px type, many panels at once. No luxury whitespace. |
| Square | Radius 0. Hairline borders. No pills, no shadows, no gradients. |
| Quiet status | Up and down are Stratum's muted green and brick, used only for direction and errors. |

The terminal ships the dark palette only. Stratum's light palette stays in the monorepo.

---

## 2. Color tokens

Names in code are `Theme::kBg0`, `Theme::kAccent`, and so on. `FromRgb` / `WithAlpha` / `Mix` build the `ImVec4`s. Values below are the dark palette only.

### Surfaces

| Token | Hex | Use |
|---|---|---|
| `kBg0` | `#0d1116` | Clear color, empty dock, title bars, fields, plot background |
| `kBg1` | `#12171e` | Window body, menu bar |
| `kBg2` | `#171d26` | Child panels, default buttons, table headers |
| `kBg3` | `#1e2530` | Hover and active frames, popups |
| `kLine` | `#262e3a` | Borders, separators, plot grid |
| `kLine2` | `#313848` | Stronger hairline, pressed default button, scrollbar grab |

### Text

| Token | Hex | Use |
|---|---|---|
| `kText` | `#ccd3dd` | Default widget text |
| `kTextDim` | `#828c9b` | Secondary labels, axis text, disabled-looking status |
| `kTextFaint` | `#586273` | Disabled widgets |

`kBg0` is also the label color on a solid accent button. Steel text on `#6f97c9` does not pass contrast.

### Accent and status

| Token | Hex | Stratum field | Use |
|---|---|---|---|
| `kAccent` | `#6f97c9` | `acc` | Focus, links, crosshair, slider, primary button |
| `kWarn` | `#c08552` | `amber` | Partial coverage and other warnings. Not a text color. |
| `kDanger` | `#b5544e` | `danger` | Price down, errors, Cancel |
| `kOk` | `#5f8a63` | `ok` | Price up, complete coverage |

Accent wash is `kAccent` at alpha 0.16, matching `accbgAlpha` in `kPalDark`. `kAccentHover` lifts the accent 18% toward white. `kAccentPressed` mixes it 22% toward `kBg0`.

### Workstation aliases

Panels and charts keep short role names. They are the same values, not extra hues.

| Alias | Token | Role |
|---|---|---|
| `kCanvas` | `kBg0` | Swapchain clear, empty dock |
| `kPanel` | `kBg2` | Child fill |
| `kField` | `kBg0` | Inputs |
| `kHairline` | `kLine` | Borders and grid |
| `kMuted` | `kTextDim` | Secondary copy |
| `kUp` | `kOk` | Up candles and complete rows |
| `kDown` | `kDanger` | Down candles and errors |
| `kGo` | `kAccent` | GO and OK |
| `kCancel` | `kDanger` | Cancel label |

---

## 3. How color is assigned

1. **Steel** — ordinary information. Labels, quotes, table text.
2. **Dim / faint** — units, timestamps, empty states, disabled controls.
3. **Accent blue** — “look here”: selection, crosshair tags, the active tab overline, the primary button, links.
4. **Green / brick** — direction and health only. Not chrome.
5. **Warn** — partial or caution. One hue, used rarely.
6. **Slate steps** — `kBg0` through `kBg3` separate wells, windows, cards, and hover. No extra grays.

Do not add hues. The working set is the twelve Stratum fields above.

### Panel anatomy

| Band | Token | ImGui |
|---|---|---|
| Title and empty dock | `kBg0` | `TitleBg`, `TitleBgActive`, `DockingEmptyBg` |
| Window body | `kBg1` | `WindowBg`, `MenuBarBg` |
| Child / card | `kBg2` | `ChildBg`, default `Button`, `TableHeaderBg` |
| Hover | `kBg3` | `FrameBgHovered`, `ButtonHovered`, `TabHovered` |
| Focus | `kAccent` | selected-tab overline, cursor, docking preview |

Active titles are the same slate as inactive titles. The accent overline is the only “this tab is selected” mark.

---

## 4. ImGui style

`Theme::ApplyStratumStyle` runs from `ImGuiLayer` after `ImGui::CreateContext()`, instead of `StyleColorsDark`. `style.ScaleAllSizes(main_scale)` stays after it.

### Metrics

```
WindowRounding = ChildRounding = FrameRounding = PopupRounding = 0
ScrollbarRounding = GrabRounding = TabRounding = 0

WindowBorderSize = ChildBorderSize = PopupBorderSize = 1
FrameBorderSize = TabBorderSize = 0

WindowPadding     = (6, 4)
FramePadding      = (6, 3)
ItemSpacing       = (6, 4)
ItemInnerSpacing  = (4, 3)
CellPadding       = (4, 2)
IndentSpacing     = 12
ScrollbarSize     = 12
GrabMinSize       = 8

WindowTitleAlign  = (0, 0.5)
```

Viewports keep `WindowRounding = 0` and an opaque window background.

### `ImGuiCol_*`

| ImGui color | Token |
|---|---|
| `Text` | `kText` |
| `TextDisabled` | `kTextFaint` |
| `WindowBg` | `kBg1` |
| `ChildBg` | `kBg2` |
| `PopupBg` | `kBg3` |
| `Border` | `kLine` |
| `FrameBg` | `kBg0` |
| `FrameBgHovered` / `FrameBgActive` | `kBg3` |
| `TitleBg` / `TitleBgActive` / `TitleBgCollapsed` | `kBg0` |
| `MenuBarBg` | `kBg1` |
| `ScrollbarBg` | `kBg0` |
| `ScrollbarGrab` | `kLine2` |
| `ScrollbarGrabHovered` | `kTextDim` |
| `ScrollbarGrabActive` | `kAccent` |
| `CheckMark` | `kBg0` |
| `CheckboxSelectedBg` | `kAccent` |
| `SliderGrab` | `kAccent` |
| `SliderGrabActive` | `kText` |
| `Button` / `Hovered` / `Active` | `kBg2` / `kBg3` / `kLine2` |
| `Header` / `Hovered` / `Active` | accent at 0.16 / 0.24 / 0.32 |
| `Separator` / `Hovered` | `kLine` / `kLine2` |
| `SeparatorActive` | `kAccent` |
| `Tab` / `TabDimmed` | `kBg0` |
| `TabSelected` / `TabDimmedSelected` | `kBg1` |
| `TabSelectedOverline` | `kAccent` |
| `DockingEmptyBg` | `kBg0` |
| `DockingPreview` | accent at 0.35 |
| `PlotLines` | `kAccent` |
| `PlotHistogram` | `kOk` |
| `TableHeaderBg` | `kBg2` |
| `TableBorderStrong` | `kLine` |
| `TableBorderLight` | `kBg3` |
| `TableRowBg` | transparent |
| `TableRowBgAlt` | `kBg0` |
| `TextLink` | `kAccent` |
| `TextSelectedBg` | accent at 0.28 |
| `NavCursor` | `kAccent` |
| `UnsavedMarker` | `kWarn` |

Primary buttons (GO, OK) push `kGo` / `kAccentHover` / `kAccentPressed` and `kBg0` text. Cancel stays a default button with `kCancel` text. Other buttons stay on slate.

### Plots

`ImPlot::StyleColorsAuto()` runs after the ImGui style, then:

| ImPlot color | Token |
|---|---|
| `PlotBg` | `kBg0` |
| `FrameBg` | `kBg1` |
| `PlotBorder` | `kLine` |
| `AxisText` | `kTextDim` |
| `AxisGrid` | `kLine` |
| `Crosshairs` | `kAccent` |

Candles: `kUp` when close ≥ open, `kDown` otherwise. Axis tags use `kAccent`.

### Clear color

`Workspace::clearColor()` is `kCanvas` (`kBg0`). Empty dock space uses the same well.

---

## 5. Typography

Body is 13 px. No display sizes. Numeric columns stay right-aligned.

`LoadFonts` takes the first installed face, in order: Noto Sans, Liberation Sans, DejaVu Sans, Ubuntu, with a matching mono for figures. Stratum's product shell ships IBM Plex; this app does not vendor a face.

---

## 6. Layout and motion

Docking is the layout system. DATA is the left 30% on first run. Tables, not cards. Hairline rules, not gutters.

Hover swaps color immediately. No eased motion. Scrollbars stay thin.

---

## 7. Where it lives

| File | Role |
|---|---|
| `apps/terminal/src/ui/Theme.h` | Tokens |
| `apps/terminal/src/ui/Theme.cpp` | `ApplyStratumStyle`, font load |
| `apps/terminal/src/ui/ImGuiLayer.cpp` | Applies the style and the ImPlot overrides |

Do not restyle `deps/imgui/`.

The monorepo palette is `shared/GUI/StratumPalette.h` (`kPalDark` / `kPalLight`). After a palette change there, update these tokens to the new dark values. The website block is regenerated by `Website/tools/gen-stratum-palette.py`; the terminal is not.

---

## 8. Acceptance

1. The window, empty dock, and plot are `#0d1116`.
2. Default text is `#ccd3dd`.
3. Focus, the crosshair, selection, and GO/OK are `#6f97c9`. Title bars are slate, with an accent overline on the selected tab.
4. Corner radius is 0.
5. Padding is tight.
6. Green and brick appear only on direction, coverage health, errors, and Cancel.
7. From a distance the screen is cool slate-blue.
