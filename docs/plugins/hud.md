# Drawing on screen

Plugins draw from an `on_frame` callback. What a frame's callbacks draw is shown until the next frame's, so draw everything again every frame, and draw nothing to hide it.

## The space

- **640 × 480**, the game's own screen, whatever the window size. (0, 0) is the top left.
- **Colours** are `0xRRGGBBAA`. `pp_rgba(0xE5322D, 0.8f)` turns an `0xRRGGBB` colour and an alpha into one.
- **Text size** is its height in those units. Melee's damage digits are about 32; panel text is usually 11 to 16.

## The calls

```cpp
H->hud_rect(x0, y0, x1, y1, rgba, rounding, filled);       // panels, bars, outlines
H->hud_text(x, y, rgba, size, "text");                    // plain text, top-left at (x, y)
H->hud_label(x, y, rgba, size, align, "text");             // outlined text; align 0 left, 1 centre, 2 right (0.3)
H->hud_circle(x, y, radius, rgba, filled);
H->hud_capsule(x0, y0, r0, x1, y1, r1, rgba, filled);      // hull of two circles (0.4)
```

Use `hud_text` inside a panel. Use `hud_label` on top of the stage, because its outline keeps it readable over any background.

## Panels: share the screen

Several plugins draw at once, and on 0.5 they share each corner. Ask `hud_place` where your panel goes instead of picking a fixed position:

```cpp
float w = 190, h = 44, x = 8, y = 8;                    // the fallback, for runtimes before 0.5
if (PP_HOST_HAS(H, hud_place)) H->hud_place(PP_CORNER_TOP_LEFT, w, h, &x, &y);
H->hud_rect(x, y, x + w, y + h, pp_rgba(0x14161C, 0.8f), 6, 1);
```

Panels asked for in the same corner during a frame stack away from it, in the order they were asked for. Ask every frame, and only while the panel is showing, so the stack closes up when you hide it.

## The Pascal UI look

The first-party plugins share one look, so a screen full of plugins still reads as one thing:

| Use | Colour |
|---|---|
| panel background | `0x14161C` at 0.8 alpha, rounding 6 |
| main text | `0xF2F4FA` |
| secondary text | `0x9AA3B5` |
| good / success | `0x2DB84D` |
| warning | `0xF2C200` |
| bad / missed | `0xE5322D` |
| port colours | `pp_port_rgb[port]`: red, blue, yellow, green |

A 4-unit bar in the port's colour down the panel's left edge tells players whose data it is at a glance. The template does this. Add an opacity setting so players can fade panels they only glance at.

Keep screen space in mind. The damage meters along the bottom and the timer at the top are what players watch. Corners are for panels, and short-lived feedback ("L-cancel: 2 frames early") goes near the fighter or the damage meter and fades out after a second or two.

## World positions on screen

To draw on a fighter, a hitbox or a point on the stage, project its world position with the game's own camera. `pascalpatch/camera.h` does it (it includes `melee.h`):

```cpp
#include "pascalpatch/camera.h"

pp_camera cam;
if (pp_camera_read(H, &cam)) {                        // 0 on the menus
  float x = H->rdf32(fp + PP_FT_POS), y = H->rdf32(fp + PP_FT_POS + 4);
  float sx, sy, ppu;
  if (pp_project(&cam, x, y + 18, 0, &sx, &sy, &ppu))   // 0 when the point is behind the camera
    H->hud_label(sx, sy, pp_rgba(PP_RGB_TEXT, 1), 14, 1, "Tech!");
}
```

Read the camera once per frame, then project as many points as you need. `ppu` is how many HUD pixels one world unit is at that depth: a hitbox's radius times `ppu` is its radius on screen (pass `nullptr` when you do not need it). [hitbox_viewer.cpp](../../plugins/hitbox-viewer/native/hitbox_viewer.cpp) draws every hitbox and hurtbox this way.

## Things to avoid

- **Drawing when the overlay is open** is fine, but reacting to input then is not. See [input](input.md).
- **Flicker.** If a value changes every frame (a timer, a position), round it, or show it only when it settles.
- **Walls of text.** One number that answers a question beats five that describe the state. See [design](design.md).
- **On the D3D11 renderer** the port shows no overlay and no HUD. Plugins still run. D3D12, the default, has both.
