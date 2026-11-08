// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FireEdgeTestSubsystem.generated.h"

class ACameraActor;
class ASquadController;
class ASquadUnit;

/**
 * @brief -FireEdgeValidate: burns one ground fire per incendiary weapon (its real radius and flame scale) and holds
 * enemy soldiers at fixed distances from its centre: inside the flames (half radius, just inside the edge) and
 * just and clearly outside (80 cm and 1.5 m beyond the edge). Checks that the soldiers in the flames lose health and the ones
 * outside do not. RTS-camera screenshots to Saved/FireEdgeTest; logs RTS_FIRE_EDGE. Anchor: -FireStrikesAnchorX= /
 * -FireStrikesAnchorY=. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFireEdgeTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FProbe
	{
		// Distance from the fire's centre as a share of its radius plus a fixed offset (cm).
		float RadiusShare = 0.f;
		float OffsetCm = 0.f;
		float AngleDegrees = 0.f;
		bool bExpectBurn = false;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<ASquadUnit> Soldier;
		float StartHealth = -1.f;
		float EndHealth = -1.f;
	};

	struct FEdgeCase
	{
		FString Label;
		float RadiusCm = 0.f;
		float VfxScale = 1.f;
		FVector Centre = FVector::ZeroVector;
		TWeakObjectPtr<ASquadController> Squad;
		TArray<FProbe> Probes;
	};

	bool bM_Started = false;
	bool bM_Finished = false;
	float M_StartSeconds = -1.f;
	float M_RunStartSeconds = -1.f;
	FVector M_Anchor = FVector::ZeroVector;
	TArray<FEdgeCase> M_Cases;
	bool bM_FiresStarted = false;
	bool bM_SampledStart = false;
	bool bM_TookShots = false;
	bool bM_ShotAimed = false;
	int32 M_ShotCase = 0;
	float M_NextShotSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_ScreenshotCount = 0;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	float GetNow() const;
	float GetRunSeconds() const;
	void TickStart();
	void BuildCases();
	void SpawnSquads();
	// Hands the squad's soldiers to the case's probes; soldiers beyond the probes are held far outside.
	void AssignProbes(FEdgeCase& Case) const;
	void PinSoldiers();
	void StartFires();
	void SampleHealth(bool bStart);
	void TickShots();
	void CheckAndFinish();
	void AimRts(const FVector& Target) const;
	void DisableFogDarkening() const;
	void Check(bool bCondition, const FString& Description, const FString& Details);
	void RequestScreenshot(const FString& Label);
};
