// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"

#include "BuildingExpansionOwnerComp.generated.h"





class ATankMaster;
class RTS_SURVIVAL_API ACPPController;
/**
 * contains all the data for all the building expansion items for a building.
 * The MainUI can obtain this data using the IBuildingExpansionOwner interface.
 * @note Init this in bp with reference casts at beginplay using InitBuildingExpansionComp
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UBuildingExpansionOwnerComp : public UActorComponent
{
	GENERATED_BODY()

	// The owner of this component has access to the private members of this class so we don't need to
	// define a million getters and setters.
	friend class RTS_SURVIVAL_API IBuildingExpansionOwner;

public:
	// Sets default values for this component's properties
	UBuildingExpansionOwnerComp(FObjectInitializer const& ObjectInitializer);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts")
	void InitBuildingExpansionComp(ACPPController* NewPlayerController);

	void SetIsAsyncBatchLoadingPackedExpansions(const bool bIsActiveLoading)
	{
		bM_IsAsyncLoadingPackedExpansionsInBatch = bIsActiveLoading;
	
	}
	inline bool GetIsAsyncBatchLoadingPackedExpansions() const
	{
		return bM_IsAsyncLoadingPackedExpansionsInBatch;
	}

protected:
	virtual void BeginPlay() override;

private:
	/** @brief The status of all the building expansion items.
	 * @note contains:
	 * @note ABuildingExpansion* Expansion, a pointer to the building expansion.
	 * @note ExpansionType, the type of expansion.
	 * @note ExpansionStatus, NotUnlocked, NotBuilt, Built, BeingBuilt, BeingPackedUp, PackedUp.
	 */
	UPROPERTY()
	TArray<FBuildingExpansionItem> M_TBuildingExpansionItems;

	/**
	 * @briefCollection of all unlocked possible expansion types that this building can expand with.
	* Contains the construction rules which define how the bxp is placed: free, socket, at building origin.
	* Contains rules about how to use collision when construction.
	* Contains the type that identifies the bxp.
	* @note Note that, before setting up the options UI we first go through all the bxp items
	* @note and check if a bxp item with the construction rule at building original already exists
	* @note If so then we do not allow that option to be built again! (prevent overlap)
	*/ 
	UPROPERTY()
	TArray<FBxpOptionData> M_TUnlockedBuildingExpansionTypes;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;


	/**
	 * Possibly updates the button in the main game UI if the owner of this component is the main selected actor.
	 * @param IndexBExpansion The index in the array of building expansions.
	 * @param NewStatus The status of the building expansion widget.
	 * @param NewType The type of the building expansion widget.
	 * @param ConstructionRules 
	 * @return 
	 */
	void UpdateMainGameUIWithStatusChanges(
		const int IndexBExpansion,
		const EBuildingExpansionStatus NewStatus,
		const EBuildingExpansionType NewType, const FBxpConstructionRules& ConstructionRules) const;

	// Set to true after converting to buidling and calling BatchAsyncLoadAttachedPackedBxps on the owner.
	// This prevents the user from manually activating the async loading of a packed expansion that is in that batch.
	bool bM_IsAsyncLoadingPackedExpansionsInBatch = false;
};
