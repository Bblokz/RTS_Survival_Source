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
  nothing. Pieces bounce on an analytic ground plane (one landscape trace per burst) and shrink away after 3.6 s, so
  nothing lingers and no physics runs.
- The folder is in `DirectoriesToAlwaysCook`, because the component loads the system by a soft path.
