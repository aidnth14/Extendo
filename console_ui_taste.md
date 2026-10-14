# Console UI Taste

Rules for a sleek, console-style TV/handheld dashboard interface.

## 1. Selection is scale + inversion, never a border
The biggest tell. A focused tile gets no outline — it grows ~4–8%, gains a soft shadow,
and **flips to the opposite luminance**: the focused row turns
solid white with dark text. Outline-only focus reads as "generic desktop app".

## 2. One saturated hue owns the screen
Each context commits to one brand colour — vibrant accent, dashboard blue,
software pages near-black. Depth comes from **tinting that same hue** (white at
8–15% alpha), never a second unrelated grey. Two mismatched greys is the classic mistake.

## 3. Nested, decreasing corner radii
Outer container ~20–24px, cards ~14–16px, thumbnails ~8–12px, chips fully round. Radius
always *decreases* as you nest inward. Nothing is a sharp rectangle except artwork.

## 4. A persistent button legend, bottom-right
Every screen ends in a floating pill: circular glyphs — Ⓐ Ⓑ Ⓧ Ⓨ — each followed by a verb
("Confirm", "Back"). Always docked, it replaces scrollbars, tooltips and menu bars as the
primary affordance.

## 5. Two icon vocabularies, strictly separated
System icons are **monochrome, uniform-stroke line art**, optically equal in size,
inheriting the foreground colour. Content — box art, avatars — is the only full-colour
element. Coloured system icons destroy the look.

## 6. Circular icon buttons
Perfect circles, neutral tinted fill, centred glyph about half the circle's diameter.
The active one inverts: white fill, coloured glyph.

## 7. Quiet status cluster, top corner
Time, Wi-Fi, battery sit top-right — small, low-contrast, never bold. Battery is a drawn
pill with a terminal nub. Status never competes with content.

## 8. Rounded humanist sans, big weight jumps
Titles are heavy (600–700) and clearly larger; body is regular and dimmed to 60–70%
opacity. Hierarchy comes from weight and size, not colour or rules.

## 9. Content peeks and breathes
Carousels cut adjacent items off at the edges so the shelf reads as continuous. Padding
is generous — about one icon-width around every group.

## 10. Flat fills; motion supplies the life
No gradients or bevels. Richness lives in *movement*: eased scale on focus, springy
press, inertial scrolling. 130–260ms, ease-out.
