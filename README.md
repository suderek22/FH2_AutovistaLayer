# FH2_AutovistaLayer 0.3.3

**Author** · suderek22

FH2_AutovistaLayer brings showroom geometry into Forza Horizon 2 free roam alongside LOD0. Depending on the car, that can include detailed engines, engine bays, hood liners, trunk interiors, and pedals.

<img width="960" height="540" alt="Zrzut ekranu 2026-11-22 205232" src="https://github.com/user-attachments/assets/f10ff630-1158-4c33-bf35-49e9af91f598" />
<img width="960" height="540" alt="Zrzut ekranu 2026-11-22 205509" src="https://github.com/user-attachments/assets/83164449-8232-4807-a50d-8fa026d2676d" />
<img width="960" height="700" alt="Zrzut ekranu 2026-11-22 200139" src="https://github.com/user-attachments/assets/3576b835-55d0-4c33-811a-d4bb1f338698" />

## How it works

The DLL detects LODS geometry as the game loads each car and makes it available at LOD0. When a showroom part has a matching stock counterpart, the mod can suppress the stock part at LOD0 to avoid overlap. It uses the car's existing assets, including assets supplied by other mods, and ships as a single DLL.

## 0.4.0 Autoshow preview

To enable the preview, add this to `fh2-mods\FH2_AutovistaLayer.ini` and restart Azimuth.

```ini
[Autoshow]
Preview=1
```

After choosing the car and its colour, the final purchase preview requests LOD0 with the available showroom geometry. Press **Y** to Explode and press **Y** again to Implode. The preview is disabled by default.

## Changes in 0.3.3

- Suppressed the McLaren P1's flat `EngineBay_a_LOD0` mesh in gameplay.

## Changes in 0.3.2

- Improved replacement of flat engine-bay meshes, including on the Corvette ZR1, Acura RSX Type-S and VW Baja Bug.
- Retained stock floors, panels and liners where showroom parts only add hardware or labels. This includes the Toyota 2000GT floor and trunk interior.
- Restored the exterior draw flag on the Audi RS 6 Avant engine.
- Added a persistent **Debug log** setting. Off logs one line per car per session. On enables per-part diagnostics.

## Requirements

- Forza Horizon 2 running in Azimuth. Verified with Azimuth 1.0.4 and FH2 Mod Platform 2.9.1.
- Set **Car Detail** to **Ultra** in Azimuth to use LOD0 in gameplay.

## Installation

1. Close FH2 and Azimuth.
2. Copy `FH2_AutovistaLayer.dll` to the `fh2-mods` folder beside `azimuth.exe`.
3. Launch FH2. The mod takes effect automatically whenever a car renders at LOD0.

To uninstall, move the DLL out of `fh2-mods` while Azimuth is closed.

FH2 Mod Platform checks GitHub Releases for newer versions and shows update
status in the Mods menu. [AutovistaLayer releases](https://github.com/suderek22/FH2_AutovistaLayer/releases)

## Known issues

- The LaFerrari front-panel rendering issue remains open.
- Some cars are missing LODS parts, so the available showroom detail varies by car.
- Custom car packs remain untested. The DLL detects LODS geometry in the assets the game loads, so custom cars using FH2's native LODS format are expected to work.
