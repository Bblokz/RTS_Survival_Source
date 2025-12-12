
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/RTSGamePlayTags/FRTSGamePlayTags.h"
#include "FRTSGamePlayTagsBPLibrary.generated.h"

class UActorComponent;

/**
 * @brief Blueprint function library exposing RTS gameplay tags.
 * Provides easy access to gameplay tags in Blueprint graphs.
 */
UCLASS()
class RTS_SURVIVAL_API UFRTSGamePlayTagsBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Get the ScavengeMeshComponent gameplay tag.
	 * @return FGameplayTag that marks a mesh component used for scavenge socket discovery.
	 */
	UFUNCTION(BlueprintPure, Category="RTS|GameplayTags")
	static FGameplayTag GetScavengeMeshComponentTag();

	/**
	 * @brief Adds the ScavengeMeshComponent tag to the given component.
	 * @param Component The component to tag.
	 * @details If the component doesn't expose a mutable tag interface, we add the tag name to ComponentTags.
	 *          This lets designers set/inspect it in the Details panel.
	 */
	UFUNCTION(BlueprintCallable, Category="RTS|GameplayTags")
	static void AddScavengeTagToComponent(UActorComponent* Component);
};

/**
 * @brief Lightweight helpers for gameplay-tag queries in C++.
 */
class FRTSGamePlayTagsHelpers
{
public:
	/** @return true if Component owns the Scavenge tag (via IGameplayTagAssetInterface or ComponentTags). */
	static bool DoesComponentHaveScavengeTag(const UActorComponent* Component);
};
