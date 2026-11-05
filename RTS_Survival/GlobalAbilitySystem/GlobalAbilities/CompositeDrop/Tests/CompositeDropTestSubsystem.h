// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "CompositeDropTestSubsystem.generated.h"

class AAircraftMaster;
class ACameraActor;
class ASquadController;
class ATankMaster;
class UGA_CompositeDrop;
class UGlobalAbility;

UENUM()
enum class ECompositeDropTestPhase : uint8
{
	WaitingForStart,
	Running,
	Finished
};

/**
 * @brief -CompositeDropValidate: clicks GA_GerLuftlandeSturm and then GA_GerMarderScavengerDrop through the player's
 * controller on the anchor (camera centre, or -CompositeDropAnchorX= / -CompositeDropAnchorY=) and checks the
 * separate aircraft, the strike, the paradropped and landed units, the single payment and cooldown, and that the
 * parent's end comes after every child's. Screenshots to Saved/CompositeDropTest; logs RTS_COMPOSITE_TEST.
 * Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UCompositeDropTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FAircraftTrack
	{
		TWeakObjectPtr<AAircraftMaster> Aircraft;
		FString ClassName;
		TArray<int32> LastMagazines;
		TArray<int32> Shots;
		TArray<float> Calibres;
		int32 LastBombCount = INDEX_NONE;
		int32 BombsThrown = 0;
		float NextStateLogSeconds = 0.f;
	};

	struct FSquadTrack
	{
		TWeakObjectPtr<ASquadController> Squad;
		FString ClassName;
		int32 Owner = 0;
		bool bM_WasInDrop = false;
		float ReleasedSeconds = -1.f;
		FVector ReleasedLocation = FVector::ZeroVector;
	};

	struct FScenario
	{
		EGlobalAbility AbilityType = EGlobalAbility::GA_None;
		FString Label;
		int32 ExpectedJu87 = 0;
		int32 ExpectedJu390 = 0;
		FString SquadMarker;
		int32 ExpectedSquads = 0;
		bool bExpectStrike = false;
		bool bExpectMarder = false;
		// The squads must be released after the strike's last shot.
		bool bSquadsAfterStrike = false;
		// -CompositeDropStrikeControl: the plain strafing template, executed as the manager would, for comparison.
		bool bIsStrikeControl = false;
		// Offset of this scenario's target from the anchor, so scenarios do not land on each other.
		FVector2D TargetOffset = FVector2D::ZeroVector;
		// Enemy tanks at the target: a Ju 87 that spawns outside its dive range only attacks after it turns
		// aggressive, so the strike needs something to attack. They are removed when the paradrop starts.
		bool bSpawnStrikeVictims = false;
		// Team weapon squads (guns on cargo chutes) and building expansions the scenario must drop.
		int32 ExpectedGunSquads = 0;
		int32 ExpectedBuildings = 0;
		// Wait after the parent ended: gun crews deploy and take their posts after they are released.
		float SettleSeconds = 2.f;
	};

	ECompositeDropTestPhase M_Phase = ECompositeDropTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_RunStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_ScenarioTarget = FVector::ZeroVector;
	// Where the paradrop child aims: the scenario target plus that child's offset.
	FVector M_SquadTarget = FVector::ZeroVector;

	TArray<FScenario> M_Scenarios;
	int32 M_ScenarioIndex = INDEX_NONE;

	// Per scenario.
	TSet<TWeakObjectPtr<AActor>> M_PreExistingActors;
	TArray<FAircraftTrack> M_Aircraft;
	TArray<FSquadTrack> M_Squads;
	TWeakObjectPtr<ATankMaster> M_Marder;
	float M_MarderDeliveredSeconds = -1.f;
	float M_FirstStrikeShotSeconds = -1.f;
	float M_LastStrikeShotSeconds = -1.f;
	TMap<TWeakObjectPtr<UGlobalAbility>, int32> M_ChildIndices;
	TMap<int32, float> M_ChildStartSeconds;
	TMap<int32, float> M_ChildEndSeconds;
	float M_ParentEndSeconds = -1.f;
	// Order of the ended events: the parent's must come after every child's, also within one frame.
	int32 M_EndedEventCount = 0;
	int32 M_ParentEndOrder = 0;
	int32 M_LastChildEndOrder = 0;
	// First paratrooper of the scenario back on the ground (squads are released only when all landed).
	float M_FirstTouchdownSeconds = -1.f;
	TArray<TWeakObjectPtr<AActor>> M_StrikeVictims;
	int32 M_ParentEndCount = 0;
	float M_LastShotSeconds = -1.f;
	bool bM_TookLandingShot = false;

	UPROPERTY()
	TObjectPtr<UGA_CompositeDrop> M_Ability;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_ControlAbility;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetRunSeconds() const;
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartScenario(int32 ScenarioIndex);
	void ResetScenarioState();
	void SnapshotExistingActors();
	bool CheckTemplate(const UGA_CompositeDrop& Ability, const FScenario& Scenario);
	bool EnsureCanAfford(const UGA_CompositeDrop& Ability, TMap<uint8, int32>& OutBefore);
	void CheckPaymentAndCooldown(const UGA_CompositeDrop& Ability, const TMap<uint8, int32>& Before);

	void OnChildStarted(UGA_CompositeDrop* Parent, int32 ChildIndex, UGlobalAbility* Child);
	void OnChildEnded(UGlobalAbility* Child);
	void OnParentEnded(UGlobalAbility* Parent);

	void TickRun();
	void TrackAircraft();
	void TrackStrikeAircraft(AAircraftMaster& Aircraft, FAircraftTrack& Track);
	void MarkStrikeShot();
	void TrackSquads();
	void TrackFirstTouchdown(ASquadController& Squad);
	/** @return The strike child's TotalStrafeTime: after it the Ju 87s cannot fire; -1 without a strike child. */
	float GetStrikeWindowSeconds() const;
	void TrackMarder();
	void FinishScenario();
	void CheckGunSquads(const FScenario& Scenario);
	void CheckDroppedBuildings(const FScenario& Scenario);
	/** @return Shortest ground distance from the location to a released infantry squad of this scenario. */
	float GetDistanceToNearestInfantry(const FVector& Location) const;
	TArray<AActor*> GetNewActorsOfClass(const UClass* ActorClass) const;
	void StartStrikeControl(UGlobalAbility* Copy);
	void SpawnStrikeVictims();
	void RemoveStrikeVictims();
	int32 CountAircraft(const TCHAR* ClassMarker) const;

	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
