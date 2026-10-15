# Sharp MIP Display

This shield provides a custom status screen for 160x68 Sharp
Memory-in-Pixel displays such as the nice!view.

This shield requires that an `&nice_view_spi` labeled SPI bus is provided with _at least_ MOSI, SCK, and CS pins defined.

## Images

The left (central) screen shows `art_zeidler` below the battery / BLE profile /
connection row, the right (peripheral) one shows `art_18c3`. Both are generated
from the PNGs in `art/`:

- Draw upright, as seen on the keyboard: 68 px wide, black ink on white.
  Left: up to 68×116 (between status row and layer name), right: up to 68×140.
- Run `python3 art/gen_art.py` (needs Pillow) to regenerate `widgets/art.c`,
  including the `SHARP_MIP_ROTATE_180` and `SHARP_MIP_WIDGET_INVERTED` variants.
- A new file `foo.png` becomes `art_foo`; reference it with
  `LV_IMG_DECLARE(art_foo)` and `lv_img_set_src()` in `widgets/status.c` or
  `widgets/peripheral_status.c`.

## Disable custom widget

The shield includes a custom vertical widget. To use the built-in ZMK one, add
the following items to your `.conf` file:

```
CONFIG_ZMK_DISPLAY_STATUS_SCREEN_BUILT_IN=y
CONFIG_ZMK_LV_FONT_DEFAULT_SMALL_MONTSERRAT_26=y
CONFIG_LV_FONT_DEFAULT_MONTSERRAT_26=y
```

## Rotate screen

You can rotate the custom widget by 180 degrees. To do so, add the following item to your `.conf` file:

```
CONFIG_SHARP_MIP_ROTATE_180=y
```
