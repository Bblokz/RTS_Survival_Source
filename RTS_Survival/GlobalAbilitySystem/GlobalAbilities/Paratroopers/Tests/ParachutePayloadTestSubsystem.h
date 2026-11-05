// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "ParachutePayloadTestSubsystem.generated.h"

class ACameraActor;
class UGlobalAbility;

UENUM()
enum class EParachutePayloadTestPhase : uint8
{
	WaitingForStart,
	StartingStep,
	Dropping,
	Settling,
	Checking,
	Finished
};

UENUM()
enum class EParachutePayloadKind : uint8
{
	Infantry,
	TeamWeapon,
	Building
};

/**
 * @brief -ParachutePayloadValidate: executes the parachute payload templates one after another at the anchor (where
 * the camera looks, or -ParachutePayloadAnchorX= / -ParachutePayloadAnchorY=): an ordinary infantry paradrop
 * (GA_GerParatroopers), the Flak 88 and sFH 18 gun drops and the Panzerschreck pit, Pak pit and field bunker drops.
 * Screenshots each drop mid-descent and after landing into Saved/ParachutePayloadsTest, then checks that guns and
 * buildings stand upright on the ground, belong to the player, that a crew mans every gun, and that the paratroopers
 * landed and take orders. -ParachutePayloadOnly=<template name part> runs a subset. Logs RTS_PARACHUTE_PAYLOAD_TEST.
 * On /Game/RTS_Survival/Maps/ToCopy/RTS_ToCopySmall the camera anchor is on the map edge: pass
 * -ParachutePayloadAnchorX=-11000 -ParachutePayloadAnchorY=-19000 (111/111 on 2026-11-05).
 */
UCLASS()
class RTS_SURVIVAL_API UParachutePayloadTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FPayloadTestStep
	{
		EGlobalAbility AbilityType = EGlobalAbility::GA_GerParatroopers;
		FString Label;
		EParachutePayloadKind Kind = EParachutePayloadKind::Infantry;
		int32 ExpectedCount = 1;
	};

	EParachutePayloadTestPhase M_Phase = EParachutePayloadTestPhase::WaitingForStart;
	TArray<FPayloadTestStep> M_Steps;
	int32 M_StepIndex = 0;
	float M_StartSeconds = -1.f;
	float M_PhaseStartSeconds = 0.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	bool bM_AbilityEnded = false;
	bool bM_TookDescentShot = false;
	bool bM_TookLandedShot = false;
	float M_LandedShotSeconds = 0.f;
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_CameraFocus = FVector::ZeroVector;
	float M_CameraDistance = 6000.f;
	float M_CameraPitch = -25.f;

	UPROPERTY()
	TObjectPtr<UGlobalAbility> M_Ability;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	// Actors that existed before the step; only new ones are checked and cleaned up.
	TSet<TWeakObjectPtr<AActor>> M_ActorsBeforeStep;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void Finish();
	void BuildSteps();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	void StartStep();
	void TickDropping();
	void TickSettling();
	void CheckStep();
	void CheckInfantryStep(const FPayloadTestStep& Step);
	void CheckTeamWeaponStep(const FPayloadTestStep& Step);
	void CheckBuildingStep(const FPayloadTestStep& Step);
	void CleanUpStep();
	TArray<AActor*> GetNewActorsOfClass(UClass* ActorClass) const;
	/** @return Height of the actor origin above the landscape under it; large when there is no landscape. */
	float GetHeightAboveGround(const AActor& Actor) const;
	bool TryFindMidDescentLoad(FVector& OutLoadLocation) const;
	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
	void OnAbilityEnded(UGlobalAbility* Ability);
};
