# Intro cinematic 1976: build tools

Brief: `Source/Docs/Campaign/IntroCinematicScript.md` (script + build ledger at the end).

## How the film is put together

- Map `/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976`. Streamed, hidden sublevels: the mission maps
  `Ger_Berlin`, `SM_Desert_01`, `Ger_2ndOreMountains`, `Ger_PripyatMe410`, the old `RTS_IntroCinematic` (its V2 pad,
  Soviet pad and hangar are reused as sets) and the cinematic's own sets under
  `/Game/RTS_Survival/Maps/Cinematics/Intro1976/` (`Intro_Bunker` at world x = 600000, ...).
- Master `/Game/RTS_Survival/Cinematics/Intro1976/LS_Intro1976_Master`: shot track with one subsequence per shot
  (`Sequences/LS_Intro_<id>`), narrator audio track, fade track. Each shot shows its levels with a Level Visibility
  track.
- C++ (`Source/RTS_Survival/Cinematics/IntroCinematic/`): `UIntroCinematicSubsystem` wakes up only in this map; it
  shows `UW_IntroCinematicHud` (portrait, cards, stamps, labels, notice; rows of `Data/DT_IntroCues`, pure function of
  sequence time) and the subtitles (`UW_IntroCinematicSubtitles`, a `UW_CinematicOverlay` under the portrait, table
  `Data/DT_IntroTranscripts` while the voice is scratch TTS), switches the fog of war post process off and hides
  listed gameplay actors. `AIntroCinematicGameMode` = map and MRQ game mode.
  Flags for a plain `-game` run: `-IntroCinematicAutoQuit`, `-IntroCinematicShotEvery=<s>`.
- Global animation goes through `Materials/MPC_Intro` (Archive, Flash, Darken, Stain, StainOpacity, Heat) with a
  Material Parameter Collection track; `M_Intro_Grade` (unbound post process volume in the map) mixes the archive and
  present grades from `Archive`.

## Tools

| File | Use |
|---|---|
| `intro_editor_server.py` / `intro_cmd.py` | throwaway editor that runs python request files (`python intro_cmd.py file.py`) |
| `intro_lib.py` | `safe_save` (refuses packages outside the cinematic folders), `make_persistent_current` |
| `intro_narration.py` / `make_scratch_vo.py` | narration rows; free offline TTS scratch waves + word timings |
| `make_text_textures.py` | PIL text textures (tags, date line, pencil label) |
| `intro_materials.py` | MPC, grade, map paper, burn ring, stain, text materials |
| `intro_sets.py` | set sublevels (bunker + map table; map helper `map_local(lat, lon)`) |
| `intro_build.py` | voice import, tables, master + shot sequences, map setup, MRQ presets |
| `intro_scout.py` / `scout_sheet.py` | editor stills through candidate cameras + contact sheet |
| `render.sh <preset>` | headless MRQ render -> `Saved/IntroCinematic/Render/<preset>/` |
| `game_run.sh` | headless plain `-game` playthrough with auto-quit and UI screenshots |
| `contact_sheet.py` | contact sheet from rendered frames |
| `intro_berlin.py` | Berlin strike (3.3 / 3.4): city plan of the Berlin kits, both Skyline levels, the 3.4b nuke, collapses and camera |
| `intro_warmap.py` | 2.4 map scene: red arrows that grow along routes, scale-model tank / V2 pieces on bases, the date line arrow |
| `intro_canisters.py` | 5.1: the Purple Death canisters (parts animated apart from one origin) and the gas puffs (`M_Intro_PDGas`) |
| `intro_fire.py` | fire on the map strikes: flame cards, light pool, burning pins (run off the ring clocks) |
| `intro_purple.py` | `M_Intro_PurpleDeath` on the trench mould and veins (growth revealed by MPC `Mould`) |
| `blender/sets/build_veins.py` | branching vein networks over the trench (`SM_Intro_Mould_Veins`, U = metres from root) |

Launch the throwaway editor (never the shared one):

    UnrealEditor.exe RTS_Survival.uproject -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976
        "-ExecCmds=py D:/UE5Projects/RTS_Survival/Source/Tools/IntroCinematic/intro_editor_server.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

## Rules learned the hard way

- `EditorLevelUtils.add_level_to_world` and `create_new_streaming_level` make that sublevel the CURRENT level.
  Never call `save_current_level` or `save_dirty_packages`; the mission maps show up dirty in the throwaway editor
  (navmesh rebuild) and must never be saved. Use `intro_lib.safe_save`.
- A Custom material node body is a function: helper functions must live in a local struct (see `VNOISE`).
- Deleting a material asset nulls it on placed actors: rebuild in place.
- Movie Render Queue draws UMG at scale 1 (designed for 1080p); preview renders at lower sizes show the UI too large.
- A C++ change needs the throwaway editor closed (it locks the Development DLL), then `Build.bat RTS_SurvivalEditor
  Win64 Development`.

## Rebuild from scratch (order)

1. `python make_scratch_vo.py` and `python make_text_textures.py` (outside the editor).
2. `bash launch_editor.sh` (throwaway editor, autosave off, command server).
3. Send a request with `intro_cmd.py` that calls, in this order: `intro_build.import_vo()`, `build_transcripts()`,
   `intro_materials.import_textures()`, `intro_materials.build_all(seed uvs)`, `intro_materials.build_notice_material()`,
   `intro_import.import_all()` (Blender kits from `Saved/IntroCinematic/import/`), `intro_sets.build_bunker()`,
   `intro_sets2.build_all()`, `intro_sets2.build_ural()`, `intro_purple.build_gas_material()` (5.1 gas), `intro_shots.build_film(intro_manifest.shots(), cues)`,
   `intro_sound.add_sounds(...)`, `intro_build.setup_map()`, `intro_build.build_render_preset(...)`.
   (`build_request.py` is the request used during the build; its STEPS list selects the parts.)
4. `bash render.sh MRQ_Final` (or `MRQ_Preview`, 5 fps), `python section_sheets.py <frames> <fps>`,
   `python check_cuts.py <frames> <fps>`, `python mix_audio.py --mux <frames> <fps> <out.mp4>`.
5. `bash game_run.sh` for the plain `-game` playthrough.

More files: `intro_manifest.py` (every shot as data), `intro_shots.py` (shot builder, master tracks, tank mesh kits),
`intro_sets2.py` (the other sets), `intro_sound.py` (one sound list for the sequence and the offline mix),
`intro_import.py` (FBX + texture import), `check_cuts.py` (junk frames at cuts), `blender/` (kit scripts).

More rules learned:
- `MovieSceneParameterSection.add_scalar_parameter_key` takes TICKS (24000/s); channel `add_key` takes display frames.
- Unit Blueprints (tanks) ignore a transform track: use their meshes (`TANK_KITS` in `intro_shots.py`).
- Shot sections get 6 pre-roll frames and the subsystem lifts the level-streaming time slice, or cuts show a junk frame.
- `LS_RTS_Logo` loads the menu map through its event track at about 16 s: keep the logo shot shorter.
- A texture parameter's default texture must match its sampler type (sRGB colour vs masks / linear). A mismatch
  compiles in the editor but fails in the render process: every instance draws as the grey default material. After a
  render: `grep "Failed to compile Material" Saved/IntroCinematic/render_<preset>.log | grep Intro1976` must be empty.
- Rebuilding `M_Intro_PBR` needs the kits re-imported in the same request (`kits` step of `build_request.py`).
- `M_Intro_Grade` works after tone mapping: strong emissive is already white when it arrives. Dark sets lock their
  exposure with `intro_sets2.fixed_exposure`.
- Camera feel lives in the manifest: two-key moves are linear, `"move": "ease"` and `"handheld": <degrees>` per shot.
- Weather is two MPC scalars of the grade (`Rain`, `Sand`), keyed per shot under `"mpc"`.
- Shot lists in the manifest: a list comprehension must be its own `[...]`; join with `] + [`.
- `MPC_Intro` scalars are only ever appended (`build_mpc`): replacing the list changes their ids and breaks every
  material that reads the collection. `render.sh` prints "broken cinematic materials:"; it must be empty.
- Python cannot reach World Position Offset or customized UVs (`MaterialProperty` lacks them): upright effects are
  crossed cards that fade where they are seen edge-on (`M_Intro_RingFlame`).
- `intro_shots.rebuild_in_place(shots, ids)` rebuilds only those shots' subsequences at their timeline length, so a
  patch render (`build_render_preset(..., frame_range=...)`) shows a change without rebuilding the master. The
  `__ONLY__` film step rewrites the master with only those shots: always follow it with a full build.
- Ring clocks are sticky across shots. `"mpc_run_on": [names]` keeps a clock running through a shot from where the
  previous shot left it; park clocks at 60 s where a map shot must show no fire (5.4, 9.2).
- Tanks: `"snap"` seats every key on the ground under the four track corners (height, pitch, roll; a corner trace
  far off the others is replaced by the median); `"drive": <play rate>` plays the forward track animation and
  pans the track material (the game sets `PlayRate` at run time). Two-key moves are densified first.
  `"trace_from"` starts the ground traces low (under a gateway lintel). `"contact": True` adds a soft black blob
  under the hull (`M_Intro_ContactShadow`): far from the world origin the sun's shadow starts behind the tracks and
  the tank reads as hovering even when the geometry sits on the ground.
- The Purple Death grows by baked stages, not a reveal mask: `blender/sets/build_mould_stages.py` (then
  `python bake_mould_relief.py`) writes `SM_Intro_Mould_<piece>_S<n>` for 44 stage sizes; `intro_purple.STAGE_LO`
  must match its `STAGES`, and `intro_manifest.MOULD_STAGES` their count. A mask edge may only fall where a mesh lies on
  the ground: anywhere else it cuts slabs, spikes and specks, and it cut every vein off blunt.
- Judge a look change blind: A/B sheets of the same frames in random order, three independent judges, majority per
  adjective decided before running. Open-ended "be sceptical" reviews gave a different bar every round.
- Swapping a mesh mid-shot: spawn both for the whole shot and hand over in the material on the same clock (the
  paper tags: `VanishAt` >= 0 hides from that Char on, < 0 shows from -VanishAt on). A spawnable that appears
  mid-shot pops late and smears with motion blur on its first frame.
- `extras()` (material instances) is separate from `build_text_material()`: run it after any parent rebuild that
  adds a parameter, or the new instance values do not stick.
- A set builds standalone when its own sublevel is the editor world (`load_map(<Intro1976>/<set>)`, then the
  `build_*` function): `Set` skips `set_level` and `finish_level` saves only that level, so the mission maps are never
  loaded or locked. Build `Intro_Skyline` / `Intro_SkylineRuin` this way after any `intro_berlin` plan change, then
  rebuild 3.3 / 3.4a / 3.4b (the falling shells are spawnables of 3.4b from the same plan).
- Re-adding sounds without a full build: remove the master's "Music" and "Effects" audio tracks, then
  `intro_sound.add_sounds(timeline shots, cues)` (it adds new tracks every call).
- A shot's `"actors"` may be a function of the shot's final length (`lambda seconds: [...]`, 5.1 canisters).
- `"attach": (parent label, socket)` rides another actor of the same shot (a Mosin on a marcher's `GunSocket`);
  its keys are then relative to that socket. `"kit_scale"` on a tank class makes a scale model (2.4 miniatures):
  offsets and parts shrink together. The IS-2 kit is in `TANK_KITS` (parts at the Blueprint's 1.1 scale).
- Infantry skeletal meshes face their +Y: yaw -90 walks them along +X. The march cycle's natural speed is
  190 cm/s (`USquadUnitAnimInstance::MarchMontageNaturalSpeed`); key the soldiers at that speed.
- The Ural gate (6.3) closes only after the last tank's rear (x > -380) and the last rank are through; check
  `URAL_GATE`, `URAL_TANK_*` and `ural_platoon` together when the shot length changes.
- `S_Medium_Engine_Rus` does not export to wav (the offline mix never had it); use exportable waves for new cues.
