// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BirdFlock.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

// Per-instance custom data slots read by the bird VAT material (M_Bird_VAT); keep both in sync.
namespace BirdFlockInstanceData
{
	constexpr int32 PhaseSeconds = 0;
	constexpr int32 PlayRate = 1;
	constexpr int32 NumFloats = 2;
}

/** The ellipse the flock circles; the actor location is its centre and the actor yaw its orientation. */
USTRUCT(BlueprintType)
struct FBirdFlockEllipse
{
	GENERATED_BODY()

	// Half length of the ellipse along the actor's forward axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ellipse", meta=(ClampMin="100"))
	float SemiAxisX = 20000.f;

	// Half length of the ellipse along the actor's right axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ellipse", meta=(ClampMin="100"))
	float SemiAxisY = 9000.f;

	// Direction seen from above; the flock starts at the actor's +X end of the ellipse.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ellipse")
	bool bClockwise = false;
};

USTRUCT(BlueprintType)
struct FBirdFlockFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="1"))
	int32 BirdCount = 8;

	// Height above the ground (or above the actor when not following terrain).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float FlightHeight = 1500.f;

	// Each bird gets a fixed random height offset in [-x, x].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float HeightSpread = 250.f;

	// Slow per-bird rise and fall so the flock does not look like it runs on rails.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float HeightDriftAmplitude = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.1"))
	float HeightDriftPeriod = 14.f;

	// Speed along the ellipse in cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="1"))
	float Speed = 900.f;

	// Each bird flies at Speed * (1 +- this); small values keep the flock together for a few laps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="0.5"))
	float SpeedVariation = 0.03f;

	// How far the flock stretches along the ellipse, in radians of ellipse angle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float FormationLength = 0.12f;

	// Sideways spread of the flock across the ellipse, in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float FormationWidth = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float MaxBankDegrees = 25.f;

	// Degrees of bank per degree/second of turning.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0"))
	float BankPerTurnRate = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.01"))
	float MinScale = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0.01"))
	float MaxScale = 1.1f;

	// Wing-beat speed varies per bird by +- this fraction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight", meta=(ClampMin="0", ClampMax="0.5"))
	float PlayRateVariation = 0.1f;

	// Keep FlightHeight above the landscape under each bird instead of above the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
	bool bFollowTerrain = true;

	// Seed for the per-bird randomisation, so a placed flock always looks the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flight")
	int32 Seed = 1337;
};

/** Runtime state of one bird; index matches the instance index. */
USTRUCT()
struct FBirdFlockBird
{
	GENERATED_BODY()

	float EllipseAngle = 0.f;
	float SpeedScale = 1.f;
	float SemiAxisScale = 1.f;
	float LateralOffset = 0.f;
	float HeightOffset = 0.f;
	float DriftPhase = 0.f;
	float Scale = 1.f;
	float GroundHeight = 0.f;
	float Roll = 0.f;
	float PreviousYaw = 0.f;
	bool bHasPreviousYaw = false;
};

/**
 * @brief Ambient flock of VAT birds circling a very large ellipse around the actor, drawn as one instanced mesh.
 * Place a Blueprint child in a desert map, set BirdMesh (a VAT mesh using M_Bird_VAT) and size the ellipse.
 * Birds have no collision and cost one draw call per flock; the wing beats run on the GPU.
 */
UCLASS()
class RTS_SURVIVAL_API ABirdFlock : public AActor
{
	GENERATED_BODY()

public:
	ABirdFlock();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Birds")
	TObjectPtr<UInstancedStaticMeshComponent> BirdInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Birds")
	TObjectPtr<UStaticMesh> BirdMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Birds")
	FBirdFlockEllipse Ellipse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Birds")
	FBirdFlockFlightSettings FlightSettings;

private:
	/** Recreates the bird states and instances; used in the editor (preview) and on BeginPlay. */
	void BuildFlock();

	FBirdFlockBird MakeBird(FRandomStream& Random, const int32 BirdIndex) const;

	/**
	 * @brief Advances one bird along its ellipse with constant arc speed and returns its instance transform.
	 * @param Bird Bird to advance; its angle, bank and yaw history are updated.
	 * @param DeltaSeconds Frame time; 0 only evaluates the current pose.
	 * @param WorldTime Game time, drives the height drift.
	 * @return The bird's world transform.
	 */
	FTransform AdvanceBird(FBirdFlockBird& Bird, const float DeltaSeconds, const float WorldTime) const;

	/** @return Local ellipse point (actor space, without height) for a bird at an ellipse angle. */
	FVector GetEllipsePoint(const FBirdFlockBird& Bird, const float Angle) const;

	void UpdateGroundHeightRoundRobin(const float DeltaSeconds);
	float TraceGroundHeight(const FVector& WorldLocation) const;

	bool GetIsValidBirdInstances() const;

	TArray<FBirdFlockBird> M_Birds;

	// Reused every tick to avoid reallocating the batch update buffer.
	TArray<FTransform> M_InstanceTransforms;

	// Index of the bird whose ground height is traced next; one trace per tick keeps terrain following cheap.
	int32 M_NextGroundTraceIndex = 0;
};
