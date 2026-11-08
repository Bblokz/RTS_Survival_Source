# Add-on armour panels (spaced armour, side skirts)

Read `Docs/Units/Vehicles/AddOnArmorPanels.md` before you change anything here or convert a vehicle.

## Rule: every spaced-armour vehicle gets a debris panel mesh

Every spaced-armour hull (side skirts, Schürzen, bolted-on plates, wire screens) needs **one panel as its own static
mesh asset**, besides the hull that carries all the panels. When a hit hides a panel, that mesh flies off the tank as
Niagara debris. Without it the panel just disappears, and the component reports an error when the tank's zones install.

The asset is only drawn by the Niagara burst. It is never a component on the vehicle, so it costs the tank nothing.
The panels on the vehicle stay baked into the hull and masked.

**Cut it out of the built hull. Don't model it.** `RTS.AddOnArmor.ExtractPanelDebris` copies one baked panel into its
own mesh. It looks exactly like the panel on the hull and costs the same as a hand-made one. Model a panel by hand only
when the panels don't exist as separate pieces in the hull, or when the user asks for it.

The debris panel mesh:
- is one **left-side** (-Y) panel as it hangs on the hull, with its bolts and clamps. Right-side panels use the same
  mesh turned 180 degrees about Z;
- has its pivot at the panel's centre (the centre of its bounds), in the hull mesh's axes (X forward, Z up) and scale;
- uses the panel's own material (the `_AddOnPanels` panel material is fine);
- has no collision, no lightmap UVs and Nanite off;
- is named `<hull>_AddOnPanels_Debris` and lives next to the hull.

Store it on the hull's panel layout with one of:
- `RTS.AddOnArmor.ExtractPanelDebris Hull=<_AddOnPanels mesh> [Panel=<index>]` (the default) cuts a left panel out of
  the built hull and stores it. Without `Panel=` it takes the left panel nearest the middle of the side, a typical
  panel and not an end piece;
- `RTS.AddOnArmor.SetPanelDebrisMesh Hull=<_AddOnPanels mesh> Debris=<panel mesh>` for a panel modelled by hand;
- `RTS.AddOnArmor.BuildPanelHull ... DebrisMesh=<panel mesh>` when you build the panel hull with an existing mesh.

`Tools/AddOnArmorBreak/build_panel_debris.sh` rebuilds the system and extracts a debris panel for every built hull
(`ONLY="<hull> ..."` for some). On 2026-11-06 all 22 `_AddOnPanels` hulls got an extracted debris panel this way.
These cut-outs are final. Don't replace them with modelled panels.

Never cut debris from a panel that carries the faction sigil stamp. Niagara has no custom primitive data, so every
flying plate would show a sigil (the Panther D's middle plate does this; its debris comes from `Panel=5`). Look at an
intact picture from the validation before you pick a panel.

End panels with a different size or outline (a chamfer, a nose) can fly off as themselves:
`RTS.AddOnArmor.ExtractPanelDebris Hull=<hull> Panel=<index> -OwnPanel` cuts that panel out as it hangs, on either
side, into `<hull>_Debris_Panel<index>` and stores it as the panel's own debris mesh (`FAddOnArmorPanel::OwnDebrisMesh`).
Cut both sides' end panels: the shared mesh is turned 180 degrees for right panels, which mirrors a symmetric plate but
points an end plate's chamfer the wrong way; an own mesh is never turned. The component throws one burst per debris
mesh, so a death blast throws one burst per side and mesh. A rebuild with the same panel count keeps the own meshes.
The T-34E does this for its 4 end plates (`build_panel_debris.sh` `OWN_PANELS`). Without own meshes, every panel flies
off as the shared one; pass `Panel=<index>` to pick another shared panel when the default looks wrong.

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
- One pooled CPU burst per hit and debris mesh, with one particle per hidden panel. When the tank dies (any death type), `BlowOffIntactPanelsOnDeath` throws every panel still on the hull, one burst per side; the validation's death step checks it. A loaded save only restores the mask and throws
  nothing, and neither does a tank that fog of war hides (its debris would give it away). Pieces bounce on an analytic ground plane (one landscape trace per burst) and shrink away after 3.6 s, so
  nothing lingers and no physics runs.
- The folder is in `DirectoriesToAlwaysCook`, because the component loads the system by a soft path.
- Check it with `Tools/AddOnArmorBreak/run_panels_validate.sh` on `MAP=/Game/RTS_Survival/Maps/UnitTests/UT_B4Howitzer`.
  `RTS_ToCopySmall` renders black silhouettes in its captures. The run checks one debris piece per hidden panel (none when fog
  of war hides the test tank, as it does the Soviet ones) and, 2.5 s after the shell, a flat resting piece, and writes a `1_FirstHit_Flight` picture. Add `-AddOnArmorPanelsTraceDebris` to log the piece
  every frame.

## The armour-off sound

`UAddOnArmorPanelAudioSubsystem` plays one clang per hit that hides panels, from a pool of 16 audio components
(`AddOnArmorOffAudioPoolSize`). Never spawn sounds for this with `UGameplayStatics::SpawnSoundAtLocation`: the pool is
there so hits create nothing for the GC. The cues are `Cue_AddOnArmorOff_Light/Medium/Heavy` in
`/Game/RTS_Survival/SpecialEffects/AddOnArmor/Sounds`; keep them in that always-cooked folder (soft paths don't cook)
and give them attenuation, because the pooled components set none.

## Debris HLSL gotchas (Niagara CPU VM)

- Don't use `asin` in the update. With it, resting plates froze at a tilt (45 degrees), half under the ground. A
  per-frame trace showed the turn stop dead right after landing. The turn now steps by the sine itself, and the plates
  settle flat in about 0.25 s.
- Feed every particle attribute the update script reads through an HLSL input pin (the map read in
  `WireDebrisHlsl`), including attributes the script also writes. The break test found that a token read alone may
  not load an attribute. Whether token reads also caused the freeze was not isolated: both changes went in together.
- The break test's `NS_TestSkirtDebris` (`Utils/RTSMapTests/AddOnArmorBreak/DebrisHlsl`) still has both problems;
  it is test content only.
