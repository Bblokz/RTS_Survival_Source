// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NomadicAttachment.generated.h"

class URTSOptimizer;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ENomadicStaticAxis : uint8
{
	X UMETA(DisplayName="X (Roll)"),
	Y UMETA(DisplayName="Y (Pitch)"),
	Z UMETA(DisplayName="Z (Yaw)")
};

USTRUCT(BlueprintType)
struct FStaticRotationMeshSettings
{
	GENERATED_BODY()

	// Minimum seconds to wait between rotation moves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Static Rotation")
	float MinWaitBetweenRotations = 0.5f;

	// Maximum seconds to wait between rotation moves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Static Rotation")
	float MaxWaitBetweenRotations = 1.5f;

	// Candidate amounts (in degrees) to rotate each iteration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Static Rotation")
	TArray<float> RotationDeltasDeg = { 15.f, 30.f, 45.f };

	// Axis to rotate the mesh around (local space).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Static Rotation")
	ENomadicStaticAxis Axis = ENomadicStaticAxis::Z;

	// Rotation speed in degrees per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Static Rotation", meta=(ClampMin="0.0"))
	float RotationSpeedDegPerSec = 90.f;
};

UCLASS()
class RTS_SURVIVAL_API ANomadicAttachment : public AActor
{
	GENERATED_BODY()

public:
	ANomadicAttachment();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	URTSOptimizer* OptimizationComponent = nullptr;
	// Can be set by nomadic attachments that inherit and do not use static mesh rotation.
	bool bM_UseStaticRotation = true;

public:
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Nomadic Attachment|Static Rotation")
	void StartStaticRotatationOn(UStaticMeshComponent* MeshCompToRotate, FStaticRotationMeshSettings RotationSettings);

	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Static Rotation")
	void StopStaticRotation();


private:
	void StaticMeshRotation(float DeltaSeconds);
	
	// Begin a new rotation iteration by picking a delta from settings.
	void BeginNextRotation();

	// Apply an incremental delta (in degrees) along the configured axis.
	void ApplyAxisDelta(float DeltaDegrees);

	// Validates that a rotation mesh has been set.
	bool EnsureStaticRotationMeshIsValid() const;
	void BeginNextStaticRotation();
	void ApplyStaticAxisDelta(float DeltaDegrees);

	// Mesh currently being rotated.
	TWeakObjectPtr<UStaticMeshComponent> M_StaticRotatingMesh = nullptr;

	// Settings used for the current static rotation session.
	FStaticRotationMeshSettings M_StaticRotationSettings;

	// Seconds remaining to wait before next rotation iteration.
	float M_TimeUntilNextRotation = 0.f;

	// Degrees remaining to complete the current rotation iteration (signed).
	float M_RemainingDeltaDeg = 0.f;

	// Whether we are currently in a waiting window.
	bool M_bWaiting = false;
};
