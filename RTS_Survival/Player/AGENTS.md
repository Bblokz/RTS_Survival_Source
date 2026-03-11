# Player Controller Orchestration Guardrails

## ACPPController is the authoritative gameplay orchestrator

`ACPPController` is the authoritative bridge between player intent and gameplay execution. In this folder scope, treat it as the top-level orchestrator for:

- selection lifecycle and selected-unit state,
- command issuance and dispatch,
- camera-context-aware action interpretation,
- build / placement initiation and confirmation,
- top-level UI-to-gameplay handoff.

When in doubt, route player-issued actions through existing `ACPPController` command entry points instead of introducing parallel pathways.

## Do not break

- Keep ability enqueue/decode flow consistent with `Docs/Abilities.md`.
- Keep selection state transitions and command dispatch atomic.
- Avoid adding direct gameplay side effects in UI callback handlers; route through existing controller command entry points.
- Preserve pointer-validation/reporting pattern for controller-owned members.

## Change checklist

Before finishing changes in this scope, verify impact on:

- `RTS_Survival/Player/CPPController.h`
- `RTS_Survival/Player/CPPController.cpp`
- `RTS_Survival/Player/Formation/*`
- `RTS_Survival/Player/ConstructionPreview/*`
- `RTS_Survival/Player/PlayerEscapeMenuManager/*`

If you modify any of the above, re-check orchestration boundaries and ensure no UI callback bypasses controller-level command routing.
