# Radiant Ruins: repeatable Shorts formats and production plan

Research date: 30 October 2026. Scope: online format research and a plan using the existing local Shorts Creator. No renders, game changes, uploads or scheduling were performed.

## Recommendation

Build editorial series around a clear question, an escalating test or a quick surprise. Keep the existing rendering presets as the capture layer. A viewer-facing template should specify the opening promise, sequence of evidence and ending, rather than only the camera and army composition.

Start with **What can stop it?**, **Does this actually work?**, **The eight-second surprise**, and **Choose the best sound**. These have the strongest combination of relevant public examples and low production cost. Retain ordinary showcases as controls while testing them.

The strongest accessible numerical examples were from War Thunder. Gates of Hell and Men of War were searched for closer RTS references, but accessible results did not establish comparably strong Shorts performance. Do not interpret this as evidence that RTS Shorts cannot succeed. It means the strongest evidence below comes from an adjacent audience, so transfer to Radiant Ruins remains a hypothesis.

## Public evidence

Counts below are approximate indexed public totals returned during research, not live analytics. Index dates vary. Direct video-page fetching was unavailable, so this is a title/metadata and public-performance study, not a frame-by-frame viewing audit. We cannot see traffic sources, paid distribution, private retention or conversion. A large publisher's total is evidence of audience interest, not a forecast for this channel.

| Reference | Published | Indexed views | What it supports |
|---|---|---:|---|
| War Thunder official: [Train vs everything else](https://www.youtube.com/watch?v=_n3sFPkM1Hc) | 15 Aug 2022 | 7.75 million | A simple recurring adversary/test premise can attract a large military-game audience. |
| War Thunder official: [Train vs everything else, part 2](https://www.youtube.com/watch?v=T8AaPdoNqsI) | 5 Nov 2022 | 30.99 million | A second successful entry strengthens the case for the repeatable series format. It does not prove why viewers watched. |
| War Thunder official: [Variety of calibers](https://www.youtube.com/watch?v=MsgQblzmNL0) | 13 Jun 2022 | 6.88 million | A visually understandable weapon comparison has broad reach. |
| TheMilitaryTok: [Mythbusting Various War Thunder Myths](https://www.youtube.com/watch?v=9q0fECj2eJ0) | 13 Jul 2025 | 1.43 million | Question-and-test packaging is a strong candidate. |
| TheMilitaryTok: [Mythbusting the Most Intriguing War Thunder Myths](https://www.youtube.com/watch?v=krimVGTRD08) | 15 Oct 2025 | 550,000 | Another successful entry provides evidence beyond a single myth video. |
| Munitions: [Biggest Calibers in War Thunder](https://www.youtube.com/watch?v=1EMo1sK3uO0) | 24 Nov 2024 | 188,000 | Caliber comparison also has a relevant creator example outside the publisher channel. |
| Munitions: [Machine Gun Sound Effects](https://www.youtube.com/watch?v=TRj3PlDmp0I) | Date not returned | 84,000 | Sound-led comparisons have audience interest, although this example alone is not a breakout hit. |
| Helmut Gaming: [Win more games by angling your armor](https://www.youtube.com/watch?v=sz_OsgHVuwc) | 29 Mar 2023 | 51,500 | A mechanic-led short has a relevant precedent; the result is modest compared with the examples above. |

A recent secondary snapshot adds useful context: vidIQ lists TheMilitaryTok's eight-second **“He Thought He Was Hidden…”** at about **1.27 million views**, versus a **176,000 median** across its latest 48 Shorts, approximately 7.2 times that median. The snapshot is dated 28 October 2026; uploads have unequal time to accumulate views. This supports testing very short surprise clips, without establishing that duration caused the result. [Source](https://vidiq.com/youtube-stats/channel/%40themilitarytok/videos/).

YouTube's own creator discussion recommends making the opening compelling immediately and building a compact story around a moment. The suggested timings below are our production hypotheses, not platform rules. [YouTube's Shorts discussion](https://blog.youtube/creator-and-artist-stories/youtube-shorts-deep-dive/).

## Verified production capabilities

Reviewed local documentation, existing job/setup files and relevant implementation. These are source-confirmed capabilities; no new live render was used to validate a proposed format.

| Existing capability | Useful for |
|---|---|
| Shorts Studio: lineups, complete per-shot settings, ordering, captions, saved recipes, render queue | Most pilots can start as new recipes without replacing existing presets. |
| TankDuel, TankBattle, SquadBattle; mixed units and separate formation offsets | Escalation, underdogs, composition and positioning experiments. |
| Demolition with a destructible apartment target | Destruction spectacle and controlled weapon comparisons. |
| SideProfile, weapon showcases, four-way stacking with active-strip highlighting | Sound tournaments and caliber ladders. Four-way export requires exactly four shots at 1080 x 1920 with SideProfile. |
| Vehicle/pit/squad orbits, firing and impact camera cuts | Weapon reveals and short demonstrations. |
| ParatrooperBattle and multiple aircraft passes | Drop sequences and combined-arms stories. |
| EditorialOverlayRecipe and the separate ShortsEditing tool | Timed question, explanation and result callouts after rendering. Existing audio is copied unchanged by the overlay tool. |
| Battle decision log containing frame, winner and survivor count | Reliable results and the basis for future automatic summaries. |

Constraints that affect the plan:

- Ordinary battle/duel stages are Meadow and DesertBattle. Convoys use the prepared DesertBattle driving corridor. Demolition retains its dedicated stage.
- Convoy mode drives formations without firing. It is not currently an automatic convoy-ambush mode.
- The current paratrooper opening is fixed at two seconds, and the dashboard rejects trimming it through its lead-trim setting. ParatrooperMeadow also applies double weapon range. Use those jobs for spectacle rather than ordinary-range balance claims.
- Some battle presets override turret speed and effects. Some demolition presets reduce target health and enlarge splash radius, including per-unit overrides. A fair gameplay comparison needs an explicit audit of the fully resolved job, not just a neutral-looking caption.
- Current overlays add timed text; they do not choose highlights, detect kills, create arbitrary montages or provide an automatic moving scoreboard. Fractions of clip duration are useful for general callouts, not proof of when a battle event occurred.
- UnitYawOffset rotates the battle's common facing relationship. Independent target hull angle for a controlled armor-angling test needs a separate capture control; the existing common yaw is insufficient.

## Ten editorial templates

### 1. What can stop it?

**Priority:** first batch. **Evidence:** strongest; repeated “X versus everything” publisher examples. **Length to test:** 20–30 seconds.

Opening: **“How many T-34s stop one Tiger?”** Show the recognizable heavy and the numerical challenge immediately.

Sequence: fresh 1v1 test, fresh 1v2 test, fresh 1v4 test; each gets one readable outcome. Preserve a short setup and the decisive exchange, then reveal the final result. Close with a specific next test such as “Try an IS-2 next?”

Production: duplicate a suitable battle recipe; keep the main vehicle fixed and change BattleEnemyClassPaths. Each segment starts a fresh encounter, so caption it as separate rounds. Render longer source battles if needed; use an editorial cut to reach the target runtime rather than simply capping unresolved fights.

Series variants: one AT gun against increasing light armor; one heavy against increasing mediums; a squad challenge against one vehicle. Keep exact unit variants and ammunition visible.

**Work:** recipe now; tightly edited result montage needs manual post work or the proposed cut-list addition. Do not imply a surviving champion carries damage across rounds unless that behavior is implemented.

### 2. Does this actually work?

**Priority:** first batch. **Evidence:** strong; two successful mythbusting examples. **Length:** 18–30 seconds.

Opening: **“Same tank. Different shell. Different result?”**

Sequence: state one claim, show condition A, show condition B, end with the observed answer. Use three repetitions per condition for an actual comparison; a single result should be described as “this attempt.”

Production: start from TankDuel or a single-unit target shot. Existing jobs contain regular and HEAT T-34-85 variants and special-round annotation support. Verify which other stats the variants change before attributing differences exclusively to ammunition. If other stats differ, call it a variant comparison.

Other tests: one gun at two distances within its normal range; infantry versus armor; ordinary and upgraded vehicle variants. Make the result a question until captured evidence exists.

**Work:** recipes for existing variants/ranges now. Armor-angle, chosen weak-point and module-specific tests need additional aiming/orientation controls and verification before production.

### 3. The eight-second surprise

**Priority:** first batch. **Evidence:** strong recent adjacent example, using a secondary performance snapshot. **Length:** 7–12 seconds.

Opening: a visually understandable situation and one short line, such as **“He brought an anti-tank rifle.”** Payoff: show whether it actually stops the light vehicle, then briefly hold the outcome. Another candidate is a naturally surprising surviving vehicle or demolition result.

Production: use InfantryAT, existing battles or Demolition as source footage. Select the event after the render; show enough before it to make the payoff understandable. Keep original weapon audio. End close to the action rather than adding a long branded outro.

**Work:** possible now with deliberate short captures or manual trims; automatic event selection is new work. Do not promise a funny AI moment in advance or present a staged response as random player behavior.

### 4. Choose the best sound

**Priority:** first batch and low-cost control. **Evidence:** strong adjacent caliber comparison, plus relevant gun-sound example. **Length:** 12–20 seconds.

Opening: **“Which WWII gun sounds best?”** Four weapons fire in a fixed order; each has a short readable name. Finish with the four options still visible and “1, 2, 3 or 4?”

Production: four existing side-profile gun segments into the supported stack exporter. For small arms, use the existing infantry showcase framing and sequential cuts unless a stacked crop has been checked. Keep weapon sound unobstructed by music or narration.

Series variants: 75 / 88 / 122 / 152 mm; four German weapons; four Soviet weapons; one machine gun from each suitable squad. Describe these as **in-game sounds**.

**Work:** existing capture and export; recipes and labels only. This is an editorial variation of a format the tool already supports, so use it as a low-risk comparison against the newer ideas.

### 5. Small unit, big threat

**Priority:** second batch. **Evidence:** adaptation of the challenge premise; the exact Radiant Ruins format is unproven. **Length:** 15–25 seconds.

Opening: **“One squad versus this tank.”** Frame both subjects so the mismatch reads before any explanation. Show the shot, response and real outcome.

Production: mixed-army lists in TankBattle/SquadBattle; existing InfantryAT jobs supply candidate unit classes. Both sides should remain armed for a combat challenge. If using a disarmed target for a firing demonstration, label that setup.

Variants: one AT squad versus a light tank; one AT pit versus a medium; one light tank against heavier armor at a defined range. Avoid claiming “cheap versus expensive” until actual current game costs are checked.

**Work:** recipes now; no new combat behavior required. Keep failures when they are interesting; the template does not require an underdog win.

### 6. From small gun to absurd gun

**Priority:** second batch. **Evidence:** strong caliber-interest examples; destruction-specific adaptation is a hypothesis. **Length:** 15–25 seconds.

Opening: **“85 mm → 380 mm against the same building.”** Three or four rapid comparisons, ending on the largest weapon's impact.

Production: existing Demolition lineup already contains T-34/85, Tiger H1, IS-2 and Sturmtiger against the same apartment class. Use a freshly spawned target each time and consistent framing.

For a measured comparison, normalize target health and splash multipliers to gameplay values across every resolved segment. For a cinematic clip using existing altered values, label it as a staged destruction showcase and avoid “damage test” claims. Do not infer destructive power from caliber alone.

**Work:** existing recipe with settings audit; exact impact highlights need post trimming.

### 7. One placement changes the fight

**Priority:** second batch. **Evidence:** mechanic-led precedent; exact format unproven. **Length:** 20–35 seconds.

Opening: **“Same army. Move one gun.”** Show formation A, then formation B with one support unit repositioned. Explain the actual difference only after repeated trials establish it.

Production: use BattleAllyOffsets/BattleEnemyOffsets with the same unit lists and range. Keep the tested unit identifiable in both shots. Check that the changed placement alters only the intended variable and stays navigable.

**Work:** basic placement variants now. Precisely directed flank maneuvers or an independent target hull angle need capture extensions. Do not sell ordinary formation offsets as a verified armor-angle control.

### 8. The last survivor

**Priority:** opportunistic second batch. **Evidence:** narrative adaptation of challenge/surprise formats; no independent breakout proof collected. **Length:** 15–25 seconds.

Opening: **“Only one tank is still firing.”** Start shortly before that state, show its decisive exchange, reveal the surviving side.

Production: mine existing battle takes and their decision logs. Use retained footage rather than asking the simulation to produce a guaranteed clutch moment. Preserve the real sequence; do not hide a supporting unit that caused the result.

**Work:** manual selection now; first-class survivor-event output would improve automation. Final survivor counts already exist in logs, but a timeline of every death is not part of the inspected post workflow.

### 9. Identify it before the reveal

**Priority:** inexpensive experiment. **Evidence:** relevant quiz precedents exist, but this research did not establish strong comparable Shorts performance. **Length:** 8–15 seconds.

Opening: **“Tiger or Panther?”** Show a recognizable but partial side/front view with identifying labels hidden. Give two or three seconds, then reveal the full unit and name, followed by one shot.

Production: an existing showcase can supply a close camera and a wider reveal segment. Static opening labels and a timed answer use existing caption/overlay tools. A smooth changing crop or full-screen mystery mask needs post work.

**Work:** simple two-shot version now. Do not use misleading “99% fail” statistics or let the caption reveal the answer early.

### 10. You choose the next test

**Priority:** after a series earns actual requests. **Evidence:** community-series hypothesis, not a proven standalone acquisition format. **Length:** 20–30 seconds.

Opening: **“You asked for one Flak 88 versus four T-34s.”** Display a real request or paraphrase it accurately; show the test and result; offer the next specific matchup.

Production: clone the relevant battle recipe and replace the requested units/counts, then use a timed callout. Keep a small request backlog. The comment screenshot itself is optional and needs separate post compositing.

**Work:** recipes and existing text now. Real requests only; no fabricated engagement. This is a continuation mechanism for templates 1, 2 and 5, not a substitute for a strong opening premise.

## Shared production recipe

1. Pick one viewer question and the exact outcome needed to answer it. Define the rules before rendering.
2. Clone a known working lineup/setup into a new saved recipe. Keep existing jobs as masters.
3. Choose a prepared stage. Use a stable shot for causal comparisons, and camera motion when it helps a spectacle reveal.
4. Make the question readable on the first frame through static captions. Save typewriter callouts for the second beat or the result; waiting for a long opening sentence to type wastes the hook.
5. Render a draft and inspect the first second, first shot, decisive event and ending. Check the full resolved job for altered combat values.
6. Capture sufficient source duration to reach the result. An unresolved time cap is not a win or loss; caption it as a timeout or omit it from a result montage.
7. Assemble the shortest clear story. Keep necessary setup and causality; remove empty waiting. Existing overlays alone do not perform that edit.
8. Use short labels and original weapon audio. On the actual phone preview, keep text out of right-side controls, the bottom interface and the central action. Existing margins are a starting point, not proof of every platform's safe area.
9. Add a small Radiant Ruins identifier and one brief wishlist cue at the end/description. Preserve the weapon payoff. For Steam traffic, use campaign-tagged Steam links so conversion can be compared by format where attribution permits it.
10. Review the final MP4, then record the result in the experiment log. Publication is a separate step from this research plan.

Historical labels should distinguish real service vehicles, prototypes and the game's fictional variants. Describe combat results as Radiant Ruins results rather than proof of WWII battlefield effectiveness.

## Small tool additions, in order

These are proposed extensions, not existing functionality or changes made by this research task.

| Order | Addition | Purpose and acceptance check |
|---|---|---|
| 1 | Post-only cut list with in/out points, segment order and optional freeze/short replay | Builds challenge and surprise edits from existing footage. Preview source timestamps; preserve audio sync and reject missing/out-of-range footage. No Unreal rerender for a different edit. |
| 2 | Structured battle result file per segment | Export winner, decision frame, survivor count and timeout status. Convert source events into delivered-video time after trims. Never generate a winner label from duration alone. |
| 3 | Editorial series presets in Shorts Studio | Expose question, subject, opponents, counts, target duration and result text; compose existing capture settings underneath. Validate exactly four shots for the current stack route. |
| 4 | Explicit gameplay-comparison preset | Audit health, cooldown, range, aiming/turret and damage overrides. Keep deliberate cinematic settings visible in the resolved recipe. Does not make staged AI encounters equivalent to normal match statistics. |
| 5 | Independent target/army facing and optional aim point | Enables controlled armor-angle and weak-point demonstrations. Verify hull angle on actual frames and intended hit location before using the claim. |

Implement only the first additions justified by the pilots. The first batch can be made with the current capture system plus manual editing; it does not require a new game system or online editor subscription.

## First twelve publications

Suggested sequence: roughly one per day if that matches the existing channel cadence. These are a production order, not scheduled posts. Prepare a reusable footage bank before starting.

| # | Format | Pilot premise |
|---|---|---|
| 1 | Escalation | One Tiger H1 versus 1, 2 and 4 T-34s, fresh tests |
| 2 | Sound choice | Four cannon sounds with the existing four-way stack |
| 3 | Myth/variant test | Regular versus HEAT T-34-85 variant, with all changed stats identified |
| 4 | Short surprise | Best clear 7–12 second payoff from pilots 1–3 |
| 5 | Escalation | One Flak 88 pit versus increasing light armor |
| 6 | Sound choice | Four infantry weapons, sequential showcase cuts |
| 7 | Myth test | The same gun and target at two normal in-range distances |
| 8 | Short surprise | A demolition payoff with any cinematic modifiers disclosed |
| 9 | Escalation | One IS-2 versus increasing Panzer IV numbers |
| 10 | Sound choice | Four Soviet cannon sounds |
| 11 | Myth/composition test | Same tanks with and without one support gun; label as composition comparison |
| 12 | Short surprise | Best naturally occurring survivor or impact moment from the batch |

For variants and distance tests, do three trials per condition before writing the conclusion. For challenge spectacles, one encounter can be a valid story, but it is not a measured win rate. Reuse source footage across different edits during development; avoid posting several near-identical hook versions back-to-back as if they were independent tests.

## Evaluation and next decision

Before publishing, record the median 48-hour and seven-day results of the last 10–20 comparable Shorts, grouped by duration/format where possible. Channel analytics were not accessed in this task, so no claim is made about why current templates perform as they do.

Track format, premise, duration, date, production minutes, public views, engaged views, chose-to-view/stayed-to-watch rate, average view duration, average percentage viewed, shares, subscribers and attributable Steam visits/wishlists. Use the same age window for comparisons. Public view counting includes starts/replays, so raw views and meaningful watching are distinct measurements. [YouTube's view-count explanation](https://support.google.com/youtube/thread/333869549?hl=en&msgid=355701253), [Shorts analytics guidance](https://support.google.com/youtube/answer/12942217?co=YOUTUBE._YTVideoType%3Dshorts&hl=en).

Suggested internal decision rules, not algorithm thresholds:

- **Repeat:** at least two of the three pilots in a format beat the comparable seven-day engaged-view baseline by 1.5 times, with acceptable production effort and no deterioration in subscriber/Steam conversion per 1,000 engaged views.
- **Revise the hook:** few choose to watch, but those who stay watch most of the story.
- **Shorten or clarify:** the opening gets viewers, but retention falls during setup or before the result.
- **Recheck exposure:** very low feed distribution is inconclusive about the story itself. Do not turn a tiny sample into a format verdict.
- **Retire provisionally:** all three get reasonable exposure and underperform the baseline; retain a strong existing showcase control.

Review at 48 hours for obvious issues and at seven days for the first decision. Revisit late-growing videos at 28 days. Three pilots per format provide directional evidence, not a statistically conclusive experiment. Expand the best two series into six further episodes each; choose their subjects from actual viewer requests and the best-converting premises.

## Local implementation references

- `ShortsCreator/Dashboard/README.md`: Studio workflow, constraints, queue and paratrooper setup.
- `ShortsCreator/ShortsCreator_Design.md`: capture pipeline and original reference formats.
- `ShortsCreator/Jobs/` and `ShortsCreator/Setups/`: working source presets.
- `ShortsCreator/Tools/post_shorts.py`: trim, captions, segment assembly and overlay integration.
- `ShortsCreator/Tools/stack_shorts.py`: existing four-way exporter.
- `Source/Tools/ShortsEditing/README.md`: independent editorial callout recipes.
- `Source/RTS_Survival/Shorts/RTSShortsJobTypes.h`: capture controls.
- `Source/RTS_Survival/Shorts/RTSShortsDirector.cpp`: battle placement, decision logging and shortening.

Paths in this reference section are project-relative. This document resides in `Source/Docs/Shorts/`.
