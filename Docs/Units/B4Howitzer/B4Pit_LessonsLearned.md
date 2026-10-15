# Building a crewed gun pit: lessons from the B-4 pit

This is what building `Rus_BXP_B4HowitzerPit` taught: a building expansion holding a team-weapon gun, with four animated crew who load it with a crane. The pillbox on RedLine (`Rus_BXP_RL_TurretPillbox`) was made from it afterwards.

What the pit is and how its parts fit together is described in [B4Howitzer.md](B4Howitzer.md). This page is the how-to and the traps. Each item cost at least one test cycle.

## 1. Start from an existing expansion, then undo what it brings along

Duplicating a working BXP is the fastest start, but the copy carries the source's settings:

| Inherited from the source | Symptom | Fix |
|---|---|---|
| Owner (the pit was copied from the German LeFH) | The "enemy" pit was player-owned. | Set `OwningPlayer` = 2 on the RTSComponent of the class defaults. |
| `BuildingMeshComponent`'s own mesh (Flak88) | A pit placed in a level showed the Flak88 sandbags. | Set the mesh on the component in the class defaults (CDO), not only in `BP_InitBuildingExp`. A placed BXP never runs construction, so it only ever shows the component's template mesh. |
| The wreck's `SetLifeSpan(12)` (from `BP_Obstacle_RusMGBunker_Destroyed`) | The destroyed pit vanished after 12 s. | Remove the node. Emplacement wrecks should stay as obstacles. |
| The source's death graph | Wrong wreck and gun wreck spawned. | Rebuild `BP_OnUnitDies` with the bridge's `override_function` action. `ev.BP_OnUnitDies` binds to the HpPawnMaster version and leaves an out-of-date node. |

Edit class defaults with Python: `unreal.get_default_object(bp.generated_class())`, then `bp.modify()`, then `save_packages`. The bridge's `set_blueprint_variable_default` fails on enum arrays.

## 2. Registering a new expansion type

A new `EBuildingExpansionType` needs all of these, or it silently misbehaves:

1. The enum entry, plus its string and display-name cases in `BuildingExpansionEnums.h`.
2. Game data in `ACPPGameState`: build time, abilities, health and resistances. The enemy map is a copy of the player map, so one entry serves both.
3. Construction rules in `BxpConstructionRulesHelpers.h`, e.g. `WithFootprint(FreeConstructionRules, 900, 900)`.
4. The spawner mapping in `RTS_BP_RTSAsyncSpawner` (class plus preview mesh).
5. For the enemy AI: `SatteliteBuildingsForBase` on `RTS_BP_EnemyController`.
6. For a player version: a `DT_Items` row with an option image and icon, and an `AsDefense(...)` entry in the HQ options.
7. Store the type on the Blueprint (`PitExpansionType`) and pass it to `BP_InitBuildingExp`. That is where the RTS subtype, vision radius, abilities and health come from.

**Soviet BXPs don't set their subtype early.** No Soviet expansion implements `BP_PostInit_SetupRTSSubtype`, so at PostInit their type is `BXT_Invalid`. This used to log "Could not find bxp data … BXT_Invalid" for every one of them. `PostInit_SetupAbilities` now skips quietly, and `InitBuildingExpansion` sets the abilities once the type is known.

## 3. Art in Blender (headless scripts)

- **Units:** work in centimetres (scene `scale_length = 0.01`). Export with `apply_unit_scale` and `FBX_SCALE_ALL`.
- **Frame rate:** an FBX import resets the scene to 25 fps. Set 30 fps after every import, or the animations come out 1.2× too long. A 30 fps reimport over a 25 fps asset fails, so delete the old animation asset first.
- **Sockets:** export empties named `SOCKET_<name>`, parented to the mesh.
  - Blender +Y becomes UE −Y, and sockets arrive with roll −90 (harmless).
  - Empty names are global in a `.blend`. Rename each piece's empties after exporting it, or the next piece gets `cargo_1.001`, which becomes `cargo_1_001` in UE.
- **Materials:** use named material slots in Blender. Assign the real materials after import with `mesh.set_material(i, mat)`; editing a copy of `static_materials` does not persist.
- **Collision:** use `CTF_USE_COMPLEX_AS_SIMPLE` for walk-in emplacements.
- **LOD group:** use LargeProp for anything with hundreds of sandbags.
- **Ground level:** put walkable floors (decks, duckboards) a few cm above 0, or raise the child actor. The pit's timber turntable sat 5 cm under the landscape and only showed in patches; the fix was raising the gun's child actor to z 4.

### Sandbags

- **Don't model bags with projected UVs.** A planar or box projection streaks across every side face of a bag.
- **Use the SandBagPack singles instead.** Export `SM_SandBag_Single_01..03` from UE with `AssetExportTask` and `StaticMeshExporterFBX`. Then use them as templates: normalise them to a unit box and keep their per-loop UVs, which match `MI_Rus_SandBag_01`.
  - The pack bags' bounds include the corner ears, so overlap neighbours by about 9 cm to press the bodies together.
- **Make walls look real:**
  - Lay them in running bond: each course offset half a bag.
  - Jitter yaw, roll and size per bag, and leave the odd gap in the upper courses.
  - Set each course back about 4 cm towards the defender.
  - Add a rare third course on short stretches.
  - Lay one row per straight run, so the courses don't restart at every path point.
- **Both generators do this:** see `redline_fieldworks.py`, functions `sandbag_templates`, `add_sandbag`, `sandbag_row` and `sandbag_arc`. `b4_pit_props.py` (`build_sandbags`) has its own copies, which share the `RedLine/PackBags` FBXs.
  - The pit's bags are 38 cm wide instead of 40, so two rows fit on the 55 cm revetment. Its entrance walls follow the sloping ramp.
  - Reimport only the bags with `ue_b4_pit_import.import_sandbags()`.
- **Put loose piles at ground level (z 0),** not at the berm foot (z −20). Otherwise the bottom bags are buried and a pile looks like a single bag.
- **Close the earthworks:** where two banks meet at an outside corner, fill the quadrant; where a bank ends in open ground, cap its cross-section. Otherwise there are holes you can see through.

## 4. The crew and the loading drill

- **Skeleton and timing:** author the crew on the Infantry Master skeleton, frame by frame against the gun's own drill in the same Blender scene. Sample the gun geometry (trail rails, breech, tray heights) for footing and hand targets instead of guessing.
- **Measure the shell handling, don't trust a render.** The test samples how close each crew member's hands come to the shell (`crew_handles_the_shell`, limit 45 cm). The first pass rendered the loader floating beside the cart.
- **Copied templates keep their hidden flag.** The first crew meshes didn't render because they inherited it.
- **One reload, one drill:** play every montage at `length / ReloadTime`, so the breech closes exactly when the weapon can fire again. Leave a small end margin (`LoadingEndMarginSeconds` 0.25). Reloads shorter than `MinReloadTimeForLoadingSequence` only recoil.
- **Shared recoil:** the fire and reload animations share one recoil curve, so the drill can continue the recoil seamlessly instead of snapping.
- **Reversed montages** (deploy = pack played at a negative rate) must start at their end. Otherwise they finish in 0.02 s and the gun snaps open (`PlayMontageFromPlayDirectionStart`).
- **Blueprint components can't be parented to sockets of an inherited mesh.** Tag them instead (`B4PitCrew`, `B4PitSocket:<Socket>`) and attach them at runtime in `InitPitCrew(Turret)` from the turret's BeginPlay, keeping their Blueprint relative transform.
- **Hook points on the turret:** the `ReloadWeapon` event starts the drill (`OnReloadStarted(ReloadTime)`). `PlaySingleFireAnimation` gives the recoil (`OnGunFired`).

## 5. Editor workflow

- **No editor window needed:** a windowless editor (`UnrealEditor-Cmd <uproject> -NullRHI -unattended -DisablePlugins=Fab`) exposes the UECP bridge on 9855 for graph, component and widget edits.
  - Its `EditorActorSubsystem.spawn_actor_from_class` crashes; place actors from a Python commandlet instead.
- **Saving maps:** landscape maps only save from a commandlet with `-AllowCommandletRendering` and without NullRHI. The landscape compiles shaders in PreSave.
- **When the user's editor is open, it holds the asset files.** A commandlet import then fails to save, with "MoveFile was unable to move". Run the same import script through the bridge inside their editor instead, and never save a map they have open with unsaved changes.
- **Don't save the load-dirtied StarterContent** packages that `get_dirty_content_packages` lists.

## 6. Testing

- **Tests are switches on a map run,** hooked into `URTSCoverFinderWorldSubsystem::StartCommandLineMapTests`:
  - `-B4HowitzerValidate` (spawned). It starts the B-4 scenario, so the two below need it as well.
  - `-B4HowitzerValidate -B4HowitzerObserve` (placed). On its own, `-B4HowitzerObserve` runs nothing and the map just idles.
  - `-B4HowitzerValidate -B4HowitzerPlayerBuild` (built from the HQ like a player)
  - `-RedLineTour` (the line on RedLine)
- **The game boots paused:** unpause through `ACPPController::PauseGame(ForceUnpause)`.
- **Enemy guns never fire under `-NullRHI`.** Enemy vision is a GPU Niagara readback, so nothing is ever tagged `IsInEnemyVision`. Run `-RenderOffscreen -windowed` whenever an enemy has to shoot.
- **Nobody watches an unattended run:**
  - Force `AlwaysTickPoseAndRefreshBones` on the meshes you follow.
  - Reset `CharacterMovement`'s tick interval every frame. Unseen soldiers otherwise move in coarse steps and overshoot.
- **Pictures:** scene-capture cameras (`FB4HowitzerCaptureRig`) need a fill light, the post-process material switched off (that's the fog of war), and enemy actors un-hidden.
- **Use real numbers:** place targets from `GetTurretRange()`, since data ranges are scaled by `GameRangeMlt`. Keep test units away from the map's own fights.
- **Player-build test:** find build spots with a ground trace (test maps may have no navmesh). Only the centre has to be inside the HQ's expansion range. Grant exactly the expansion's cost.

## 7. Reusing the pit for other emplacements

The turret pillbox is the pit's Blueprint with these changes:
- the Howitzer child actor swapped for a tank turret on a `TurretMount` socket;
- the building mesh, type, decal and selection sizes changed;
- the wreck rebuilt in its own death function.

Six children swap only the child turret class. Tank turrets work mounted on a BXP: `SetupTurret` plus `SetTurretsToAutoEngage`, the same as the BT-7 guard tower.

## 8. Three more pits from the same parts (German Pak 40, leFH 18, sFH 18)

The German pits ([GerGunPits.md](../GerGunPits.md)) moved the crew logic into `UGunPitCrewComponent` / `UGunPitAnimInstance` and made the B-4 classes thin subclasses. The `-B4HowitzerValidate` run was the regression check. What was new:

- **Turn the whole gun on its root bone.** An additive component-space rotation on the root bone traverses gun and crew 360°, with no turntable mesh. Pitch goes on the elevation bone, after the `DefaultSlot`.
- **Cancel odd root bones with a socket.** Team weapon skeletons import with scaled or rotated roots (leFH ×71.66, sFH ×100 and yaw −90). A `PitOrigin` socket with the inverse transform gives a clean 1:1 frame for crew, crate and rounds.
  - The bridge's `add_skeleton_socket` ignores transforms; set them with `set_skeleton_socket_transform`.
  - Python can't rename a `SkeletalMeshSocket`.
- **An additive animation played as a full pose zeroes every bone's scale.** `Anim_LeFH_18_Base` is a mesh-space rotation offset. Played alone, the leFH vanished (root scale 0, and everything on `PitOrigin` collapsed to one point). Apply it on its reference pose: `ApplyAdditive(Base = Anim_LeFH_18_TPose, Additive = Base)`. Check `additive_anim_type` before using any animation as a base pose.
- **The bridge's rotations are `[pitch, yaw, roll]`.** `set_component_transform rotation=[0, 0, -90]` rolled the loader, spotter and commander onto their sides. The test caught it because the loader's hands were 25 cm under the deck at the handover.
- **Reuse squad animations at their own crew positions.** Keep the crew at their Blueprint offset (`bKeepCrewBlueprintOffset`) with mesh yaw −90, exactly as a squad soldier stands. Bake only the new man (the handler) against the loader's sampled palm positions.
- **Hand a prop over by attaching with KeepWorld at the right moment.** Timers at fixed drill fractions move the round from hand to hand; no bone-space offsets are needed. The test measures each hand's distance to the round at the moment it takes it (15–33 cm here).
- **Find Blueprint nodes by role, not GUID.** Duplicating a Blueprint regenerates node GUIDs.
- **Setting the same skeletal mesh again is a no-op.** To force a turret mesh override, set another mesh first, then the wanted one.
- **The atlas material is Masked and single-sided:**
  - Camo-net regions need their alpha.
  - Sloped sheets must be wound so their normal faces up and out, or they show as dark fins.
- **Give sandbags the faction's sandbag material.** Remapping the bags onto a camo panel of the building atlas made them look like tarps. German walls use `M_GerSandBagScaffold_Atlas` with the loose bags' own UVs, so the pits now use that as a third material.
- **Map a deck plank by plank, never as one stretched region.** The planks quarter of the atlas mixes boards of different colours with flat padding between them. Spread over the deck, it gave dark and light stripes. Lay boards one by one from a single board region, and let a few pixels of the padding along one edge form the seam.
- **Offscreen pictures show low mips:** run with `-ExecCmds="r.TextureStreaming 0"`.
- **A save in the user's editor can fail with "MoveFile … Error Code 32"** right after a reimport, because the old mesh's bulk data file is still open. A garbage collect and a second `save_asset` work.

## 9. A machine-gun pit from the same parts (Soviet DShK)

The DShK pit ([RusDShKPit.md](../RusDShKPit.md)) reuses the gun pit crew component and the B-4 crew solver for a weapon with no shells to hand over:
- **Rig a static prop for a pit in Blender.** Split the mesh by loose parts into bone groups with simple bounds rules, add the mount (tripod) as new geometry UV'd into plain areas of the same texture, and keep one material.
- **Yaw a bone other than the root when the base must stay.** `Traverse` turns the gun and crew; the tripod on `Root` stays. Put `PitOrigin` on that bone, and work out its socket transform from the base pose: bone-relative = desired × inverse(bone component transform).
- **Swap props by trading bones.** The full and empty boxes end on each other's rest pose, so the drill loops without a pop. Drive the props and the loader's hands from one trajectory in one script.
- **Per-shot crew motion:** `FireMontage` on a crew member starts on a shot unless it already plays. Give the short recoil and shake montages a 0.04 s blend, or they never reach full weight.
- **Blender:** importing any FBX sets the scene to the file's frame rate (25). Reset to 30 after the last import, before exporting actions, or every clip comes out 1.2× long.
- **Taper earth berms into openings** instead of capping them; a vertical cap shows stretched planar UVs.

## 10. A new gun on an existing gun pit (Soviet 53-K)

The 53-K team weapon and pit ([Rus53K.md](../Rus53K.md)) reuse two existing setups.

**Share a skeleton when the gun has the same parts.** The 53-K is rigged in Blender with the ZiS-3's bone names and hierarchy, and it imports onto `SK_TW_Zis3_Skeleton`. The ZiS-3 anim BP, aim offset layout and team weapon Blueprint then only need their assets swapped.

**Reuse a pit drill by comparing breech positions, not socket chains.** Measure the breech from geometry in Blender. The 53-K's breech matched the Pak 40's in x and y, so the whole Pak drill, crew spots and crate carried over.

**A shared skeleton needs its own pit-origin socket per gun.** Name it after the gun (`PitOrigin53K`) and point the crate's `PitSocket:` tag at it.

**Size the pit by everything that turns with the gun.** The ready crate rides on the pit origin, so it sweeps a circle. Clear its far corner, not only the trail spades.

**Test the first shot, not only the settled state.** The traverse check at the first shot found a turret bug: an attack-ground order fired once along the old yaw (fixed in `SetEngageGroundLocation`).

**Blender:** importing an FBX into a scene at `scale_length` 0.01 scales it ×100. Import at 1.0, then switch.

## 11. A big gun, a square pit and a drill that fits any pit (Soviet BS-3)

The BS-3 team weapon and pit ([RusBS3.md](../RusBS3.md)) repeat the 53-K's recipe on a gun with 5 m-wide trails.

**Keep the crew drill independent of the pit.** Put every crew spot, the ready crate and the rounds in gun space on the pit-origin socket, and keep the crew on flat ground. A pit then only has to leave a clear radius around the pivot: everything that turns with the gun, plus a margin. The same turret drops into a round, square or dug-in pit.

**Move a borrowed drill as one piece.** The Pak drill's handover spot and crate fell on the BS-3's trail. Turning the whole layout (loader, handler spots, crate) 20° about the breech kept every relative distance the bake depends on. Moving single spots would have meant re-tuning the handover.

**Check the barrel mesh before you pick the recoiling parts.** KADO-3D's BS-3 barrel is two segments that meet hidden inside the cradle collar, so the collar has to recoil with them.

**Square walls:** lay straight runs in running bond and bond the corners by alternating, per course, which pair of walls runs through.

**Two editors on one project:** the second editor serves the bridge on its own port (30001). A test build on the DebugGame target avoids a Development DLL another session's editor holds.

## Open item

`SM_B4Pit_Sandbags` (from `b4_pit_props.py`) still uses the projected `box_uvs`. It has the same streaky bag sides that the RedLine pieces had, and it should switch to the pack-bag templates from section 3.
