// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireRules.h"
#include "WorldBatteryFireDirector.generated.h"

class AWorldBuildNode;
class AWorldDivisionBase;
class AWorldPlayerController;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UStaticMeshComponent;
class UWorldBatteryFireSettings;
class UWorldBuildNodeManager;
class UWorldCampaignProgressManager;
class UWorldDivisionManager;
class UWorldDivisionTankFormation;

/**
 * @brief Everything the show needs to know about a fire mission whose damage is already applied and saved.
 */
USTRUCT()
struct FWorldBatteryFireMissionResult
{
	GENERATED_BODY()

	FName BuildNodeId = NAME_None;
	FGuid DivisionKey;
	FVector BatteryLocation = FVector::ZeroVector;
	// Where the shells leave the battery.
	FVector MuzzleLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	// Formation slots that died, in the order they blow up; empty for pieces without a tank formation.
	TArray<int32> DestroyedSlots;
	// World location of each destroyed slot, taken before the damage was applied.
	TArray<FVector> DestroyedSlotLocations;
	int32 EntriesDestroyed = 0;
	bool bIsDivisionEmptied = false;
};

/**
 * @brief One shell of the salvo on its way from the muzzle to its impact point.
 */
USTRUCT()
struct FWorldBatteryFireShell
{
	GENERATED_BODY()

	// Moves along the arc; carries the optional shell mesh and the trail.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Carrier = nullptr;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Trail = nullptr;

	FVector ImpactLocation = FVector::ZeroVector;
	float LaunchSeconds = 0.f;
	// Formation slot this shell blows up, or INDEX_NONE for a shell that only hits the ground.
	int32 CasualtySlot = INDEX_NONE;
	bool bHasLaunched = false;
	bool bHasLanded = false;
};

/**
 * @brief What the running show keeps between ticks.
 */
USTRUCT()
struct FWorldBatteryFireShowState
{
	GENERATED_BODY()

	UPROPERTY()
	FWorldBatteryFireMissionResult Result;

	UPROPERTY()
	TArray<FWorldBatteryFireShell> Shells;

	// The target while it still stands; an emptied division is a released husk this director destroys at the end.
	UPROPERTY()
	TWeakObjectPtr<AWorldDivisionBase> Division;

	// Owns the shells and their trails. The controller cannot: it is a hidden actor, and nothing it owns is drawn.
	UPROPERTY()
	TObjectPtr<AActor> ShellOwner = nullptr;

	float ElapsedSeconds = 0.f;
	float DurationSeconds = 0.f;
	FVector CameraLocationBeforeShow = FVector::ZeroVector;
	float ZoomDistanceBeforeShow = 0.f;
	bool bIsPlaying = false;
	bool bHasCameraToRestore = false;
	bool bHasPlayedDescentSound = false;
};

/** Target picking started or ended. */
DECLARE_MULTICAST_DELEGATE(FOnWorldBatteryFirePickingChanged);

/** A click while picking was refused; the text says why. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldBatteryFirePickRefused, const FText& /*Reason*/);

/** A fire mission was carried out (damage applied and saved); the show may still be playing. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldBatteryFireMissionFired, const FWorldBatteryFireMissionResult& /*Result*/);

/** The show of the last fire mission ended, or the mission finished without a show. */
DECLARE_MULTICAST_DELEGATE(FOnWorldBatteryFireShowFinished);

/**
 * @brief Owned by the world player controller. Carries out battery fire missions: checks the rules, applies the
 * damage and saves, and only then plays the salvo as a cosmetic show that blocks input. Also holds the target pick
 * that the build site panel starts and the controller's map clicks finish.
 */
UCLASS(ClassGroup=(WorldCampaign))
class RTS_SURVIVAL_API UWorldBatteryFireDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldBatteryFireDirector();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// --- Rules against the live map ---

	/** @return Success when the battery on the node can fire on something now, else why its button is off. */
	EWorldBatteryFireResult GetBatteryReadiness(FName BuildNodeId) const;

	EWorldBatteryFireResult GetFireMissionResult(FName BuildNodeId, const FGuid& DivisionKey) const;

	TArray<FGuid> GetValidTargetKeys(FName BuildNodeId) const;

	// --- Fire mission ---

	/**
	 * @brief Fires the battery on a division: applies the damage, marks the battery as fired, saves, then plays the
	 * show. Nothing of the mission depends on the show reaching its end.
	 * @param BuildNodeId Node of the built battery.
	 * @param DivisionKey Enemy division or convoy to hit.
	 * @param OutFailReason Player-facing reason when the mission is refused; empty on success.
	 * @return true when the mission was carried out.
	 */
	bool RequestFireMission(FName BuildNodeId, const FGuid& DivisionKey, FText& OutFailReason);

	bool GetIsShowPlaying() const { return M_Show.bIsPlaying; }

	/** Seconds since the running show started; 0 when none plays. */
	float GetShowElapsedSeconds() const { return M_Show.bIsPlaying ? M_Show.ElapsedSeconds : 0.f; }

	/** Forgets which batteries fired and clears the wrecks of last turn's missions. */
	void HandleNewPlayerTurn(int32 CurrentTurn);

#if !UE_BUILD_SHIPPING
	/** Headless checks: plays the show's timeline without a renderer, so its code runs although nothing is drawn. */
	void SetPlaysShowWithoutRenderer(bool bPlays) { bM_PlaysShowWithoutRenderer = bPlays; }
#endif

	// --- Target picking ---

	/**
	 * @brief Starts waiting for the player to click a target and shows the battery's range ring.
	 * @param BuildNodeId Node of the built battery.
	 * @param OutFailReason Player-facing reason when the battery cannot fire now.
	 * @return true when picking started.
	 */
	bool BeginTargetPicking(FName BuildNodeId, FText& OutFailReason);

	void CancelTargetPicking();

	bool GetIsPickingTarget() const { return bM_IsPickingTarget; }

	/**
	 * @brief The controller hands every map click here while picking. A wrong click keeps the pick open and
	 * broadcasts the reason.
	 * @param ClickedActor Actor under the cursor, or nullptr.
	 * @param OutFailReason Player-facing reason when nothing was fired.
	 * @return true when the battery fired and picking ended.
	 */
	bool TryFireOnPickedActor(AActor* ClickedActor, FText& OutFailReason);

	FOnWorldBatteryFirePickingChanged OnPickingChanged;
	FOnWorldBatteryFirePickRefused OnPickRefused;
	FOnWorldBatteryFireMissionFired OnMissionFired;
	FOnWorldBatteryFireShowFinished OnShowFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// --- Rule inputs ---
	FWorldBatteryFireBattery MakeBatteryInput(FName BuildNodeId) const;
	FWorldBatteryFireTarget MakeTargetInput(const AWorldDivisionBase* Division) const;
	TArray<FWorldBatteryFireTarget> MakeAllTargetInputs() const;
	int32 GetCurrentTurn() const;

	// --- Mission ---

	/**
	 * @brief Snapshots what the show needs before the damage changes the division.
	 * @param BuildNode The firing battery.
	 * @param Division The target, still undamaged.
	 * @return Locations and the slots that will die.
	 */
	FWorldBatteryFireMissionResult BuildMissionResult(const AWorldBuildNode& BuildNode,
	                                                  const AWorldDivisionBase& Division) const;
	FVector GetMuzzleLocation(const AWorldBuildNode& BuildNode) const;

	/**
	 * @brief Removes the roster entries. An emptied division leaves the manager at once but its actor is kept for
	 * the show; a division that survives keeps its dead tanks standing until the show blows them up.
	 * @param Result The mission, with its casualties.
	 * @param Division The target.
	 * @param bWillPlayShow false = every visual change happens at once.
	 */
	void ApplyMissionDamage(const FWorldBatteryFireMissionResult& Result, AWorldDivisionBase& Division,
	                        bool bWillPlayShow);
	void SaveCampaign() const;

	// --- Show ---
	bool GetCanPlayShow() const;
	void StartShow(const FWorldBatteryFireMissionResult& Result, AWorldDivisionBase* Division);
	void StartShow_BuildShells(const UWorldBatteryFireSettings& Settings);

	/** Loads every asset of the show before the first shell, so no load stalls the salvo. */
	void StartShow_PreloadAssets(const UWorldBatteryFireSettings& Settings) const;
	void StartShow_FrameCamera(const UWorldBatteryFireSettings& Settings);
	void UpdateShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings);
	void LaunchShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings);
	void LandShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings);
	void RemoveShellVisuals(FWorldBatteryFireShell& Shell) const;

	/** Ends the show from any point: remaining casualties become wrecks, the husk goes, input comes back. */
	void FinishShow(bool bRestoreCamera);

	/** What ends a mission with or without a show: flushes the formation and destroys an emptied division. */
	void FinishMissionVisuals(AWorldDivisionBase* Division, bool bIsDivisionEmptied) const;

	void SpawnEffect(UNiagaraSystem* Effect, const FVector& Location, const FRotator& Rotation, float Scale) const;
	void PlaySound(USoundBase* Sound) const;

	// --- Picking ---
	void ShowRangeRing(const FVector& BatteryLocation);
	void HideRangeRing();

	UWorldDivisionTankFormation* GetTankFormation(AWorldDivisionBase* Division) const;

	bool GetIsValidWorldPlayerController() const;
	UWorldBuildNodeManager* GetBuildNodeManager() const;
	UWorldDivisionManager* GetDivisionManager() const;
	UWorldCampaignProgressManager* GetProgressManager() const;

	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	FWorldBatteryFireShowState M_Show;

	bool bM_IsPickingTarget = false;

	FName M_PickingBuildNodeId = NAME_None;

	// Id of the range ring in the radius pool while picking; 0 or less = none.
	int32 M_RangeRingId = INDEX_NONE;

	// Set by headless checks only; see SetPlaysShowWithoutRenderer.
	bool bM_PlaysShowWithoutRenderer = false;
};
