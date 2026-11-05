// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "ResourceDropTestSubsystem.generated.h"

class ACameraActor;
class ACPPResourceMaster;
class AScavengeableObject;
class ASquadController;
class UGA_ResourceDrop;

UENUM()
enum class EResourceDropTestPhase : uint8
{
	WaitingForStart,
	WaitingForLanding,
	Collecting,
	Settling,
	Finished
};

/** @brief One of the three resource drops the test runs in turn. */
USTRUCT()
struct FResourceDropTestCase
{
	GENERATED_BODY()

	EGlobalAbility AbilityType = EGlobalAbility::GA_None;
	FString Name;
};

/** @brief What one test case measured; logged as RTS_RESOURCE_DROP_TEST lines. */
USTRUCT()
struct FResourceDropTestMeasurement
{
	GENERATED_BODY()

	float CastSeconds = 0.f;
	float LandedSeconds = -1.f;
	float PaybackSeconds = -1.f;
	float CollectedSeconds = -1.f;
	float FirstGainSeconds = -1.f;
	int32 CostTotal = 0;
	int32 BaselineRadixite = 0;
	int32 BaselineMetal = 0;
	int32 GainedRadixite = 0;
	int32 GainedMetal = 0;
	int32 GainEvents = 0;
	int32 DepositStartAmount = 0;
	int32 GainAtEmptied = 0;
	float ScavengeMultiplier = 1.f;
};

/**
 * @brief -ResourceDropValidate: runs the three resource drop templates (supply, construction supply, radixite cache)
 * on the anchor (camera ground point, or -ResourceDropAnchorX= / -ResourceDropAnchorY=; -ResourceDropOnly=<name> runs
 * one case). Containers are emptied by a spawned scavenger squad, the cache is mined by a spawned Pz IV harvester into
 * a spawned refinery. Checks landing, gains against cost, payback time and that nothing pays twice. Hostile units
 * within 300 m are removed first (-ResourceDropKeepHostiles keeps them); -ResourceDropTimeScale=N speeds it up.
 * Screenshots to Saved/ResourceDropTest; logs RTS_RESOURCE_DROP_TEST. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UResourceDropTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EResourceDropTestPhase M_Phase = EResourceDropTestPhase::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_RunDirection = FVector::ForwardVector;

	TArray<FResourceDropTestCase> M_Cases;
	int32 M_CaseIndex = INDEX_NONE;
	FResourceDropTestMeasurement M_Measurement;
	TArray<FVector> M_ExpectedDropPoints;
	bool bM_TookFallingShot = false;
	bool bM_TookMidCollectShot = false;
	int32 M_LastSeenRadixite = 0;
	int32 M_LastSeenMetal = 0;
	float M_NextOrderSeconds = 0.f;
	float M_NextDiagnosticSeconds = 0.f;
	float M_FirstFallingSeconds = -1.f;

	// Abilities stay alive for the whole run: their payloads and load callbacks outlive the case that cast them.
	UPROPERTY()
	TArray<TObjectPtr<UGA_ResourceDrop>> M_Abilities;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_LandedResources;

	// How often each container reported a finished scavenge; anything above one would be a double payout.
	TMap<TWeakObjectPtr<AScavengeableObject>, int32> M_ScavengeCompletions;

	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_Scavengers;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Harvester;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Refinery;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetPhaseSeconds() const;
	void SetPhase(EResourceDropTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void BuildCases();
	void RemoveHostilesNearAnchor() const;
	void GrantTestStorage() const;
	void StartNextCase();
	UGA_ResourceDrop* CreateAbility(EGlobalAbility AbilityType);
	void CheckTemplate(const UGA_ResourceDrop& Ability);
	void SpawnCollectors(const UGA_ResourceDrop& Ability);
	AActor* SpawnTestUnit(const TCHAR* ClassPath, const FVector& NearLocation, bool bStartAsBuilding) const;
	void CastAbility(UGA_ResourceDrop& Ability);
	void OnResourceLanded(UGA_ResourceDrop* Ability, AActor* SpawnedResource);

	void TickWaitingForLanding();
	void CheckLandedResources();
	void CheckLandedContainer(const AScavengeableObject& Container);
	void CheckLandedDeposit(ACPPResourceMaster& Deposit);
	void TickCollecting();
	void TrackResourceGain();
	void LogHarvestDiagnostics();
	void OrderScavengersToNextContainer();
	bool GetAreAllContainersEmptied() const;
	void TickSettling();
	void FinishContainerCase();
	void FinishDepositCase();
	void LogMeasurement() const;

	UGA_ResourceDrop* GetCurrentAbility() const;
	bool GetIsCurrentCaseDeposit() const;
	int32 GetPlayerResource(ERTSResourceType ResourceType) const;
	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
