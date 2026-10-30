// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSMenuBattleTypes.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "RTSMenuBattleDirector.generated.h"

class ACPPController;
class ARTSMenuBattleArena;
class ARTSMenuBattleCamera;
struct FStreamableHandle;

UENUM()
enum class ERTSMenuBattlePhase : uint8
{
	// Waiting for the first battle's units to load.
	Starting,
	Fighting,
	// The screen fades to black before the next battle replaces the current one.
	FadingOut
};

/**
 * @brief Place once in the start map, next to ARTSMenuBattleArena actors: stages endless random German versus Soviet
 * battles in those arenas behind the menu UI and films them with its own camera, cutting from battle to battle.
 * Germans fight as player 1, Soviets as player 2; the unit pools below list the Blueprints each side draws from.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSMenuBattleDirector : public AActor
{
	GENERATED_BODY()

public:
	ARTSMenuBattleDirector();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Units")
	FRTSMenuBattleFactionPool M_GermanPool;

	UPROPERTY(EditAnywhere, Category = "MenuBattle|Units")
	FRTSMenuBattleFactionPool M_SovietPool;

	// Arenas to stage battles in; empty means every arena in the level.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Arenas")
	TArray<TObjectPtr<ARTSMenuBattleArena>> M_Arenas;

	// Hosts the first battle: the arena the menu's start camera looks at, so the menu opens on that fight.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Arenas")
	TObjectPtr<ARTSMenuBattleArena> M_OpeningArena;

	// 0 picks a new seed every run; any other value replays the same sequence of battles.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing")
	int32 M_Seed = 0;

	// A battle still undecided this long after its last reinforcement wave is cut away from anyway.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "10"))
	float M_MaxBattleSeconds = 75.f;

	// The opening units drive up to their lines this long before the attack orders start; they shoot whatever comes
	// into range on the way.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "0"))
	float M_ApproachSeconds = 14.f;

	// Chance that a vehicle or squad is drawn from the side's variant pool instead of its base pool.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Units", meta = (ClampMin = "0", ClampMax = "1"))
	float M_VariantChance = 0.35f;

	// Aircraft climb this high above the ground they take off from and fly their runs from there (default 800).
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Units", meta = (ClampMin = "500"))
	float M_AircraftFlightHeight = 3200.f;

	// The camera lingers on the victors and the wrecks this long after one side has lost.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "0"))
	float M_DecidedHoldSeconds = 5.f;

	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "0.1"))
	float M_FadeSeconds = 0.8f;

	// An attack order is renewed this long after it was given even while its target lives: units retarget the
	// nearest enemy and retry paths that were aborted.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "2"))
	float M_ReorderSeconds = 8.f;

	// Seconds between rounds of attack orders; units whose target died get the nearest living enemy.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Pacing", meta = (ClampMin = "0.1"))
	float M_OrderIntervalSeconds = 0.75f;

	// The camera films each arena from its outer side toward the centre of the landscape under it, so shots never look
	// past the landscape's edge into the void; this point is used when no large landscape is found under an arena.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Presentation")
	FVector M_MapViewCentre = FVector::ZeroVector;

	// Health bars and other unit widgets stay hidden so the battles read as a backdrop, not as gameplay.
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Presentation")
	bool bM_HideUnitWidgets = true;

	// On: units leave the fog and stay visible to both sides, so neither the fog nor enemy vision decides whether
	// the camera and the opposing line can see them, and no shot looks into unexplored darkness.
	// Off: the battles play in the fog of war like a mission (the Soviet line shows once German units see it).
	UPROPERTY(EditAnywhere, Category = "MenuBattle|Presentation")
	bool bM_KeepBattleUnitsOutOfFog = true;

	UPROPERTY()
	TObjectPtr<ARTSMenuBattleCamera> M_Camera;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	FRTSMenuBattleState M_Battle;

	// Arena and units of the battle after the current one; loaded while the current one is fought.
	UPROPERTY()
	FRTSMenuBattleRoster M_NextRoster;

	// Keeps the next battle's classes loaded; replaced when its battle starts.
	TSharedPtr<FStreamableHandle> M_NextRosterLoadHandle;
	TSharedPtr<FStreamableHandle> M_CurrentBattleLoadHandle;

	ERTSMenuBattlePhase M_Phase = ERTSMenuBattlePhase::Starting;
	bool bM_IsNextRosterLoaded = false;
	float M_FadeOutStartSeconds = -1.f;
	float M_LastOrderSeconds = -1.f;
	int32 M_BattleCount = 0;
	FRandomStream M_RandomStream;

	bool GetIsValidCamera() const;
	bool GetIsValidPlayerController() const;
	float GetTimeSeconds() const;

	void BeginPlay_CollectArenas();
	void BeginPlay_InitRandomStream();
	void BeginPlay_SpawnCamera();

	/** Finds the menu controller once it exists, silences its unit voice lines and gives the view to the camera. */
	void EnsurePlayerControllerSetup();
	void KeepCameraAsViewTarget();
	void FadeCamera(const float FromAlpha, const float ToAlpha) const;

	// ---- Next battle ----
	void PrepareNextRoster();
	ARTSMenuBattleArena* PickArena() const;

	ERTSMenuBattleFlavour PickFlavour() const;

	/**
	 * @brief How many units one side brings in a wave: few in the opening skirmish, more per reinforcement wave,
	 * weighted by the battle's flavour.
	 * @param Settings The arena's escalation settings.
	 * @param Flavour What the battle is mostly fought with.
	 * @param Wave 0 for the opening skirmish.
	 * @return The counts per unit kind.
	 */
	FRTSMenuBattleWaveCounts GetWaveCounts(const FRTSMenuBattleArenaSettings& Settings,
	                                       const ERTSMenuBattleFlavour Flavour, const int32 Wave) const;

	/**
	 * @brief Adds one side's units of one wave to the roster, with slots and jitter.
	 * @param Side Side the units fight for.
	 * @param Kind Rank of the units.
	 * @param Wave Wave the units arrive with.
	 * @param Count Number of units to draw.
	 * @param OutRoster Roster that receives the units.
	 */
	void AddRosterEntries(const ERTSMenuBattleSide Side, const ERTSMenuBattleUnitKind Kind, const int32 Wave,
	                      const int32 Count, FRTSMenuBattleRoster& OutRoster) const;

	/** A random class of the kind from the side's pool, from its variants at M_VariantChance; null when empty. */
	TSoftClassPtr<AActor> PickUnitClass(const ERTSMenuBattleSide Side, const ERTSMenuBattleUnitKind Kind) const;
	void OnNextRosterLoaded();

	// ---- Battle lifetime ----
	void StartNextBattle();
	void SpawnWave(const int32 Wave);
	FVector GetViewCentreForArena(const ARTSMenuBattleArena& Arena) const;
	void SetFogOfWarDarkeningEnabled(const bool bEnabled) const;
	void TickReinforcements();
	void SpawnRosterUnit(const FRTSMenuBattleRosterEntry& Entry, const ARTSMenuBattleArena& Arena);
	void OrderDriveToFormUp(AActor& Unit, const FRTSMenuBattleRosterEntry& Entry, const ARTSMenuBattleArena& Arena) const;
	FVector GetGroundedSpawnLocation(const FVector& Location, const ERTSMenuBattleUnitKind Kind) const;
	/**
	 * @brief An arena near its landscape's edge can put deep reinforcement slots past that edge; such a slot moves
	 * towards the middle of the battle until there is ground under it, so no unit spawns over the void.
	 * @param Arena The battle's arena.
	 * @param SlotLocation The roster entry's slot.
	 * @return The slot when it has ground under it, else the first point towards the battle that has.
	 */
	FVector GetSlotOverGround(const ARTSMenuBattleArena& Arena, const FVector& SlotLocation) const;
	void EndCurrentBattle();
	void OnBattleActorSpawned(AActor* SpawnedActor);

	// ---- Running battle ----
	void TickFighting();
	void TickFadingOut();
	void AbsorbSquadSoldiers(FRTSMenuBattleSideUnits& Side) const;
	void PrepareBattleUnit(AActor* Unit) const;
	void KeepBattleUnitsPresentable(const FRTSMenuBattleSideUnits& Side);
	/**
	 * @brief Sends every attacker without a living target at the nearest living defender: an attack order when its
	 * side can see that defender, else a move toward it until the fog of war reveals it.
	 * @param Attackers Side that receives the orders.
	 * @param Defenders Side whose ground units are the targets.
	 * @param AttackerPlayer Owning player of the attackers, which decides what they can see.
	 */
	void IssueAttackOrders(const FRTSMenuBattleSideUnits& Attackers, const FRTSMenuBattleSideUnits& Defenders,
	                       const uint8 AttackerPlayer);

	/** Attack order on a visible target, or a drive toward it when the attacker has not moved since its last order. */
	void OrderAttack(const TWeakObjectPtr<AActor>& Attacker, AActor& Target);

	// Moves the attacker toward a target the fog hides; renewed every few seconds while the target stays hidden.
	void OrderAdvanceOnHiddenTarget(const TWeakObjectPtr<AActor>& Attacker, AActor& Target);
	void UpdateCameraSubjects() const;
	void UpdateBattleDecision();
	bool GetIsReadyToLeaveBattle() const;

	void LogUndecidedSurvivors() const;

	static int32 CountAliveGroundUnits(const FRTSMenuBattleSideUnits& Side);
	// Living vehicles and soldiers, without the side's weapon pits.
	static int32 CountAliveMobileUnits(const FRTSMenuBattleSideUnits& Side);
	static uint8 GetOwningPlayer(const ERTSMenuBattleSide Side);

#if RTS_WITH_MAP_TESTS
	// -MenuBattleShots=<seconds> saves a screenshot at that interval; -MenuBattleQuitAfter=<seconds> exits the game;
	// -MenuBattleGameplayFog keeps the battle units in the fog of war (bM_KeepBattleUnitsOutOfFog off).
	float M_ScreenshotIntervalSeconds = 0.f;
	float M_LastScreenshotSeconds = 0.f;
	int32 M_ScreenshotIndex = 0;
	float M_QuitAfterSeconds = 0.f;
	void BeginPlay_ReadTestSwitches();
	void TickTestSwitches();
#endif
};
