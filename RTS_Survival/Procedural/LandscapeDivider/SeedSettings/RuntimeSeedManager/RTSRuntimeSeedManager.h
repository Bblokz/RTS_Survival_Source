// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Procedural/LandscapeDivider/SeedSettings/RTSRuntimeSeed.h"

#include "RTSRuntimeSeedManager.generated.h"

// Component of landscape divider that manages the runtime seed for objects that are placed in the map and cannot
// access the seeded RandomStream from the PCG system.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSRuntimeSeedManager : public UActorComponent
{
	GENERATED_BODY()

	friend class ARTSLandscapeDivider;

public:
	URTSRuntimeSeedManager();

	/**
	 * @param Min Min for range.
	 * @param Max Max for range. 
	 * @param OutRandomInteger The random integer that was generated with the seed system.
	 * @return Whether the seed system is initialized and the random integer was generated.
	 */
	[[nodiscard]] bool GetSeededRandInt(const int32 Min, const int32 Max, int32& OutRandomInteger) const;
	/**
	 * 
	 * @param Min Min for range.
	 * @param Max Max for range.
	 * @param OutRandomFloat The random float that was generated with the seed system.
	 * @return Whether the seed system is initialized and the random float was generated. 
	 */
	[[nodiscard]] bool GetSeededRandFloat(const float Min, const float Max, float& OutRandomFloat) const;

protected:
	virtual void BeginPlay() override;

private:
	void InitializeSeed(const int32 Seed);

	// Keeps track of the random stream for the placed objects.
	FRTSRuntimeSeed PlacedObjectsRandomStream;
};
