# Add-on armour panels (spaced armour, side skirts)

Read `Docs/Units/Vehicles/AddOnArmorPanels.md` before you change anything here or convert a vehicle.

## Rule: every spaced-armour vehicle gets a debris panel mesh

Whenever you develop spaced armour (side skirts, Schürzen, bolted-on plates, wire screens), model **one panel as its
own static mesh**, besides the hull that carries all the panels. When a hit hides a panel, that mesh flies off the
tank as Niagara debris. Without it the panel just disappears, and the component reports an error when the tank's
zones install.

The debris panel mesh:
- is one **left-side** (-Y) panel as it hangs on the hull, with its bolts and clamps. Right-side panels use the same
  mesh turned 180 degrees about Z;
- has its pivot at the panel's centre (the centre of its bounds), in the hull mesh's axes (X forward, Z up) and scale;
- uses the panel's own material (the `_AddOnPanels` panel material is fine);
- has no collision, no lightmap UVs and Nanite off;
- is named `<hull>_AddOnPanels_Debris` and lives next to the hull.

Store it on the hull's panel layout with one of:
- `RTS.AddOnArmor.SetPanelDebrisMesh Hull=<_AddOnPanels mesh> Debris=<panel mesh>` for a modelled panel;
- `RTS.AddOnArmor.BuildPanelHull ... DebrisMesh=<panel mesh>` when you build the panel hull;
- `RTS.AddOnArmor.ExtractPanelDebris Hull=<_AddOnPanels mesh> [Panel=<index>]` cuts a left panel out of the built
  hull. Use it only when the panels already exist as separate pieces in the hull and a modelled panel isn't available.

`Tools/AddOnArmorBreak/build_panel_debris.sh` rebuilds the system and extracts a debris panel for every built hull
(`ONLY="<hull> ..."` for some). On 2026-11-06 all 22 `_AddOnPanels` hulls got an extracted debris panel this way.
Replace one with a modelled panel when that vehicle's skirts get worked on.

These commands also set "Used with Niagara Mesh Particles" on the panel's base materials, and save them. Without that
flag a cooked build draws the default material on the debris. `BuildPanelHull` keeps the previous build's debris mesh
when you rebuild without `DebrisMesh=`. It warns when the hull has none.

## The debris effect

- `NS_AddOnArmorPanelDebris` (`/Game/RTS_Survival/SpecialEffects/AddOnArmor`) is built by
  `RTS.AddOnArmor.BuildDebrisSystem` (`AddOnArmorPanelDebrisAuthoring.cpp`). Rebuild it with that command instead of
  editing the asset by hand. The motion is `DebrisHlsl/*.hlsl`.
- The static mesh is the user parameter **`User.DebrisMesh`**. `UAddOnArmorPanelsComponent` sets it per burst to the
  hull's debris panel, so one system serves every vehicle. The other user parameters are listed in
  `AddOnArmorPanelDebris.h`.
- One pooled CPU burst per hit, with one particle per hidden panel. A loaded save only restores the mask and throws
  nothing, and neither does a tank that fog of war hides (its debris would give it away). Pieces bounce on an analytic ground plane (one landscape trace per burst) and shrink away after 3.6 s, so
  nothing lingers and no physics runs.
- The folder is in `DirectoriesToAlwaysCook`, because the component loads the system by a soft path.
- Check it with `Tools/AddOnArmorBreak/run_panels_validate.sh` on `MAP=/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer`.
  `RTS_ToCopySmall` renders black silhouettes in its captures. The run checks one debris piece per hidden panel (none when fog
  of war hides the test tank, as it does the Soviet ones) and, 2.5 s after the shell, a flat resting piece, and writes a `1_FirstHit_Flight` picture. Add `-AddOnArmorPanelsTraceDebris` to log the piece
  every frame.

## Debris HLSL gotchas (Niagara CPU VM)

- Don't use `asin` in the update. With it, resting plates froze at a tilt (45 degrees), half under the ground. A
  per-frame trace showed the turn stop dead right after landing. The turn now steps by the sine itself, and the plates
  settle flat in about 0.25 s.
- Feed every particle attribute the update script reads through an HLSL input pin (the map read in
  `WireDebrisHlsl`), including attributes the script also writes. The break test found that a token read alone may
  not load an attribute. Whether token reads also caused the freeze was not isolated: both changes went in together.
- The break test's `NS_TestSkirtDebris` (`Utils/RTSMapTests/AddOnArmorBreak/DebrisHlsl`) still has both problems;
  it is test content only.
