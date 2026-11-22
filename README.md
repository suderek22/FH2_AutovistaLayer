# FH2_AutovistaLayer 0.3.0

**Author** · suderek22

FH2_AutovistaLayer brings showroom geometry into Forza Horizon 2 free roam alongside LOD0. Depending on the car, that can include detailed engines, engine bays, hood liners, trunk interiors, and pedals.

## How it works

The DLL detects LODS geometry as the game loads each car and makes it available at LOD0. When a showroom part has a matching stock counterpart, the mod can suppress the stock part at LOD0 to avoid overlap. It uses the car's existing assets, including assets supplied by other mods, and ships as a single DLL.

## Requirements

- Forza Horizon 2 running in Azimuth. Verified with Azimuth 1.0.4 and FH2 Mod Platform 2.9.1.
- Set **Car Detail** to **Ultra** in Azimuth to use LOD0 in gameplay.

## Installation

1. Close FH2 and Azimuth.
2. Copy `FH2_AutovistaLayer.dll` to the `fh2-mods` folder beside `azimuth.exe`.
3. Launch FH2. The mod takes effect automatically whenever a car renders at LOD0.

To uninstall, move the DLL out of `fh2-mods` while Azimuth is closed.

## Known issues

- Some cars are missing LODS parts, so the available showroom detail varies by car.
- On some cars, a flat LOD0 engine-bay texture overlaps the detailed LODS engine model.
- Custom car packs remain untested. The DLL detects LODS geometry in the assets the game loads, so custom cars using FH2's native LODS format are expected to work.
