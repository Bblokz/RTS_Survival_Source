// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SurvivalMapContentVolume.generated.h"

class UBoxComponent;
class UPCGComponent;

/**
 * @brief Runs a survival map's own content graph (capturables, resources, wrecks, animals) once per match with a new
 * seed, so the same terrain plays differently. A map with this volume skips the generic points of interest.
 * @note Place one over the playable area and set the graph on its PCG component; leave the generation on demand.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASurvivalMapContentVolume : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalMapContentVolume();

	/**
	 * @brief Called by the survival director when the deployment starts.
	 * @param WorldContextObject Any object of the match's world.
	 * @return True when the map has a content volume and it started generating.
	 */
	static bool StartForMatch(const UObject* WorldContextObject);

protected:
	// 0 picks a new seed every match; any other value gives the same content every time (testing, set pieces).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Map Content")
	int32 FixedSeed = 0;

private:
	UPROPERTY(VisibleAnywhere, Category = "Survival|Map Content")
	TObjectPtr<UBoxComponent> M_Bounds = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Survival|Map Content")
	TObjectPtr<UPCGComponent> M_PCGComponent = nullptr;

	int32 ResolveMatchSeed() const;
	bool GenerateForMatch();
	bool GetIsValidPCGComponent() const;
};
