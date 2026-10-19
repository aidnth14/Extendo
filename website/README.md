# Console UI Taste — Custom Interactive Website

This website is a custom, fully functional implementation of the design principles detailed in [`console_ui_taste.md`](../console_ui_taste.md).

## What's Included

### 1. Interactive 3DS Console Dashboard
- **Horizontal Game Shelf**: Carousel of 12 classic 3DS titles with authentic vector cover art, ratings, playtime, and descriptions.
- **Rule 1 Focus Effect**: Hover or select any tile to experience **Scale (1.06x) + Luminance Inversion** (card flips to solid white with dark text, zero outlines).
- **Rule 6 Dock Bar**: Floating pill dock featuring perfect circular buttons with monochrome line art that invert to solid white when active.
- **Rule 7 Quiet Status Cluster**: Live 12-hour clock, uniform-stroke Wi-Fi indicator, and custom drawn battery pill with terminal nub and low-charge red shift.
- **Rule 4 Persistent Button Legend**: Bottom-right docked controller pill (Ⓐ Ⓑ Ⓧ Ⓨ) that updates dynamically with context.
- **Rule 5 Strictly Separated Icon Vocabularies**: Monochrome uniform-stroke line art system icons vs full-color game content art.
- **3DS Dual-Screen Modal**: Click or press Ⓐ on any game to launch the interactive dual-screen simulation (top 3D screen at 60 FPS + bottom interactive touch screen inventory).
- **Synthesizer Audio Engine**: Native HTML5 Web Audio API micro-sounds for cursor movement (blip), selection (rising harmonic chime), and back (low drop).

### 2. Interactive Taste Spec & Living Lab
Toggle to the **Taste Spec** tab (or press `Y`) to interactively test all 10 rules with live side-by-side comparison widgets:
1. **Rule 1**: Scale + Inversion vs Generic Desktop Outline Border.
2. **Rule 2**: One Saturated Hue Screen Palette (Sapphire Blue, Cyan, Coral Red, Jade, Violet, OLED Dark).
3. **Rule 3**: Nested Decreasing Radii visualizer (24px → 16px → 10px → Round).
4. **Rule 4**: Controller Legend layout switcher (Nintendo, PlayStation, Xbox).
5. **Rule 5**: Monochrome Line Art vs Content Art comparison.
6. **Rule 6**: Circular button active state tester.
7. **Rule 7**: Interactive battery charge slider (5% to 100%).
8. **Rule 8**: Humanist rounded typography scale.
9. **Rule 9**: Carousel peek and breathe simulation.
10. **Rule 10**: Springy press & 180ms ease-out physics.

## Controls

| Action | Keyboard | Gamepad | Mouse / Touch |
|---|---|---|---|
| Navigate Shelf | `←` / `→` | D-Pad / Left Stick | Click / Hover / Drag |
| Switch Shelf ↔ Dock | `↓` / `↑` | D-Pad Vertical | Click dock icon |
| Confirm / Launch | `Enter` / `Space` | Button Ⓐ / ✕ | Click card |
| Back / Close Modal | `Esc` / `Backspace` | Button Ⓑ / ○ | Click close / background |
| Game Details / Options | `X` | Button Ⓧ / □ | Click card |
| Toggle Dashboard ↔ Spec | `Y` | Button Ⓨ / △ | Click header tab |
| Toggle Audio Sound | `M` | — | Click speaker icon |

## How to Run

Open directly in any browser:
```sh
open /Users/shoroz/Desktop/azahar-src/website/index.html
```

Or view via the local server:
```sh
http://localhost:8089
```
