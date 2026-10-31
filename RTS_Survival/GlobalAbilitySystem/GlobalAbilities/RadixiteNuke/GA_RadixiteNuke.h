// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RadixiteNuke.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GA_RadixiteNuke.generated.h"

struct FStreamableHandle;

/**
 * @brief A radixite warhead comes down on the target after a warning everyone can see. The blast reaches the aim
 * area's AreaRadius and the core its InnerRadius, so the target area shows what will happen; both sides are hurt.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_RadixiteNuke : public UGlobalAbility
{
	GENERATED_BODY()

public:
	virtual void ExecuteAbilityAtLocation(const FVector& TargetLocation) override;
	virtual void BeginDestroy() override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FRadixiteNukeSettings M_NukeSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FRadixiteNukeVisuals M_NukeVisuals;

	// Keeps the async asset loads alive until their callbacks can spawn the nuke.
	TArray<TSharedPtr<FStreamableHandle>> M_LoadHandles;

	void OnAssetsLoaded(const FVector& GroundZero);
	void GiveOwnerVisionOfBlast(const FVector& GroundZero) const;
	TArray<FSoftObjectPath> GetAssetsToLoad() const;
};
