# Soviet DShK pit

A small Soviet sandbag pit with a DShK 12.7 mm machine gun on its tripod, worked by two men:
- the **gunner** stands on the spade grips, shakes with the bursts and cocks the gun after a reload;
- the **loader** swaps the ammunition box on every reload.

The gun and both crew turn a full circle on the tripod's pintle; the tripod stays put. It is an enemy emplacement (owner 2) that engages as soon as it is placed in a level. It comes from the same parts as the B-4 and German gun pits ([GerGunPits.md](GerGunPits.md), [B4Howitzer/B4Pit_LessonsLearned.md](B4Howitzer/B4Pit_LessonsLearned.md)).

| | |
|---|---|
| Expansion type | `BTX_RusDShKPit` ("DShK Pit") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusDShKPit/Rus_BXP_DShKPit` |
| Weapon | `DShk_12_7MM`, trace weapon in random bursts of 6–14 (200-round box, 6 s reload) |
| Health | `T2BxpBunkerHealth`, reinforced resistances, armed abilities |
| Cost | 240 radixite, 120 metal (`DeveloperSettings::GameBalance::UnitCosts::Bxps`) |
| Footprint | 800 × 800; the bags are 2.8 m inside, the berm runs out to about 7 m |

## Assets (`.../BuildingExpansions/RusDShKPit/`)

| Folder | Content |
|---|---|
| `Meshes` | `SK_DShKPit_Gun` (+ skeleton) uses the user's IS-3 roof DShK (`IS-3_TopMG.FBX`, scaled ×1.25 to a real DShK) on a modelled tripod, with one material (`M_IS2`). `SM_DShKPit` is the pit, in the RedLine materials (earth, timber, `MI_Rus_SandBag_01`) with complex collision. |
| `Anims` | `AnimBp_DShKPit` (parent `UGunPitAnimInstance`), `Anim_DShKPit_Base/Fire/Reload` and their montages |
| `Crew` | `Anim_DShKPit_Gunner_Idle/Fire/Reload`, `Anim_DShKPit_Loader_Idle/Reload` (Infantry Master skeleton) and their montages |
| `Turrets` | `RTS_BP_EmbTurret_RusDShKPit`, the embedded turret with the crew |

### Gun skeleton

| Bone | Pivot (cm) | Carries |
|---|---|---|
| `Root` | ground | the tripod |
| `Traverse` | pintle head, 84 cm | yaws the gun; the crew ride on its `PitOrigin` socket |
| `Elevation` | trunnion, 95 cm | pitches the cradle, the gun and the mounted box |
| `Gun` | receiver | recoil |
| `AmmoBox` | the mounted box | moved by the reload |
| `SpareTop` | top box of the spare stack on the floor beside the loader | moved by the reload |

Sockets:
- `PitOrigin` on `Traverse`, at the gun frame origin (ground, +X forward).
- `Muzzle` on `Gun`, the fire and casing socket.

The anim BP rotates `Elevation` (pitch) and `Traverse` (yaw) additively in component space, after `DefaultSlot`.

### The reload drill (6 s, stretched to the reload time)

The swap leaves no pop when the montage ends. The two box bones trade places:
1. The loader lifts the full box off the spare stack and sets it on the floor beside the gun.
2. He unclips the empty box (`AmmoBox`) and puts it on the stack.
3. He mounts the full box (`SpareTop`).

At the end the full box sits exactly where `AmmoBox` rests, and the empty one where `SpareTop` rests. Meanwhile the gunner keeps the grips, leans in and pulls the cocking handle at the end.

The new crew field `FGunPitCrewMemberAnimations::FireMontage` plays the gunner's 0.6 s firing shake on a shot unless it is still playing, so a burst keeps one shake going. Its montage and the gun's recoil montage blend in 0.04 s.

## Rebuilding

The Blender side is in `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/MGPit`:
- `Scripts/dshk_pit_build.py` (Blender **4.0**, it uses the B-4 crew solver `b4_pit_crew`) builds:
  - the rigged gun and its animations;
  - both crew members' animations (two-bone IK on the game skeleton, standing mocap base, foot stepping), with hand reach misses printed;
  - the pit mesh, and `dshk_pit_info.json`.
- `-- preview` also renders the drill to `Exports/Preview`.
- The sources in `Source/` (soldier, standing mocap, pack sandbags) are copies of the B-4 crew and RedLine sources.

The Unreal side is in [Tools/MGPit](../../Tools/MGPit):
1. `ue_dshkpit_import.py` (in the editor, or a `pythonscript` commandlet) imports the gun, both animation sets and the pit, and prints the socket transforms. Copy `Exports/dshk_pit_info.json` next to it first, because the editor's sandboxed Python only reads project files. Set `STEPS` to run single steps.
2. Add the `PitOrigin` / `Muzzle` sockets with the bridge's `add_skeleton_socket` and `set_skeleton_socket_transform`, using the printed values.
3. `build_dshkpit_animbp.py` builds the anim BP and montages.
4. `build_dshkpit_turret.py` builds the turret (`--with-fire` for the gunner's fire montage).
5. `build_dshkpit_bxp.py` builds the expansion.

## Testing

`-DShKPitValidate` on TestCover spawns the pit, orders it to attack the ground 60° off its forward and checks:
- two crew on `PitOrigin`;
- the gun and crew turned to the target;
- the box fired out (200 shots);
- the gun's box swap ran, with the empty box moving at least 30 cm;
- the loader's hands within 35 cm of both boxes (12 and 15 cm measured);
- the gunner's firing shake played.

Add `-GerPitCapture=<folder> -ExecCmds="r.TextureStreaming 0"` for pictures. Enemy guns only fire in a rendered run (`-RenderOffscreen -windowed`).

## Not done

- When destroyed, the pit collapses with the B-4 pit's collapse FX and leaves no wreck.
- The enemy AI does not build it, and there is no spawner entry; it is placed in levels.
- The gun's pitch moves the spade grips a few centimetres; the gunner's hands are baked at 0° pitch.
