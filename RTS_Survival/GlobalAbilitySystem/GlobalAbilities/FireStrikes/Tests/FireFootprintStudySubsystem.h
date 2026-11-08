// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FireFootprintStudySubsystem.generated.h"

class ACameraActor;

/**
 * @brief -FireFootprintStudy: burns one ground fire per incendiary weapon (its real radius and flame scale) through the
 * fire subsystem on the anchor and photographs it straight from above (known scale: the footprint is measured from
 * the pictures) and from the RTS camera. Each fire is put out before the next. Screenshots to Saved/FireFootprint;
 * logs RTS_FIRE_FOOTPRINT with the camera geometry. Anchor: -FireStrikesAnchorX= / -FireStrikesAnchorY=.
 * Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFireFootprintStudySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FFootprintCase
	{
		FString Label;
		float RadiusCm = 0.f;
		float VfxScale = 1.f;
	};

	struct FShot
	{
		float AtSeconds = 0.f;
		bool bTopDown = true;
		FString Suffix;
	};

	bool bM_Started = false;
	bool bM_Finished = false;
	float M_StartSeconds = -1.f;
	float M_CaseStartSeconds = -1.f;
	FVector M_Anchor = FVector::ZeroVector;
	TArray<FFootprintCase> M_Cases;
	int32 M_CaseIndex = INDEX_NONE;
	int32 M_NextShot = 0;
	TArray<FShot> M_Shots;
	int32 M_AreaHandle = INDEX_NONE;
	int32 M_ScreenshotCount = 0;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	float GetNow() const;
	void TickStart();
	void StartCase(int32 CaseIndex);
	void TickCase();
	void AimTopDown() const;
	void AimRts() const;
	void DisableFogDarkening() const;
	void RequestScreenshot(const FString& Label);
};
