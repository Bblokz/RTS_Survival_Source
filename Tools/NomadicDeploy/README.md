# Nomadic building deploy (VAT + smoke screen)

Every nomadic building deploys from its truck with a baked vertex-animation (VAT) of its own pieces, and packs up by
playing it backwards. The "3 Smoke Screen" effects go with it: smoke around the footprint hides the first seconds,
and heavy pieces kick dust where they land. GPU-only motion: the CPU writes one custom-primitive-data float at 30 Hz
while a deploy plays, and nothing when idle. Effects use pooled Niagara components.

## Runtime (C++)

- `UNomadicDeployAnimationComponent` (`Units/Tanks/WheeledTank/BaseTruck/NomadicDeployAnimation`) plays the deploy
  mesh's progress 0 -> 1 (deploy) or 1 -> 0 (pack-up) and fires `FNomadicDeployEffectBurst`s as the progress crosses
  them. Progress = custom primitive data 3.
- `ANomadicVehicle` owns the component plus the `DeployAnimationMesh` component. The building mesh is hidden while the
  deploy mesh animates and shown at the end. Its collision, sockets and materials are untouched.
- `UNomadicDeployAnimationData` (`DA_<Building>_Deploy`): the deploy mesh and the bursts. Set as `DeployAnimationData`
  on the truck blueprint. Without it the building simply appears after the mesh animation time.
- The old construction-material swap and its smoke-location calculation are gone. `M_ConstructionAnimationMaterial`
  now only serves the airbase roof.

## Materials

`MF_NomadicDeploy_VAT` sits inside each building's master materials behind the static switch `NomadicDeploy`
(default OFF, so existing uses compile exactly as before). The deploy mesh uses `MI_<Building>_Deploy_NN` children
of the building's own slot materials, with the switch on and the building's VAT textures (`NomadicDeployVatOffset`,
`NomadicDeployVatRot`, `NomadicDeployVatFrames`).

Megascans material-attributes masters get `M_NomadicDeploy_MSPlain` instead.

Never wire a material pin from a script by bridge node index. Use `UNomadicDeployEditorLibrary`, which also clears a
stale `UseConstant` on World Position Offset that old masters carry. A `UseConstant` makes the compiler ignore the
wired pin.

## Per building: the pipeline

Work dir: `Saved/NomadicDeploy/<Building>/` with `config.json` (see `HQ/config.json`):

| key | meaning |
|---|---|
| `building`, `source_mesh`, `truck_bp` | label, building mesh asset, truck blueprint |
| `truck_materials`, `truck_box_lo`, `truck_box_hi` | a piece is the static truck when >= 70 % of its faces use these materials AND it lies in the box (Unreal cm, mesh space) |
| `truck_centre`, `truck_bed` | outward ordering origin; where unpacked pieces start (bed top) |
| `mast_xy`, `mast_radius` | optional tall lattice that becomes one hinged piece |
| `smoke_screen` | optional `{ring, waves}` overrides (see `ue_data_asset.py`) |
| `extra_bursts` | optional hand-placed bursts `[{progress, location, scale, system: smoke/dust}]`: contact-frame dust, covers for runtime attachments |

Per-building hooks live in the work dir, never in the shared scripts: `pieces_hook.py` (`relabel(records)`,
`post_bands(BANDS)`, see `pieces.py`) and `choreo.py` (`animate(ob, r)`, `post()`, with `rest_of` / `repivot`
helpers, see `vat_animate.py`).

Burst timing: a burst fires when the progress crosses it (deploy rising, pack-up falling), and bursts placed exactly
at 0.0 / 1.0 also fire at the start of a deploy / pack-up, so a 1.0 burst plays at the end of the deploy AND at the
start of the pack-up. Building attachments are removed 3 % into the pack-up, after such cover bursts. Heavy dust from
`vat_events.py` fires within 12 cm of rest: land pieces hard or place dust with `extra_bursts`.

Steps (Blender 4.0 headless: `B=D:/Blender/BlenderSoftware/4.0/blender.exe`; `MSYS_NO_PATHCONV=1` in Git Bash):

1. `python ue_pipeline.py <work> export`: writes `building.fbx`.
2. `$B -b --factory-startup -P analyze.py -- <work>/building.fbx <work> [--material]`: part / material renders, to
   choose the truck box and materials.
3. `$B -b --factory-startup -P pieces.py -- <work>`: `pieces.json` plus `pieces_check_*.png`. The truck must be red
   and nothing else.
4. `$B -b --factory-startup -P vat_animate.py -- <work>`: `deploy.blend` (one keyframed object per piece). Tune the
   choreography here or by hand in the .blend.
5. `$B -b <work>/deploy.blend --factory-startup -P vat_bake.py -- <work> [--preview]`: `vat_offset.exr`,
   `vat_rot.exr`, `deploy_mesh.fbx`.
6. `$B -b <work>/deploy.blend --factory-startup -P vat_events.py -- <work>`: `deploy_events.json` (landing times).
7. `python ue_pipeline.py <work>`: import, textures, masters, mis, data, verify (editor lock; saving steps also take
   the headless lock and fail on an unsaved asset). `verify` must print `SUMMARY finished=1 ok=1` (every slot
   compiled with WPO, VAT UVs on 4/5). Then `... probe`: progress 0 / 0.5 / 1 PNGs in `<work>/probe`. Re-runs skip
   `masters` (it re-saves masters other buildings share).
8. `python run_deploy_test.py <work>`: real deploy + pack-up in a throwaway editor (headless lock), giving
   `<work>/deploy_test.mp4`.

Content per building: `/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/<Building>/`.

## Locks (several building agents run in parallel)

- `Saved/NomadicDeploy/editor.lock`: held by `ue_pipeline.py` for any shared-editor step.
- `Saved/NomadicDeploy/headless.lock`: one throwaway editor at a time (also held by saving pipeline steps).
- `locks.py`: a lock whose owner process is gone is broken automatically. Never delete a lock dir by hand.

Blender steps run in parallel freely.

## Building expansions (BXP mode)

The same VAT construction exists for building expansions, opt-in per expansion blueprint: `ABuildingExpansion` has
`DeployAnimationData` (EditDefaultsOnly, default empty). Empty keeps the old construction-material animation (every
expansion that has not been converted, and field defenses always). When set, the expansion creates a deploy mesh
component in game worlds and:

- construction / unpacking plays progress 0 -> 1 over the construction time, with the progress bar synced to it;
- pack-up plays down from the current progress over the owner's pack-up time. Building attachments go 3 % in, as on
  the nomadic buildings. In BXP mode `BP_OnStartPackUpBxp` (which hides turrets and the radar dish) fires at the same moment
  (at least 0.35 s in), after the cover smoke at progress 1.0 has built up;
- a cancelled pack-up of a built expansion snaps back to the building (BP_OnCancelledPackUpBxp), and a cancelled pack-up that interrupted the
  construction resumes the construction from the shown progress (raising BP_OnStartBxpConstruction again, so turrets stay hidden);
- dying mid-animation shows the real mesh again for the collapse.

Config (`Saved/NomadicDeploy/<Bxp>/config.json`):

- `"mode": "bxp"` and `bxp_bp` (the expansion blueprint) instead of `truck_bp`;
- content goes to `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/BxpDeploy/<Bxp>/`;
- no truck: `truck_materials: []` and a zero truck box. `truck_bed` / `truck_centre` are only where pieces come from;
- every piece is hidden at progress 0, because nothing exists before construction;
- `drop_extra_uvs: true` drops UV sets 4+ from the deploy mesh only, for 6-UV source meshes whose materials do not
  read them;
- smoke may shrink to `smoke_screen.min_size_factor`, default 0.25 in BXP mode;
- test settings: `host_truck_bp` (default the SD HQ truck), `host_socket` (empty means the host origin, for
  origin expansions), `pack_up_seconds`, `construction_seconds` (informational).

Test: `python run_deploy_test.py <work> [max_seconds] [--states]` runs `bxp_deploy_test.py`. The host deploys,
then the expansion is built where its owner would place it through the `UNomadicDeployEditorLibrary` test hooks,
without an owner. It holds, then packs up. `--states` adds a pack-up during construction with a cancel, and a
cancelled pack-up of the built expansion.

The expansions' child actors and extra components (turrets, radar dish) are hidden and shown by blueprint events:
they appear when construction finishes and vanish when pack-up starts. Cover them with `extra_bursts` near 0.985 / 1.0.
