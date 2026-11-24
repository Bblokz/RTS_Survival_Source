# Frozen Lake: static mesh prompts for delegated modelling

Prompts for other AI agents to start the static meshes of the Frozen Lake mission (Lake Vygozero, Act 3 opener) before
the map is built. Written 2026-11-17 from the state of [FrozenLakeMission.md](FrozenLakeMission.md) and layout sketch
A (`MapDesigner/FrozenLake/out_A/layout.png`).

How to use: give an agent **one** prompt from "The prompts". Each prompt tells it to read "Shared rules" below first.
Every agent works in its own subfolder of `D:\Blender\BlenderProjects\BlenderTanks\AA_LevelSpecific\FrozenLake`.

## What can start now, and what waits

Can start now (the design doc fixes enough for these). In priority order:

| # | Kit | Why it can start | Subfolder |
|---|---|---|---|
| 1 | Cryo pump station (intact + destroyed) | Four pumps, destroy-only, sizes fixed by the sketch | `CryoPump` |
| 2 | Forge drill rig + sedated core | The main objective; platform size and exits fixed | `DrillRig` |
| 3 | Ice kit: pressure ridges, floes, hole rims, freeze-in collars | Needed everywhere on the lake | `IceKit` |
| 4 | Forge dam + spillway + canal bridge | The north shore is fixed (v2) | `ForgeDam` |
| 5 | Ice road and lane kit | Heavy ice road and player lanes are decided | `IceRoadKit` |
| 6 | Blown gorge bridge (blown + repaired) | Repair by player engineers is decided | `GorgeBridge` |
| 7 | Burnt Forge convoy (cryo-tank trucks) | The POI is kept | `BurntConvoy` |

Kits 1 and 4 share one interface: pump 3 sits inside the dam. Both prompts state it, so they can run in parallel.

Waits (not delegated yet):

- **Shield HQ and AI base buildings.** The AI builds its base from the existing Soviet building set.
- **Landing field dressing and the radio mast.** Whether the mast exists is still open.
- **Core carrier (heavy unit) and cryo-troopers.** These are units, not static meshes, and their design is open.
- **Ice surface, thickness colours, cracks, melt front, steam.** Landscape material and VFX work, not meshes.
- **The 1946-47 wreck hulls and the King Tiger.** Reuse the game's tank meshes; only the ice collars around them are
  new (kit 3).
- **Dead forest and shore rocks.** Reuse the Ore Mountains dead forest kit
  (`CustomMapItems/Environment/OreDeadForest`) and existing rock kits; a frost pass on them is material work.

## Shared rules (every agent reads this first)

### The game and the mission

- RTS in Unreal Engine 5, alternate history, set in **1976**. Radixite (a living crystal root network) made the
  world a hot, scorched desert; lakes have boiled away.
- Mission: **Lake Vygozero, Karelia**, on the White Sea-Baltic canal. The Soviet **CSD** faction's **Director of
  Forge** (huge foundries, heavy thermal shielding, cryo-suits, Radixite weapons) keeps a bay of the lake frozen with
  four **cryo pumps** so it can sedate and drill the Radixite vein under it. Snow and frost reach only as far as the
  pumps; beyond, the ground is the dry brown of the rest of the game.
- The player is German. The guard force is the **Director of Shield** (Soviet military).
- Read the design doc for context: `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMission.md`, and look
  at the layout sketch `D:\UE5Projects\RTS_Survival\MapDesigner\FrozenLake\out_A\layout.png` (586 x 586 m; +X north
  is up, +Y east is right; axes in metres). Lore: `D:\UE5Projects\RTS_Survival\Source\DesignTexts\Lore.txt`.

### Look

- **Forge** pieces are heavy Soviet industry of the 1940s-70s retrofitted with cryo-tech: riveted steel, cast
  concrete, thick insulated pipes, frost and hoarfrost crusting every cold part, icicles, frozen spills. They should
  read as engineering, not as military bunkers (that is Shield's look).
- **Cold reads white and pale blue; Radixite heat reads as an orange glow.** Keep anything that glows on its own
  emissive material slot, so its colour can be matched to the game later.
- Worn and old, not new: 30 years after the war. No logos or invented emblems. Short Russian stencils in Cyrillic are
  fine (for example "НАСОС 3", "pump 3"); check spelling.

### Camera and scale

- Top-down RTS camera about **40 m above the ground, pitch -52 to -70 degrees**. The silhouette from above, the roof or
  deck, and the cast shadow matter most; undersides hardly show. Nothing should hide units for long: keep tall parts
  slender or off to one side.
- Real scale in **centimetres**, Z up, +X north. Scale blocks in every render: a **6.5 m x 3.5 m x 2.5 m** tank block
  and **1.8 m** soldiers. A King Tiger is about 7.4 m x 3.8 m.
- No map sun yet. Render with a sun from the north-west, about 45 degrees up.

### Workflow

1. **Make your subfolder** in `D:\Blender\BlenderProjects\BlenderTanks\AA_LevelSpecific\FrozenLake` (the name is in
   your prompt). Work only there. Do not change other folders, other agents' subfolders or the Unreal project.
2. **Write `<Name>_Brief.md`** in your subfolder: the goal, the constraints from your prompt, and a log of what you
   did, what you checked and what is open. Keep it current.
3. **Concept round first.** Build greyboxes for the number of options your prompt asks for. Render one sheet per
   option to `Concepts/v01/`:
   - the game camera view (pitch -55) with the scale blocks;
   - a low three-quarter view;
   - a plan view with dimensions;
   - the part list with sizes and estimated triangle counts.

   Then add an `overview.png` comparing the options, a recommendation in the brief, and **stop**. Do not model
   production meshes until the owner picks.
4. **Production (only after the owner's pick):**
   - One material family per kit; textures 2048 or 4096, PBR (BaseColor, Normal, ORM). Atlas small pieces.
   - Name meshes `SM_FL_<Kit>_<Part>`. Pivot at the bottom centre, on the ground or ice plane, facing +X.
   - Collision as `UCX_` convex hulls. Triangle budgets as in your prompt.
   - Save the `.blend` and export one FBX per mesh to `Export/`. Render a lineup and an in-game-camera shot.
   - Keep all scripts in `_tools/` so the work can be rebuilt.
5. **Tools:**
   - Blender 5.0 for headless renders: `D:\Blender\BlenderSoftware\5\Blender5.0.exe -b --factory-startup --python <script>`.
   - Blender 4.0 for production: `D:\Blender\BlenderSoftware\4.0\blender.exe`.
   - Example of a finished concept round: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\A4Viaduct\A4Viaduct_Brief.md`.
6. **Do not invent mechanics.** If the prompt does not say how something plays, put the question in the brief.
7. Report what you did not verify.

## The prompts

### 1. Cryo pump station (subfolder `CryoPump`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\CryoPump`.

Design the Forge **cryo pump station**: four of them stand on the lake shore, and each keeps one zone of the lake
frozen. They are the mission's main decision: the player can destroy a pump (never capture it), and its zone then
melts into open water over about 40 s, drowning anything on it.

- **What it does, visibly:** draws lake water in, chills it in cryo plants (frosted tanks, compressors, cooling
  fins), and pushes chilled brine out under the ice through two or three thick, frost-crusted pipes that run 10-20 m
  out onto the ice and dive under it. The pipes leading onto the lake are what tells the player "this pump holds that
  ice".
- **Footprint:** pumps 1, 2 and 4 stand on a round pad 26 m across. **Pump 3 is built into the Forge dam** on the
  north shore and has a 20 m pad: make it a variant that fits a **20 m wide bay in the dam**, reached from the ice
  side only (another agent builds the dam to leave that bay).
- **Height:** up to about 14 m. A slender stack or condenser may go higher if it is thin.
- **States:** intact, and destroyed (burst tanks, collapsed hall, ruptured pipes with the ends lying on the ice). Add
  6-10 loose debris chunks. Add empty sockets for explosions, frost vents and steam.
- **Budget:** about 15-25k triangles intact; the destroyed state about the same.
- **Concept round:** 3 options, plus the pump 3 dam variant of each, shown at least in plan.

### 2. Forge drill rig and sedated core (subfolder `DrillRig`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\DrillRig`.

Design the **Forge drill rig**, the mission's main objective. It stands on a steel platform on the ice in the middle
of the lake, over the Radixite vein. The player captures it, then hauls a cryo-sedated Radixite core home on a heavy
carrier.

- **Platform:** about 46 m across (round or many-sided), deck about 1 m above the ice, on frosted piles. It needs
  **three ramps down to the ice**, gentle enough for heavy tanks (6-8 m long):
  - **south**, bearing about 190 degrees, the direct haul home;
  - **west**, bearing about 282 degrees, a short way to the heavy ice road;
  - **north-east**, bearing about 40 degrees, the Forge ice road to the Shield base.
- **On the deck:**
  - a **drill derrick** over the vein, slightly north-east of centre. Up to about 20 m high, slender lattice so units
    behind it stay visible;
  - a cryo plant that feeds the drill (frosted tanks, pipes);
  - the **core cradle** near the south ramp;
  - two raised mounts for anti-aircraft guns (the guns are existing units; build only the mount, sandbags or rails);
  - walkways, railings, and a hazmat crew corner.
- **The sedated core:** a **separate mesh**, because it is lifted onto a carrier. A Radixite crystal core about
  4 x 2 x 2 m in a steel frame, crusted with frost, with the orange glow showing through cracks in the frost (glow on
  its own emissive slot). The cradle comes in two meshes: empty, and holding the core.
- **Look:** the vein glows under clear black ice next to the platform. Do not model that (it is the ice material),
  but leave the drill shaft open down to the ice.
- **Budget:** about 40-60k triangles for the whole rig; the core about 3k.
- **Concept round:** 3 options.

### 3. Ice kit: pressure ridges, floes, hole rims and freeze-in collars (subfolder `IceKit`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\IceKit`.

Design the **ice kit** for the frozen lake. The flat ice itself is a landscape material, so it is not part of this
kit. Pieces:

- **Pressure ridges:** jumbled walls of tilted ice slabs, 2-4 m wide and 1.2-2.5 m high, with snow drifted on one side.
  - Modules 6, 10 and 14 m long, plus two end caps. On the map they chain into five ridges of 30-40 m.
  - They block vehicles (collision) and are the only infantry cover on the lake, so give them a few low spots
    infantry can crouch behind.
- **Floes and open-water edges:**
  - 6 broken floes, 1-6 m across, for flooded zones and open leads;
  - 3 lead-edge strips, 10 m long, of the broken rim where ice meets open water;
  - 3 shell-hole rims, 3, 5 and 8 m across. Whether holes are meshes is still open in the design, so keep the rims
    simple.
- **Freeze-in collars:** rings of heaved, cracked ice and frozen spray that sit round a hull frozen into the ice. Do
  not model the tanks; use scale blocks. The collars:
  - one for a **King Tiger** (hull about 7.4 x 3.8 m) frozen into **shore ice** at the lake edge, next to frozen
    ground. It is an optional objective: scavengers free it. Also make a "freed" version (broken open, chunks pushed
    aside);
  - medium hull (about 7 x 3.5 m) and heavy hull (about 8.1 x 3.4 m, a Ferdinand or IS-2) collars in lake ice, for the
    1946-47 wreck bay;
  - one **crystal heave**: ice pushed up and cracked round a Radixite crystal cluster about 6-10 m across (the crystals
    are existing game meshes; make only the ice).
- **Look:** white to pale blue slabs, glassy broken faces, snow on top surfaces. One material family.
- **Budget:** 300-2,000 triangles per piece; collars up to 3k.
- **Concept round:** 2 looks for the whole kit (for example sharp glassy slabs or rounded snow-buried slabs), each
  showing every piece type.

### 4. Forge dam, spillway and canal bridge (subfolder `ForgeDam`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\ForgeDam`.

Design a **modular dam kit** for the north shore. The Forge cut its frozen bay off from the rest of Lake Vygozero with
this dam. Beyond it the main lake has steamed away to a dry, cracked lakebed.

- **History for the look:** the White Sea-Baltic canal of the 1930s was built largely of **timber cribs, earth and
  stone**, because steel and concrete were scarce. The Forge reinforced the old works with concrete, steel and cryo
  pipes. Show both layers.
- **Layout:** two curved arms, about 140 m (west) and 125 m (east), following the shore. On the sketch the dam line is
  6 m thick; the full embankment may be wider. It **blocks all movement**, so it is a wall: about 4-6 m above the ice
  on the lake side.
- **Pieces:**
  - a straight segment about 10 m long, and bend pieces (the arms turn in 2.5-degree steps of about 10 m);
  - **end abutments** at the spillway;
  - a **sluice structure** for the spillway gap, about 35-40 m wide. The gap must stay **passable**: an enemy column
    comes down the frozen spillway through it. So the gates are open or broken, and any gantry over the gap is at
    least 6 m above the ice;
  - a **pump 3 bay**: a 20 m wide recess in the west arm, open to the lake, where another agent's pump station
    variant sits;
  - the **canal bridge**: a road bridge 10 m wide over the steaming spillway canal (15 m wide) north of the dam. It
    must carry heavy tanks.
- **Budget:** 1-3k triangles per segment; sluice and bridge up to 10k each.
- **Concept round:** 2 options. Each shows a 60 m stretch with the pump 3 bay, the spillway sluice, and the canal
  bridge.

### 5. Ice road and lane kit (subfolder `IceRoadKit`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\IceRoadKit`.

Design dressing for two kinds of route over the ice.

1. **The heavy ice road** (Forge, enemy-built). One road about 10 m wide and about 620 m long, of Forge-thickened ice.
   Heavy tanks can cross the lake only here. It must read as "safe, maintained road" from the game camera:
   - edge marker poles every 15-20 m (frosted, with red flags or reflectors);
   - a frosted cryo feed pipe on low trestles along one side, with a nozzle station every 50-80 m (this is what keeps
     the road thick);
   - a few Cyrillic hazard and weight-limit signs;
   - optional: a road gate for the Shield end.
2. **The player's prepared lane** (German engineers). Engineers lay timber and flood the ice to thicken it, so heavies
   can cross elsewhere. Make **timber mat modules** 10 m wide, 4 m and 8 m long, plus an end ramp. They should look
   improvised and freshly laid, with frozen water spilled over the planks. Small props: a hand pump with hose, sledges,
   timber stacks, barrels.

- **Look:** keep the two clearly apart: Soviet industrial versus German field engineering.
- **Budget:** 100-1,500 triangles per piece. Atlas the kit.
- **Concept round:** 2 options, each with a 40 m stretch of road and a 20 m stretch of lane.

### 6. Blown gorge bridge, blown and repaired (subfolder `GorgeBridge`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\GorgeBridge`.

Design the **south road bridge** over pump 1's outflow gorge.

- **The site:** warm outflow water from pump 1 has cut a **steaming gorge about 18 m wide** from the lake to the
  south map edge. Assume about 4 m deep and note it as an assumption. The south shore road (9 m) crossed it on a
  pre-war Soviet road bridge with a span of about 22 m.
- **States:**
  - **Blown:** the span has dropped into the gorge and the abutments stand with broken stubs. It must be clearly
    impassable.
  - **Repaired:** German engineers rebuilt it as a field repair: timber cribs on the fallen span, steel beams, a plank
    deck at least 9 m wide. It looks improvised but carries heavy tanks.
  - Optional: 1-2 in-between construction stages, if the game shows repair progress. Ask in the brief; don't assume.
- A Shield picket (anti-tank gun and squad) holds the far side. That is placed later; leave room for it on the east
  abutment.
- **Budget:** about 10-15k triangles per state.
- **Concept round:** 2 options for the original bridge type, each in both states.

### 7. Burnt Forge convoy (subfolder `BurntConvoy`)

Read "Shared rules" in `D:\UE5Projects\RTS_Survival\Source\Docs\Campaign\FrozenLakeMeshPrompts.md` first, then work in
`...\AA_LevelSpecific\FrozenLake\BurntConvoy`.

Design a **burnt Forge supply convoy** for a point of interest at the frost edge in the south-east. It holds intel on
the pump zones.

- **Contents:** 3-4 scorched trucks with insulated **cryo tanks** on their beds, burst open (frost-burst ruptures,
  peeled insulation), half on frozen ground and half on scorched ground. Add scattered drums and a crate or map case
  where the intel is found.
- **Base the trucks on an existing Soviet truck** if there is one in `D:\Blender\BlenderProjects\BlenderTanks\Trucks`.
  Copy it into your folder; do not edit the original. Say in the brief which one you used.
- **Budget:** about 8-12k triangles per truck wreck.
- **Concept round:** 2 options for the cryo tank body and the burn damage.
