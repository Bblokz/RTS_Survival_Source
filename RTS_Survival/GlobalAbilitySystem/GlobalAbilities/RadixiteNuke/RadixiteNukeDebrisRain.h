// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RadixiteNuke.h"
#include "RadixiteNukeDebrisRain.generated.h"

class FRadixiteNukeGroundSampler;
class UInstancedStaticMeshComponent;

/** @brief One stone of the rain: a ballistic arc from the crater to its landing spot. */
USTRUCT()
struct FRadixiteNukeDebrisStone
{
	GENERATED_BODY()

	int32 MeshIndex = 0;
	int32 InstanceIndex = INDEX_NONE;
	FVector LaunchLocation = FVector::ZeroVector;
	FVector LaunchVelocity = FVector::ZeroVector;
	FVector LandingLocation = FVector::ZeroVector;
	float FlightSeconds = 1.f;
	FQuat StartRotation = FQuat::Identity;
	FVector SpinAxis = FVector::UpVector;
	float SpinRadiansPerSecond = 0.f;
	FVector Scale = FVector::OneVector;
	bool bHasLanded = false;
};

/**
 * @brief Stones the blast throws out of the crater; they arc up through the fireball and rain down around it for a
 * few seconds, each landing with a dust puff (and some with a clatter), then stay on the ground. Spawned by
 * ARadixiteNuke at the blast with its assets loaded; one instanced mesh per stone mesh, no collision.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ARadixiteNukeDebrisRain : public AActor
{
	GENERATED_BODY()

public:
	ARadixiteNukeDebrisRain();

	/**
	 * @brief Throws the stones; call once right after spawning at ground zero.
	 * @param DebrisRain Stones, flight and landing; its assets have to be loaded.
	 * @param BlastRadius Reach of the blast in cm; the landing ranges are parts of it.
	 */
	void StartRain(const FRadixiteNukeDebrisRainSettings& DebrisRain, const float BlastRadius);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> M_StoneMeshes;

	// Plain data, no object references.
	TArray<FRadixiteNukeDebrisStone> M_Stones;

	UPROPERTY()
	FRadixiteNukeDebrisRainSettings M_DebrisRain;

	float M_RainSeconds = 0.f;
	int32 M_LandedStones = 0;
	int32 M_LandingSoundsPlayed = 0;

	void CreateStoneMeshes();
	void LaunchStone(const int32 StoneIndex, const float BlastRadius, const FRadixiteNukeGroundSampler& Ground);
	FTransform GetFlyingTransform(const FRadixiteNukeDebrisStone& Stone) const;
	void LandStone(FRadixiteNukeDebrisStone& Stone);
};
