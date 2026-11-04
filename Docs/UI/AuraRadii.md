# Aura radii: command, ammo resupply, heal and range

Four auras have their own procedural ground shader. The command and ammo auras show a rising icon over every unit
that enters them; the range aura shows one too.

| Aura | Units | Component | Radius type | Material | Rising icon |
|---|---|---|---|---|---|
| Command | Panzer III J / Panzer IV Commander | `URadiusCommandAura` | `FullCircle_CommandAura` | `RadiusMaterials/M_CommandAuraRadius` | `CommanderBoost` (`T_Beh_BeingCommanded`) |
| Ammo resupply | Sd.Kfz. 250 / 251 Ammo Carrier | `URadiusAOEBehaviourComponent` (`AmmoResupplyAura`) | `FullCircle_AmmoResupplyAura` | `RadiusMaterials/M_AmmoResupplyAuraRadius` | `AmmoResupply` (`Behaviours/AmmoResupply/T_Beh_BeingResupplied`) |
| Heal | Sd.Kfz. 251 Medic / Sd.Kfz. 250 Armoured Ambulance | `URadiusHealAura` | `Fullcircle_HealAura` | `RadiusMaterials/M_HealAuraRadius` | none (see below) |
| Range | HQ Radar expansion (`Ger_Bxp_HQRadar`) | `URadiusImprovedRangeAura` | `FullCircle_ImprovedRangeArea` | `RadiusMaterials/M_RangeAuraRadius` | `RangeBoost` (`T_Beh_RangeBeingImproved`) |

All four materials are mapped in `DefaultGame.ini` under `RadiusPoolSettings` `TypeToMaterial`. They replace the grid
instances `MI_CommandingRadius` (command and ammo) and `MI_RangeImprovedRadius` (heal and range), which are no longer
mapped but left on disk. Those are instances of the grid material with a centred icon texture; it is Masked, so the
icon's soft alpha was cut to hard steps. The repair, pulse repair, reinforcement and radiation auras still use
`MI_RepairRadius`.

## The shaders

Unlit, Alpha Composite, two-sided, depth test on (the host vehicle and terrain hide them like the grenade fuse warning).
One Custom node does everything: `p = (UV - 0.5) * 2 * RadiusCm` is the pixel in cm from the aura centre, because the
radius pool scales the 100 cm engine plane to the diameter and sets `RadiusCm`. Strokes are antialiased with
`fwidth`, so they stay crisp at any camera height. The host vehicle covers the centre, so every motif sits on the ring.

- **Command:** blue wash that strengthens towards the edge, rings of orders that leave the commander and fade before
  the edge, and a blue and white border with a thin gold line inside it. Blue and gold are the colours of the
  commander's behaviour icons.
- **Ammo resupply:** warm wash and an ammo belt round the edge (dark red belt, brass rounds pointing out, feeding
  slowly). Nothing inside the circle. Red and brass are the colours of its host icon.
- **Heal:** green wash that beats like a heart (two quick beats, then rest), and a green band of white roundels with
  red crosses round the edge, turning slowly; the band lines glow with each beat.
- **Range:** teal wash, dashed range rings at one and two thirds of the radius, a radar sweep (one turn every 8 s
  with a fading trail) for the HQ Radar, and a rangefinder scale on the edge (minor ticks every 2 degrees, major every
  10).

Tried and removed at the user's request (2026-11-04): rank stars round the command aura, supply lanes of travelling
rounds and a large stripper clip at the centre of the ammo aura.

Parameters: `RadiusCm` (set by the pool) and `Brightness` (emissive multiplier, default 1.6). Motif size follows the
radius, so small auras stay readable.

Rebuild with `Tools/AuraRadii/build_aura_radius_materials.py` (UECP `execute_python`, `exec(open(...).read())`).
A new material is built from scratch; an existing one only gets its Custom node code replaced, because pooled radius
actors may hold it. Preview without PIE by drawing the material into a render target
(`RenderingLibrary.draw_material_to_render_target`, through a dynamic instance to set `RadiusCm`) and exporting it.
The running editor reads `TypeToMaterial` at startup; to try a new mapping without a restart, set it on the
`/Script/RTS_Survival.RadiusPoolSettings` default object.

The aura component attaches the disc yaw-only (`AttachRTSRadiusToActorYawOnly`): with the full rotation, a host
pitched a few degrees on a slope tilted the far edge of the disc into the ground. This applies to every
`URadiusAOEBehaviourComponent` aura.

## Rising icons instead of text

The aura component shows the feedback when a unit enters (`ApplyOnlyOnEnter`): `AOEBehaviourSettings.IconSettings`
has Use Icons on and `TextSettings` Use Text off on the command, ammo and range auras. The commanders override the icon
animation with the timing their text had (2 s visible, 0.5 s fade, 125 cm rise); the others use the catalog default.
`URadiusCommandAura` now also defaults Use Icons to on. The range aura had no feedback before.

The heal aura has none: it applies `ApplyEveryTick` at a 1 s interval, so an icon on every unit in range would rise
every second.

The icon catalog is `DA_VeticalAnim_Icons` (the asset `AnimatedIconSettings` points at, 48 x 48 entries).
`T_Beh_BeingResupplied` is drawn by `Tools/AuraRadii/compose_ammo_resupply_icon.py` from the exported
`T_Beh_LifeRegen` plate (cross painted out, recoloured amber) with three cartridges and a plus; its PNG is kept next to
the script. `Tools/AuraRadii/setup_aura_feedback.py` imports it, adds the catalog entry and switches the aura
components; it is idempotent.
