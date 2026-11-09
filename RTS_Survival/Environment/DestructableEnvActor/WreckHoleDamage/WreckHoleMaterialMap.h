// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "WreckHoleMaterialMap.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct FWreckHoleMaterialPair
{
	GENERATED_BODY()

	// The opaque material a wreck slot uses while intact.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WreckHoles")
	TSoftObjectPtr<UMaterialInterface> Source;

	// Its masked `_WreckHoles` copy (Tools/WreckHoles/make_wreck_holes_material.py), swapped in on the first hole.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WreckHoles")
	TSoftObjectPtr<UMaterialInterface> Damaged;
};

/**
 * @brief One table for every wreck: which masked material replaces a slot's material once that mesh gets a hole.
 * UWreckHoleDamageComponent looks slots up here, so no wreck Blueprint lists materials. The authoring script
 * registers each pair with RegisterDamagedMaterial; a slot without an entry never gets holes.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UWreckHoleMaterialMap : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * @brief The masked replacement for a slot material, or null when the material has none.
	 * @param SlotMaterial The slot's current material; a dynamic instance is looked up by its parent.
	 * @return Soft pointer to the damaged material; null when not registered.
	 */
	TSoftObjectPtr<UMaterialInterface> FindDamagedMaterial(const UMaterialInterface* SlotMaterial) const;

	// Editor tooling (Python): adds or replaces the pair for Source.
	UFUNCTION(BlueprintCallable, Category="WreckHoles")
	void RegisterDamagedMaterial(UMaterialInterface* Source, UMaterialInterface* Damaged);

	UFUNCTION(BlueprintPure, Category="WreckHoles")
	int32 GetPairCount() const { return Pairs.Num(); }

protected:
#if WITH_EDITOR
	// A pair edited in the details panel must not leave a stale lookup.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WreckHoles")
	TArray<FWreckHoleMaterialPair> Pairs;

private:
	void EnsureLookupBuilt() const;

	// Source path -> index into Pairs; built on the first lookup, rebuilt when Pairs changes size.
	mutable TMap<FSoftObjectPath, int32> M_LookupBySource;
	mutable int32 M_LookupBuiltForPairCount = INDEX_NONE;
};
