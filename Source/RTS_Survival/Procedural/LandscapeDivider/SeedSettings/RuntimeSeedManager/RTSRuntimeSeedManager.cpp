// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSRuntimeSeedManager.h"


URTSRuntimeSeedManager::URTSRuntimeSeedManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PlacedObjectsRandomStream.bIsInitialised = false;

}

bool URTSRuntimeSeedManager::GetSeededRandInt(const int32 Min, const int32 Max, int32& OutRandomInteger) const
{
	if(not PlacedObjectsRandomStream.bIsInitialised)
	{
		return false;
	}
	OutRandomInteger = PlacedObjectsRandomStream.RandomStreamForPlacedObjects.RandRange(Min, Max);
	return true;
}

bool URTSRuntimeSeedManager::GetSeededRandFloat(const float Min, const float Max, float& OutRandomFloat) const
{
	if(not PlacedObjectsRandomStream.bIsInitialised)
	{
		return false;
	}
	OutRandomFloat = PlacedObjectsRandomStream.RandomStreamForPlacedObjects.FRandRange(Min, Max);
	return true;
}


void URTSRuntimeSeedManager::BeginPlay()
{
	Super::BeginPlay();
	
}

void URTSRuntimeSeedManager::InitializeSeed(const int32 Seed)
{
	PlacedObjectsRandomStream.RandomStreamForPlacedObjects.Initialize(Seed);
	PlacedObjectsRandomStream.bIsInitialised = true;
	
}



