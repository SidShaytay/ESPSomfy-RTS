# README LCD screenshots

These exact-size images use the production `LocalUiRenderer.cpp`, pinned Adafruit GFX library/fonts and synthetic public data. They are renderer captures, not photographs or evidence of real motor movement.

After building dependencies, reproduce them from the repository root:

```bash
bash tools/preview-local-ui.sh
```

Copy `status.png`, `quick-home.png`, `home-destinations.png`, `home-tools.png` and `shutter-{up,stop,down}.png` from `build/ui-preview/`. The committed navigation images are named `quick-access.png`, `home-2.png` and `home-3.png`. Fixtures live in `tests/ui-render/render.cpp`; the shutter captures select UP/STOP/DOWN with different estimated positions and `moving=true`. The LCD receives no direction feedback from the motor.
