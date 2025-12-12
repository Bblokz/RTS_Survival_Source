// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GameDecalManager.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UGameDecalManager::UGameDecalManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGameDecalManager::InitDecalManager(TArray<FDecalTypeSystems> DecalSystemsData)
{
	M_DecalMaterialsMap.Empty();
	for (const FDecalTypeSystems& Entry : DecalSystemsData)
	{
		TArray<UMaterialInterface*>& SystemsForType = M_DecalMaterialsMap.FindOrAdd(Entry.DecalType);
		for (UMaterialInterface* DecalMaterial : Entry.DecalMaterials)
		{
			if (DecalMaterial)
			{
				SystemsForType.AddUnique(DecalMaterial);
			}
		}
	}
}

void UGameDecalManager::SpawnRTSDecalAtLocation(const ERTS_DecalType DecalType, const FVector& SpawnLocation,
                                                const FVector2D& MinMaxSize, const FVector2D& MinMaxXYOffset, const float LifeTime)
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	UMaterialInterface* DecalMaterial = PickRandomDecalForType(DecalType);
	if (not DecalMaterial)
	{
		return;
	}
	// Create a random size for the decal within the specified range
	float Size = FMath::RandRange(MinMaxSize.X, MinMaxSize.Y);
	const float RTSDecalHeight = DeveloperSettings::GamePlay::Environment::RTSDecalHeight;
	const float XOffset = FMath::RandRange(MinMaxXYOffset.X, MinMaxXYOffset.Y);
	const float YOffset = FMath::RandRange(MinMaxXYOffset.X, MinMaxXYOffset.Y);
	const FVector DecalLocation = SpawnLocation + FVector(XOffset, YOffset, 0.f);
	if (Size <= 0)
	{
		Size = 300;
	}
	if (FMath::IsNearlyZero(LifeTime))
	{
		UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial, FVector(RTSDecalHeight, Size, Size), DecalLocation);
	}
	else
	{
		UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial, FVector(RTSDecalHeight, Size, Size), DecalLocation,
		                                       FRotator(-90, 0.f, 0.f), LifeTime);
	}
}


void UGameDecalManager::BeginPlay()
{
	Super::BeginPlay();
}

UMaterialInterface* UGameDecalManager::PickRandomDecalForType(const ERTS_DecalType DecalType)
{
	const TArray<UMaterialInterface*>* SystemsArrayPtr = M_DecalMaterialsMap.Find(DecalType);
	if (!SystemsArrayPtr || SystemsArrayPtr->Num() == 0)
	{
		const FString EnumValueName = UEnum::GetValueAsString(DecalType);
		RTSFunctionLibrary::ReportError(
			"[UGameDecalManager] No decals found for this type: " + EnumValueName +
			"\nUGameDecalManager::PickRandomDecalForType");
		return nullptr;
	}
	// Randomly pick a decal from the array
	const TArray<UMaterialInterface*>& SystemsArray = *SystemsArrayPtr;
	const int32 RandomIndex = FMath::RandRange(0, SystemsArray.Num() - 1);
	return SystemsArray[RandomIndex];
}
