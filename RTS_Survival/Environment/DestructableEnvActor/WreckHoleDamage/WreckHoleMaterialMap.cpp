// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WreckHoleMaterialMap.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

TSoftObjectPtr<UMaterialInterface> UWreckHoleMaterialMap::FindDamagedMaterial(
	const UMaterialInterface* SlotMaterial) const
{
	// A runtime dynamic instance carries no asset path; its parent is the registered material.
	const UMaterialInterface* LookupMaterial = SlotMaterial;
	while (const UMaterialInstanceDynamic* const DynamicInstance = Cast<UMaterialInstanceDynamic>(LookupMaterial))
	{
		LookupMaterial = DynamicInstance->Parent;
	}
	if (not IsValid(LookupMaterial))
	{
		return nullptr;
	}
	EnsureLookupBuilt();
	const int32* const PairIndex = M_LookupBySource.Find(FSoftObjectPath(LookupMaterial));
	if (PairIndex == nullptr || not Pairs.IsValidIndex(*PairIndex))
	{
		return nullptr;
	}
	return Pairs[*PairIndex].Damaged;
}

void UWreckHoleMaterialMap::RegisterDamagedMaterial(UMaterialInterface* Source, UMaterialInterface* Damaged)
{
	if (not IsValid(Source) || not IsValid(Damaged))
	{
		return;
	}
	Modify();
	const FSoftObjectPath SourcePath(Source);
	for (FWreckHoleMaterialPair& Pair : Pairs)
	{
		if (Pair.Source.ToSoftObjectPath() == SourcePath)
		{
			Pair.Damaged = Damaged;
			M_LookupBuiltForPairCount = INDEX_NONE;
			return;
		}
	}
	FWreckHoleMaterialPair& NewPair = Pairs.AddDefaulted_GetRef();
	NewPair.Source = Source;
	NewPair.Damaged = Damaged;
	M_LookupBuiltForPairCount = INDEX_NONE;
}

#if WITH_EDITOR
void UWreckHoleMaterialMap::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	M_LookupBuiltForPairCount = INDEX_NONE;
}
#endif

void UWreckHoleMaterialMap::EnsureLookupBuilt() const
{
	if (M_LookupBuiltForPairCount == Pairs.Num())
	{
		return;
	}
	M_LookupBySource.Reset();
	for (int32 PairIndex = 0; PairIndex < Pairs.Num(); ++PairIndex)
	{
		M_LookupBySource.Add(Pairs[PairIndex].Source.ToSoftObjectPath(), PairIndex);
	}
	M_LookupBuiltForPairCount = Pairs.Num();
}
