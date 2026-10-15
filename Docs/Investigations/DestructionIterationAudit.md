# Destruction during container iteration

Audit date: 2026-09-07.

## Scope

Searched 1,676 readable C++ source/header files for destruction inside loops, direct
container mutations, damage/death calls, and cleanup helpers. Traced candidate
callbacks and container mutations in native code. This is a static source audit,
not a guarantee about every possible Blueprint callback or arbitrary re-entrant call.
Two paths ending in `.h` (`ExplosionTypeSystems.h` and `EnemyWorldPersonality.h`)
were verified to be directories, not unreadable headers. Blueprint assets and
plugin source outside this Source workspace were not covered.

## Findings and changes

| Area | Hazard | Change |
| --- | --- | --- |
| Placement effects | Destruction can change the tracked effects array. | Existing `Swap` into a local array was verified and retained. |
| Squad controller teardown | `ASquadUnit::EndPlay` calls `UnitInSquadDied`, which removes from `M_TSquadUnits`. | Iterate a snapshot while retaining live squad bookkeeping for callbacks. |
| Radixite growth teardown | Registered `OnDestroyed` handler removes from `M_GrowthNodes`. | Iterate a snapshot while allowing the handler to update the live nodes. |
| Behaviour cleanup and ticking | `OnRemoved` invokes Blueprint; an `OnTick` callback can destroy the owner and clear behaviours through `EndPlay`. | Detach cleanup batches, snapshot ticking, validate membership, and unregister individual behaviours before removal callbacks to avoid duplicate removal. |
| Mission triggers | Mission Blueprint can remove registrations during an overlap callback; the old handler subsequently accessed the invalidated reference/index. Destruction loops also retained indices across callbacks. | Resolve registrations by weak object identity, unregister before destruction, snapshot removal batches, and never reuse the registration reference after the callback. |
| Building expansion attachments | Actor detachment/destruction exposes teardown callbacks while iterating live attachments. | Detach the cleanup batch and revalidate after detachment. |
| Nomadic building attachments | Attachment/effect teardown can re-enter cleanup of live arrays. | Detach actor, Niagara, audio, and navigation cleanup batches; clear audio tracking as part of batch transfer. |
| Editor spline preview | Destroying child actor components can invoke child actor teardown callbacks. | Detach the preview batch before destruction. |
| Global ability markers | Reverse indices remain vulnerable if component teardown changes tracked markers. | Detach the marker batch before destruction. |

The latter cleanup changes are preventive hardening of callback boundaries, not
claims that a reproducing Blueprint was found for each one.

## Patterns retained

- Cargo damage and scavenging already iterate local squad snapshots.
- Mission actor searches and Blueprint utility actor searches use local result arrays.
- Procedural inspection actor destruction does not remove entries from the iterated settings arrays in the inspected native paths.
- Reverse removal is appropriate where only the current entry is removed and no callback mutates the backing container.
- The inspected spatial/off-map audio pools have separate active-index lists; no native completion callback removes entries from the iterated instance arrays.
- World division teardown unbinds the manager's strength delegate; no native division-destruction removal callback was found for the iterated division array.

## Validation

- `RTS_SurvivalEditor Win64 Development` built and linked successfully with the project's Unreal 5.5 engine.
- UnrealHeaderTool passed with warnings treated as errors.
- `git diff --check` passed.
- PIE callback scenarios were not executed. Useful runtime checks include destroying a populated squad/Radixite owner, destroying a behaviour owner from its tick, removing mission triggers from their own callback, and re-entering attachment cleanup from actor teardown.

Snapshots stabilize container storage, not the lifetime of the objects inside it.
Validate each object immediately before use. Where callbacks still need live
bookkeeping, copy the batch; where cleanup relinquishes the batch, swap it out
before the first callback rather than emptying the live array afterward.
