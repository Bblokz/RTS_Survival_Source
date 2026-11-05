// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FireVfxStudySubsystem.generated.h"

class ACameraActor;
class UNiagaraComponent;

UENUM()
enum class EFireVfxStudyPhase : uint8
{
	WaitingForStart,
	Gallery,
	PerfBaseline,
	PerfBurning,
	Finished
};

/**
 * @brief -FireVfxStudy: lines up ground fire candidates (the pooled GroundFire as the fire manager places it, other
 * fire systems) and photographs each from the RTS camera and from the ground, then measures the frame cost of the
 * fire subsystem's ground fires at the configured cap. Screenshots to Saved/FireVfxStudy; logs RTS_FIRE_STUDY.
 * Anchor: -FireStrikesAnchorX= / -FireStrikesAnchorY=. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFireVfxStudySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FStudyItem
	{
		FString Label;
		FString SystemPath;
		// <= 0: place it through the fire subsystem (RequestGroundFire) with this radius instead of spawning it here.
		float GroundFireRadius = 0.f;
		FVector ComponentScale = FVector::OneVector;
		float UserScale = 1.f;
		FVector Location = FVector::ZeroVector;
	};

	struct FPerfSamples
	{
		TArray<float> GameMs;
		TArray<float> RenderMs;
		TArray<float> GpuMs;
	};

	EFireVfxStudyPhase M_Phase = EFireVfxStudyPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	FVector M_Anchor = FVector::ZeroVector;
	TArray<FStudyItem> M_Items;
	int32 M_ShotIndex = 0;
	float M_NextShotSeconds = 0.f;
	int32 M_ScreenshotCount = 0;
	FPerfSamples M_Baseline;
	FPerfSamples M_Burning;
	int32 M_PerfFireCount = 0;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraComponent>> M_GalleryComponents;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	float GetNow() const;
	void SetPhase(EFireVfxStudyPhase NewPhase);
	void TickWaitingForStart();
	void BuildItems();
	void DisableFogDarkening() const;
	void SpawnGallery();
	void TickGallery();
	void AimCamera(const FVector& Target, float Pitch, float Distance) const;
	void ClearGallery();
	void StartBurningPerf();
	void TickPerf(FPerfSamples& Samples);
	void LogPerf(const TCHAR* Label, const FPerfSamples& Samples) const;
	void RequestScreenshot(const FString& Label);
	void Finish();
};
