# EWeaponName uint32: merge runbook

How to bring the finished migration (worktree `D:/UE5Projects/RTS_WN`, branch `migration/weaponname-uint32` in both the superproject and the `Source` submodule) into `main` once the other agents have finished their new vehicles. Background and tooling: `EWeaponName_Uint32_Migration.md`.

**Approach.** Do all the risky work in the worktree, where the editors on `main` (and the UECP bridge other agents use) are not affected:

1. Record the weapon values of everything `main` added or changed, while `main` is still `uint8`.
2. Merge `main` into the migration branch.
3. Re-apply the migration to just those assets.
4. Test.

`main` itself then only needs a fast-forward and a rebuild, so the editors are closed for about 30 minutes.

## Progress log

**Round 1 (2026-10-26): merged `main` superproject `325e93160` / Source `7cb7a9d5`.**

* Snapshot of `main`: 1,501 rows. 52 of them are `RTS_BP_GameState` CDO rows for the 13 new weapons, which the C++ fills into `SkipSerialization` maps; they are harmless.
* Delta: 7 packages, all new batch-3 weapon Blueprints, none of them already migrated: Sd.Kfz. 140 AA / Bunker turret / Bunker vehicle, Pz II F Radixite turret, and the Puma DualBarrel / OverchargedBeam / RadixiteSniper turrets.
* Merge: Source merged cleanly. The only superproject conflict was the `Source` gitlink.
* Apply: 0 `FAILED`, 6 variables retyped, 9 wires relinked, 0 compile errors.
* Verify (1,449 combined rows): 0 changed, only the 2 known `Break Weapon Description` rows missing.
* CompileAllBlueprints: identical to the baseline.
* Tests: 138 of 139 `RTS.` tests pass. The one failure is not caused by the migration: `RTS.Weapons.GroundFire.OtherWeaponsHaveNone` fails on `main` too, because batch 3 gives `KwK37_75MM_Napalm` ground fire while the test still allows only `Rocket_320mm_Incendiary`.
* Headless `-game` smoke runs on 3 maps: clean.
* Not landed. The variants agent still has uncommitted weapon Blueprints on `main`: the Luchs and 234/3 turret edits, the new 234/3 IncendiarySupport and StummelAssault turrets, and the Puma, 234/3, Pz II F and Sd.Kfz. 140 vehicles. Once they are committed, run round 2 (steps 2–4 for `325e93160..main` only), then step 5.
**Round 2 (2026-10-26): merged `main` superproject `a88486722` / Source `92f92162`** (variants `09eb60cad`, Luchs + Shorts `a88486722`, Shorts text `92f92162`).

* Delta: 3 packages. The new `EmbTurret_Sdkfz_234_3_IncendiarySupport` and `_StummelAssault`, plus `Ger_Turret_PZ_II_Luchs`. The Luchs turret was changed on both sides: the merge took `main`'s version (identical blob) and Apply migrated it again.
* Apply: 0 `FAILED`, 0 compile errors.
* Verify (1,459 combined rows): 0 changed, the 2 known missing rows.
* CompileAllBlueprints: identical to the baseline.
* Tests: 138 of 139; the GroundFire test fails on `main` too.
* Headless `-game` smoke runs on 3 maps: clean.

* Note for the variants agent: some new variant Blueprints currently store the *base* weapon, not the batch-3 one. The Pz II F Radixite turret stores `KwK30_20MM`, the Puma DualBarrel `Kwk34_Twin30MM`, the 140 AA `MG151_15MM_Twin` and the 140 Bunker mortar `Mortar_40MM`. The migration keeps whatever is stored.

## 0. State on 2026-10-26 (re-check before starting)

| | Superproject | Source |
|---|---|---|
| Branch point (last commit both have) | `8948a4c66` | `16891bc9` |
| Migration branch head when this was written | `0fc0bd2dd` (+ the commit adding this runbook) | `69b5be17` (+ this runbook) |
| Commits on `main` since the branch point | 0 | 0 |

* The in-progress work on `main` is all uncommitted. Variant batch 3 appends 13 weapons (`MG151_15MM_Drilling` … `Rocket_200mm_PrecisionMissile`) **at the end** of `EWeaponName`. The branch's 200 entries are an exact prefix of `main`'s 213, so the enum merges as text and no stored value moves. `main` has 43 `uint8` slots left.
* None of that work exposes `EWeaponName` to Blueprints. `VariantBatchThreeWeaponData.h` uses it only as a `TMap` key, which is fine at any width.
* No uncommitted or untracked Content on `main` contains weapon data yet; the batch-3 Blueprints are not made. They will be. That is what step 2 is for.

## 1. Before the merge: rules for agents working on `main`

Pass these on while the branch is open:

1. **Append only.** New `EWeaponName` entries go at the end, after the last one. Never insert, reorder or remove (hide with `UMETA(Hidden)`). An inserted entry shifts every value after it and breaks this plan.
2. **Stay under 256 entries.** `main` is still `uint8`. If a batch would pass 255, merge this branch first.
3. **New Blueprint-facing weapon fields.** On `main` they still have to be `EWeaponName`. Tell the merger, because after the merge they become `FName` with `meta=(GetOptions="RTSBlueprintFunctionLibrary.GetWeaponNameOptions")` plus a `Get…Enum()` accessor. UHT reports any that are missed.
4. **New weapon Blueprints are fine.** Make them as usual: enum pin on `Make InitWeaponState…`, `EWeaponName` variables, duplicates of existing turrets. Step 2 records them.
5. **Commit everything** (Source first, then the superproject with the new `Source` pointer) before the merge window. Uncommitted Content cannot be merged.
6. Editing one of the 269 already-migrated weapon Blueprints on `main` is allowed but costs a re-apply. The merge takes `main`'s version and migrates it again.

## 2. Snapshot `main`'s new weapon data (editors on `main` may stay open)

Done in `RTS_WN`, temporarily checked out at `main`'s commit. The branch must be fully committed first (`git status` is clean in both repos).

```bash
export MSYS_NO_PATHCONV=1
cd /d/UE5Projects/RTS_WN
MAIN_SUPER=$(git rev-parse main); MAIN_SOURCE=$(git -C Source rev-parse main)
git switch --detach $MAIN_SUPER
git -C Source switch --detach $MAIN_SOURCE
# The commandlet only exists on the branch: copy it in as untracked files.
mkdir -p Source/RTS_Survival/Utils/Migration
for f in WeaponNameMigrationCommandlet.h WeaponNameMigrationCommandlet.cpp; do
  git -C Source show migration/weaponname-uint32:RTS_Survival/Utils/Migration/$f > Source/RTS_Survival/Utils/Migration/$f
done
```

Build the editor target (enum still `uint8`, so this is a large rebuild):

```bash
cd /d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine
cmd //c "Engine\\Build\\BatchFiles\\Build.bat RTS_SurvivalEditor Win64 Development -Project=D:\\UE5Projects\\RTS_WN\\RTS_Survival.uproject -WaitMutex -NoHotReloadFromIDE"
```

Run the snapshot. It is read-only and takes about 1 minute. `-DisablePlugins=BpGeneratorUltimate` keeps this process off the UECP bridge:

```bash
mkdir -p /d/UE5Projects/RTS_WN/Saved/WeaponMerge
"/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "D:/UE5Projects/RTS_WN/RTS_Survival.uproject" \
  -run=WeaponNameMigration -Mode=Snapshot -Tsv="D:/UE5Projects/RTS_WN/Saved/WeaponMerge/EWeaponName_Snapshot_Main.tsv" \
  -DisablePlugins=BpGeneratorUltimate -unattended -nullrhi -nosplash -stdout > /d/UE5Projects/RTS_WN/Saved/WeaponMerge/snapshot_main.log 2>&1
```

List what `main` changed, build the delta, then go back to the branch:

```bash
cd /d/UE5Projects/RTS_WN
W=Saved/WeaponMerge
git diff --name-only --diff-filter=AMR 8948a4c66 $MAIN_SUPER -- Content > $W/main_changed_content.txt
# Native Windows python cannot read <(...) process substitutions, so extract the branch files first.
for f in make_delta_snapshot.py EWeaponName_Snapshot.tsv; do
  git -C Source show migration/weaponname-uint32:Refactor/MigrationGuides/$f > $W/$f
done
python -I $W/make_delta_snapshot.py $W/EWeaponName_Snapshot_Main.tsv $W/main_changed_content.txt $W/EWeaponName_Snapshot.tsv $W
rm -r Source/RTS_Survival/Utils/Migration   # untracked copies; they would block the switch back
git -C Source switch migration/weaponname-uint32
git switch migration/weaponname-uint32
mkdir -p Source/Refactor/MigrationGuides/Merge
cp $W/EWeaponName_Snapshot_Main.tsv $W/EWeaponName_Snapshot_Delta.tsv $W/EWeaponName_Snapshot_Combined.tsv $W/main_changed_content.txt \
  Source/Refactor/MigrationGuides/Merge/
```

The branch's binaries are now the `uint8` build. Step 3 rebuilds them after the merge.

Check the delta before going on:

* Run the §4.4 checks of the migration guide on `EWeaponName_Snapshot_Delta.tsv`. Use `check_weaponname_snapshot.py` with an inventory of the changed packages that contain `EWeaponName`. Every new vehicle should show a value. An empty `CanonicalValue` must be fixed on `main` now, while its editor still shows the enum dropdown.
* `make_delta_snapshot.py` prints how many delta packages were already migrated on the branch. Those are the ones that will conflict in step 3.

Commit `Refactor/MigrationGuides/Merge/` to the Source branch, so the record of which vehicle uses which weapon is in git before anything is overwritten.

## 3. Merge `main` into the branch (in `RTS_WN`)

Source first, then the superproject:

```bash
cd /d/UE5Projects/RTS_WN
git -C Source merge $MAIN_SOURCE
git merge $MAIN_SUPER
```

Expected conflicts and how to resolve them:

| Conflict | Resolution |
|---|---|
| Superproject `Source` (gitlink) | `git add Source`. `RTS_WN/Source` is already at the merged Source commit. |
| `Content/**.uasset` changed on both sides | Take **`main`'s** version: `git checkout --theirs -- <file>`, then `git add <file>`. Step 4 migrates it again from the delta. Check that every such file is in `EWeaponName_Snapshot_Delta.tsv`, or that it holds no weapon data. |
| Asset deleted on `main`, migrated on the branch | `git rm <file>`. |
| `WeaponSystems.h` | Should merge cleanly (append-only). If not, keep the branch's header block (`UENUM()`, `uint32`, comment) and `main`'s full entry list, plus both sides' `Global_GetWeaponDisplayName` / `Global_GetWeaponEnumAsString` cases. |
| Other C++ | Normal resolution. Then see below. |

C++ that `main` added and that must change for `uint32`. UHT and the compiler find every case:

* `enum class EWeaponName : uint8;` in new files → `: uint32;`. A mismatch is a hard compile error.
* New `EWeaponName` UPROPERTY with `Blueprint*` specifiers, or UFUNCTION parameters → convert as in §5.2 of the migration guide: Details-only fields drop the specifier, Blueprint-facing ones become `FName` with `GetOptions`.
* New init structs with an `EWeaponName WeaponName` field → the `FName` + `GetWeaponNameEnum()` pattern.
* New code passing `Params.WeaponName` to an `Init*Weapon` call → `Params.GetWeaponNameEnum()`.

Build `RTS_SurvivalEditor Win64 Development` for `RTS_WN` until UHT and the compiler are clean. Commit the merge.

## 4. Re-apply and verify (in `RTS_WN`)

```bash
CMD="/d/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
P="D:/UE5Projects/RTS_WN/RTS_Survival.uproject"; M=D:/UE5Projects/RTS_WN/Source/Refactor/MigrationGuides/Merge
FLAGS="-DisablePlugins=BpGeneratorUltimate -unattended -nullrhi -nosplash -stdout"
"$CMD" "$P" -run=WeaponNameMigration -Mode=Apply -DryRun -Tsv="$M/EWeaponName_Snapshot_Delta.tsv" $FLAGS > apply_dry.log 2>&1
"$CMD" "$P" -run=WeaponNameMigration -Mode=Apply         -Tsv="$M/EWeaponName_Snapshot_Delta.tsv" $FLAGS > apply.log 2>&1
"$CMD" "$P" -run=WeaponNameMigration -Mode=Verify        -Tsv="$M/EWeaponName_Snapshot_Combined.tsv" $FLAGS > verify.log 2>&1
python -I Source/Refactor/MigrationGuides/apply_summary.py apply.log
```

Pass criteria:

* **Dry run and Apply:** 0 `FAILED` and 0 Blueprint compile errors. The summary groups any errors per asset.
* **Verify (combined TSV):** 0 `VERIFY CHANGED`. The only `VERIFY MISSING` lines are the two known `Break Weapon Description` rows in `BP_W_WeaponDescriptionHorizontal`.
* **`git status Content`:** only delta packages changed.

Apply handles the cases the merge creates:

* New variants that are children of migrated turrets: their CDO override is restored onto the parent's retyped `Name` variable.
* Duplicated Blueprints: nodes are looked up per graph.
* Wires K2 rewired through "Enum to Name" nodes: the debris is removed.

Then run the wider checks:

1. `-run=CompileAllBlueprints`. Group the output with `group_bp_errors.py` and diff it against `EWeaponName_CompileAll_Baseline.txt`. Only new lines matter.
2. `-ExecCmds="Automation RunTests RTS.; Quit" -testexit="Automation Test Queue Empty"`. Count only `Path={RTS.` results: the filter also catches engine tests, and `System.Engine.Loading.InvalidExports.ImportPackageConvertedOnLoad` crashes after the RTS tests finish.
3. Headless `-game` smoke runs, with `MSYS_NO_PATHCONV=1` or the `/Game/...` map path gets mangled:
   * maps: `Ger_DestroyT35`, `RTS_GameStartTutorial`, `Ger_FirstMission`, plus any map that places the new vehicles;
   * flags: `-game -nullrhi -nosound -unattended -benchmark -benchmarkseconds=90 -fps=30`;
   * pass: 0 `Unknown weapon name` and 0 `Message dialog closed`.
4. PIE in the worktree editor (`UnrealEditor.exe <RTS_WN uproject> <map> -DisablePlugins=BpGeneratorUltimate`) with the new vehicles: weapon UI, shell types, firing.

Commit the re-applied Content (`Re-apply weapon name migration to assets added on main`).

## 5. Land on `main` (editors closed, ~30 minutes)

1. **Freeze.** Everyone has committed; no agent is mid-task on `main`.
2. **Check `main` has not moved.** `git rev-parse main` and `git -C Source rev-parse main` must still equal `$MAIN_SUPER` / `$MAIN_SOURCE`. If they moved, repeat steps 2–4 for the new commits only: snapshot the new head, diff from the previous `$MAIN_SUPER`, and merge again. It is cheap.
3. **Close every editor on `main`.** Check `Get-Process UnrealEditor*`; the user's editor is `UnrealEditor-Win64-DebugGame`. Ask before stopping anything you did not start. A running editor locks `UnrealEditor-RTS_Survival.dll` and holds stale copies of the migrated assets.
4. **Tag the rollback point** in both repos:

   ```bash
   cd /d/UE5Projects/RTS_Survival
   git tag pre-weaponname-uint32 main
   git -C Source tag pre-weaponname-uint32 main
   ```

5. **Fast-forward, Source first.** `--ff-only` refuses if uncommitted changes on `main` overlap the incoming files, which is the point of step 1.

   ```bash
   git -C Source merge --ff-only migration/weaponname-uint32
   git merge --ff-only migration/weaponname-uint32
   git submodule status   # Source must show no '+'
   ```

6. **Build both editor configurations used on `main`:** `RTS_SurvivalEditor Win64 Development` and `RTS_SurvivalEditor Win64 DebugGame` (with `-Project=D:\UE5Projects\RTS_Survival\RTS_Survival.uproject`).
7. **Reopen the editor** and run one quick PIE.
8. **Pushing** is the user's call. Push Source before the superproject, so the gitlink never points at a commit the remote lacks.

## 6. Afterwards

* **Rollback** (before pushing): `git reset --hard pre-weaponname-uint32` in both repos, then rebuild. Never reset one repo without the other.
* **Remove the worktree.** Delete the 81 Content junctions **as links**, not with a recursive delete, which would follow them into `main`'s marketplace folders: in PowerShell, `Get-ChildItem -Recurse -Attributes ReparsePoint D:\UE5Projects\RTS_WN\Content | ForEach-Object { $_.Delete() }`. Then:

  ```bash
  git -C /d/UE5Projects/RTS_Survival/Source worktree remove /d/UE5Projects/RTS_WN/Source
  git -C /d/UE5Projects/RTS_Survival worktree remove /d/UE5Projects/RTS_WN
  ```

  Delete the `migration/weaponname-uint32` branches once merged; keep the tags.
* **Retire the commandlet.** It is editor-only and compiled into the game module. Delete `Utils/Migration/` in a follow-up commit once the merge has settled. Keep the snapshot TSVs: they are the record of every Blueprint's weapon before the migration.
* **From now on,** follow §9 of the migration guide: Blueprint-facing weapon fields are `FName` with `GetOptions`, and enum entries are append-only.
