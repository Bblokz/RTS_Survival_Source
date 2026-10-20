# Soviet 45 mm + twin Maxim emplacement

`Rus_BXP_MaximATPit`: the 53-K's round pit in the middle, an ammunition bay on its rear entrance, and a round Maxim nest on each flank, joined to the bay by sandbag corridors. Each Maxim has a gunner and a loader, who fetches a belt box from the bay on every reload. It is an enemy emplacement (owner 2) placed in levels: it engages as soon as it is placed (`bEngageWhenPlaced`, from the 53-K pit BXP it is copied from).

Blender sources: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/MaximATPit` (`generate_rus_maxim_at_pit.py`, `build_maxim_pit_gun.py`, `rus_maxim_at_pit_crew.py`, `validate_rus_maxim_at_pit.py`, report `rus_maxim_at_pit_report.txt`). Unreal build: `Saved/AgentScripts/RusMaximATPit/ue_rus_maxim_at_pit_build.py` (run by the editor queue).

## Layout (BXP space, cm)

| Part | Where |
|---|---|
| 53-K pit, turret `Howitzer` (`RTS_BP_EmbTurret_Rus53KPit`) | origin; ring inner radius 440, outer 556, rear entrance |
| Ammunition bay | x −800..−540, \|y\| < 170, on the AT pit's rear entrance; 45 mm crates in the middle, a crate of Maxim belt boxes per loader at (−762, ±92), a door in each side wall at x −690 |
| Left nest, turret `MaximLeft` (`Rus_BP_EmbTurret_MaximPit_Left`) | (−330, −800): two concentric rings (5 and 4 courses), inner radius 135 |
| Right nest, turret `MaximRight` (`Rus_BP_EmbTurret_MaximPit_Right`) | (−330, 800): one taller ring (6 courses), inner radius 155, earth berm outside |
| Corridors | 90 cm lane: nest opening → (−690, ±360) → bay door |

Mesh `Meshes/SM_RusMaximATPit` (63k triangles after the sandbag cut, Nanite off; RL_Sandbag, RL_Earth, RL_Timber, MaximAmmo → M_Maxim).

## The Maxim on its tripod

`Meshes/SK_MaximPit_Gun` is the KADO Maxim (no wheels, trail or turntable) on the DShK pit's tripod. It is skinned to the DShK pit gun's own six bones with the same reference pose, so it sits on `SK_DShKPit_Gun_Skeleton` and reuses `AnimBp_DShKPit` and the DShK gunner's animations. Its spade grips are where the DShK's are, so the gunner holds it. The shield rides `Traverse`, the cradle and the belt box hung on the right ride `Elevation`, the gun rides `Gun` (recoil).

The Maxim's muzzle is 45 cm short of the DShK's, and the skeleton is shared, so no socket is added. Each turret has `MaximMuzzle`, an empty inert mesh component under `Crew1` (which the crew component attaches to the gun's `PitOrigin`), at the Maxim's muzzle. The weapon (`Maxim_M1910_7_62MM_TW`) fires from it, so the shots turn with the gun; they do not follow the few degrees of pitch or the recoil.

## Turret crew

The turrets are copies of the twin MG42 turrets ([GerTwinMG42Pit.md](GerTwinMG42Pit.md)), with Soviet soldiers:

| Component | Socket | Role |
|---|---|---|
| `PitCrew` → `Crew1` | `PitOrigin` | DShK pit gunner: idle, burst shake, reload (cocks at the end) |
| `LoaderCrew` → `Crew2` | `PitStatic` | `Montage_RusMaximATPit_Loader{L,R}_Reload`, idle `Anim_RusMaximATPit_Loader{L,R}_Idle` |
| `LoaderBox` on `Crew2` / `ik_foot_root` | identity | the carried belt box, scaled 0.001 by the bake while his hands are empty |

`MG42VisualSetup` (inherited from the twin turret) is inert: it attaches a 0.001-scale MG42 to the empty `AmmoBox` bone.

## Reload and the loader's walk

The drill: out of the nest, along the corridor, into the bay, take a belt box, back (the right loader goes round behind his gunner to the gun's right side), hang it on the gun, back to his idle spot at the gun's right rear. It is 14.3 s (left) and 16.2 s (right), authored as a brisk walk.

So the loaders walk rather than run, the BXP adds `Behaviours/Behaviour_RusMaximATPit_BeltFromStore` at BeginPlay. It is a permanent weapon behaviour: +6 s reload on weapons of 10 mm calibre or less, so the Maxims reload in 12 s and the 45 mm is not affected. At 12 s the drills play at about 1.2× and 1.4×.

## Limits

- The guns turn 360°; the loaders do not. With the gun swung far round, the box is hung beside the gun's rest position.
- The belt box comes off a static stack, so the stack always shows three boxes.
- The BXP keeps the 53-K pit's expansion type (`BTX_Rus53KPit`) until a `BTX_RusMaximATPit` entry is added in C++.
- PIE check pending (PIE freeze, 2026-10-20).
