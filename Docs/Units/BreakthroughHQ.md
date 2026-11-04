# Breakthrough HQ (test set-up)

The Breakthrough faction's HQ building, modelled in Blender (`D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/HQ`,
see `HQ/Model/README.md` there). Not in the game's faction data yet: it runs as a test copy of the German HQ truck.

| What | Where |
|---|---|
| Building mesh, sandbags, ammunition | `/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Mesh/` |
| Resource fills (`Radixite` crystals on `MI_Crystal1`, `Metal`, `VehicleParts`; 10/25/50/75/100 %) | `.../BreakthroughHQ/Fills/`, import script `Source/Tools/BreakthroughBuildings/ue_hq_fills_import.py` |
| Cranes (skeletal meshes, AnimBPs, `AM_*_Working` montages) | `.../BreakthroughHQ/Cranes/Rail`, `.../Cranes/Fixed` |
| Attachments (both cranes, the dressing) | `.../BreakthroughHQ/Attachments/` |
| Test Blueprint | `.../BreakthroughHQ/Ger_HQ_Truck_Breakthrough` (copy of `Ger_HQ_Truck`: same truck, data and training) |
| Test map | `/Game/RTS_Survival/Maps/UnitTests/UT_BreakthroughHQ` (`UT_Nomadics` with the player's first HQ truck swapped) |

Play the map and expand the truck at the west edge of the vehicle park (-17947, -3732) to try it by hand.

## Cranes
Both cranes are `ANomadicAttachmentSkeletal` attachments driven like the old HQ crane: they work while the HQ trains. Their montages are
working loops: sections `Out` -> `Work` (loops to itself) and `Stop`. `NomadicSkeletalAttachment` treats a montage with `Work` and `Stop`
sections specially: when training stops, the current section finishes and `Stop` plays (the rail crane drives back into its bay) instead of the
montage being cut; when training starts again during a lift, the loop simply continues. Montages without those sections behave as before.
The fixed crane's `Out` starts with an 8.5 s wait so both cranes reach `Work` together: their loops are timed to take turns over the slot.

## Headless test
`-BreakthroughHQValidate` (`Units/Tanks/WheeledTank/BaseTruck/Tests/BreakthroughHQTestSubsystem`) expands the HQ and checks:
building mesh and all 25 sockets; that the cranes, dressing, radar and banners spawned; the drop-off has capacity for all three stores; WayPoint,
UnitExit and DropOff stand on open ground. It then fills the stores through the drop-off to 0, 10, 25, 50, 75, 100, 40 and 5 % and checks the mesh
shown on each store socket, runs the cranes' working loop for 45 s (the rail truck must leave its bay, the hooks must stay 2.5 m apart) and stops it
(both cranes must end where they started). `-BreakthroughHQCapture=<folder>` saves scene-capture pictures (overview, stores per level, cranes).

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_BreakthroughHQ -game -unattended -nosound -RenderOffscreen -windowed -ResX=1280 -ResY=720 -DisablePlugins=Fab -BreakthroughHQValidate -BreakthroughHQCapture=<folder> -CaptureExposureBias=1 -abslog=<file>
```
Logs `RTS_BF_HQ_TEST check=PASS|FAIL ...` and a final `RTS_BF_HQ_TEST RESULT`.

The map was built headlessly by `Saved/BF_HQ_Import/make_test_map.py`. Saving a landscape map from `-run=pythonscript` crashes in the landscape
collision's presave unless the commandlet renders: pass `-AllowCommandletRendering -RenderOffscreen`.
