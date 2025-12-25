// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/AsyncBxpLoadingType/AsyncBxpLoadingType.h"
#include "UObject/Interface.h"
#include "BuildingExpansionOwner.generated.h"

class ACPPController;
struct RTS_SURVIVAL_API FBuildingExpansionItem;
enum class EBuildingExpansionStatus : uint8;
enum class EBuildingExpansionType : uint8;
class UBuildingExpansionOwnerComp;
class RTS_SURVIVAL_API ABuildingExpansion;
// This class does not need to be modified.
UINTERFACE(MinimalAPI, NotBlueprintable)
class UBuildingExpansionOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * @note -----------------------------------------------------
 * @note In the derived cpp class make sure to implement:
 * @note GetBuildingExpansionData and IsBuildingAbleToExpand.
 * @note -----------------------------------------------------
 * @note In derived blueprint of this interface make sure to implement the InitBuildingExpansionOptions function
 * @note as well as the CreateBuildingExpansionAtLevel function.
 * @note -----------------------------------------------------
 * @note Apart from the Bxp itself, only this object is in charge of destroying Bxp's.
 * @note -----------------------------------------------------
 * 
 * @brief A building expansion actor can be added to a building in an expansion slot.
 * These slots are stored in a TArray in the building expansion owner component called UBuildingExpansionOwnerComp.
 * To interact with this component accross multiple different derived actor classes we use the IBuildingExpansionOwner interface.
 *
 * @note Building Expansions are spawned async using the ARTSAsyncSpawner class while their preview mesh is loaded sync with game thread.
 */
class RTS_SURVIVAL_API IBuildingExpansionOwner
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/**
	* @return The name of the actor that implements this interface.
	*/
	FString GetOwnerName() const;

	bool GetIsAsyncBatchLoadingAttachedPackedExpansions() const;

	virtual float GetBxpExpansionRange() const;
	virtual FVector GetBxpOwnerLocation() const;

	virtual UStaticMeshComponent* GetAttachToMeshComponent() const;

	/**
	 * @return An array of all building expansion items that this building can expand with.
	 * @note An item is a struct with the following members:
	 * @note -----------------------------------------------------
	 * @note ABuildingExpansion* Expansion, a pointer to the building expansion, is null if not built!
	 * @note -----------------------------------------------------
	 * @note ExpansionType, the type of expansion.
	 * @note -----------------------------------------------------
	 * @note ExpansionStatus: What is the status of construction/building this expansion?
	 * @note NotUnlocked, UnlockedButNotBuilt, LookingForPlacement, BeingBuilt, Built, BeingPackedUp, PackedUp.
	 * @note -----------------------------------------------------
	 * The Bxp Construction rules which define how it can be placed: free, socket, at building origin.
	 * It also saves the socket name if it is a socket type.
	 */
	TArray<FBuildingExpansionItem>& GetBuildingExpansions() const;

	// /**
	//  * In the off chance that a building expansion is cancelled before it is loaded we need to update the array
	//  * and inform the game UI of the change.
	//  * @param Index The index in the array of building expansions.
	//  * @param bIsCancelledPackedExpansion Whether this expansion was packed up or not. If it was we make sure
	//  * to keep the type of expansion.
	//  */
	// void CancelBxpThatWasNotLoaded(const int Index, const bool bIsCancelledPackedExpansion);

	/** @note Can be null if faulty index is supplied. */
	ABuildingExpansion* GetBuildingExpansionAtIndex(const int Index) const;
	FBuildingExpansionItem* GetBuildingExpansionItemAtIndex(const int Index) const;

	/** @return An array of all unlocked building expansion types that this building can expand with.*/
	TArray<FBxpOptionData> GetUnlockedBuildingExpansionTypes() const;

		EBxpOptionSection GetBxpOptionTypeFromBxpType(const EBuildingExpansionType BxpType) const;

	/** @return Whether the building is in a state in which expanding is possible. */
	virtual bool IsBuildingAbleToExpand() const;

	void BatchAsyncLoadAttachedPackedBxps(const ACPPController* PlayerController, const TScriptInterface<IBuildingExpansionOwner>& OwnerSmartPointer) const;

	void BatchBxp_OnInstantPlacementBxpsSpawned(TArray<ABuildingExpansion*> SpawnedBxps, TArray<int32> BxpItemIndices, TArray<FBxpOptionData>
	                                            BxpTypesAndConstructionRules, const ACPPController* PlayerController, const TScriptInterface<IBuildingExpansionOwner>
	                                            & OwnerSmartPointer) const;

	/**	
	 * Setup for the building expansion at the given index.
	 * @param BuildingExpansion The building expansion spawned.
	 * @param BuildingExpansionIndex The index in the array of building expansions to add the expansion to.
	 * @param BuildingExpansionTypeData The type of building expansion created
	 * @param bIsUnpackedExpansion Whether the expansion is unpacked or not.
	 * @param OwnerScriptInterface
	 * @pre The RTSAsyncSpawner has the correct mapping from bxp type to class and the class is spawned successfully.
	 * @post The building expansion is added to the array of building expansions.
	 * @post The bxp status is set to beingbuilt if it is a clean build, or beingUnpacked if it is unpacked.
	 */
	void OnBuildingExpansionCreated(
		ABuildingExpansion* BuildingExpansion,
		const int BuildingExpansionIndex,
		const FBxpOptionData& BuildingExpansionTypeData,
		const bool bIsUnpackedExpansion, const TScriptInterface<IBuildingExpansionOwner>& OwnerScriptInterface);

	/**
	 * @brief Destroys the bxp and updates the array accoordingly.
	 * @param BuildingExpansion The building expansion to destroy.
	 * @param bDestroyPackedBxp Whether the expansion is packed up or not. If it is we set the status back to packed
	 * and make sure to save the type of expansion.
	 */
	void DestroyBuildingExpansion(ABuildingExpansion* BuildingExpansion, const bool bDestroyPackedBxp);


	/**
	 * Updates expansion status for the building expansion if it is contained in the array.
	 * @param BuildingExpansion The building expansion part of this buildingt's expansion array.
	 * @param bCalledUponDestroy IF set to true the building expansion is informing the owner that it is being destroyed
	 * and that the building UI needs to change, we do not report errors for an invalid object in this case.
	 * @post Updates the game UI if the owner is the main selected actor.
	 */
	void UpdateExpansionData(const ABuildingExpansion* BuildingExpansion, const bool bCalledUponDestroy = false) const;

	/**
	 * @brief A special bxp data update that is only called once a new construction location for a bxp is determined.
	 * In this case we change the rotation and socket name of the bxp which need to be saved if a bxp is later unpacked
	 * and of the type that it can be placed at a socket.
	 *
	 * @note If the bxp is of attach to origin then the socket name is None but the rotation will be used when unpacked.
	 * @param BuildingExpansion BuldingExpansion that is being constructed.
	 * @param BxpRotation The rotation of the building expansion that is being constructed.
	 * @param AttachedToSocketName The name of the socket that the building expansion is attached to.
	 */
	void OnBxpConstructionStartUpdateExpansionData(
		const ABuildingExpansion* BuildingExpansion,
		const FRotator& BxpRotation,
		const FName& AttachedToSocketName) const;

	/**
	 * Updates the data array of expansions and the Game UI if the owner is the main selected actor.
	 * @param BuildingExpansion Expansion that was destroyed.
	 * @param bDestroyedPackedExpansion Whether the expansion was packed up or not. If it was we set the status back to packed.
	 */
	virtual void OnBuildingExpansionDestroyed(ABuildingExpansion* BuildingExpansion,
	                                  const bool bDestroyedPackedExpansion);

	/** @return The list of all sockets that are available for Bxps but do not have Bxps on them yet.*/
	virtual TArray<UStaticMeshSocket*> GetFreeSocketList(const int32 ExpansionSlotToIgnore) const = 0;

	/** @return All the sockets assoicated with a BXP placed or packed in the building expansion slots.
	 * @param ExpansionSlotToIgnore: the active expansion slot of which we do not look at whether it occupies anything.*/
	TArray<FName> GetOccupiedSocketNames(const int32 ExpansionSlotToIgnore) const;

	bool HasBxpItemOfType(const EBuildingExpansionType Type) const;

	/**
	 * @brief This is used when the player is previewing and then clicking another option instead of placing the bxp.
	 * In that case we need to get the type of what bxp is getting cancelled so we can refund it.
	 * However if the bxp was looking to unpack then we do not refund as it stays in the BXP item.
	 * @param bOutIsLookingToUnpack Whether we are cancelling an unpacking bxp.
	 * @return The type of the unpacking bxp; possibly set to Invalid type if none available.
	 */
	EBuildingExpansionType GetTypeOfBxpLookingForPlacement(bool& bOutIsLookingToUnpack)const;
	

protected:
	virtual UBuildingExpansionOwnerComp& GetBuildingExpansionData() const = 0;
	/**
	 * @brief Setup the amount of building expansions that this building can expand with as well as which
	 * expansions are unlocked.
	 * @note do not override; has to be virtual because of BlueprintCallable.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts", meta = (BlueprintProtected = "true"))
	virtual void InitBuildingExpansionOptions(
		const int NewAmountBuildingExpansions,
		TArray<FBxpOptionData> NewUnlockedBuildingExpansionTypes);

	/**
	 * @brief Start pack up animation on all expansions but will not destroy them yet.
	 * @param TotalOwnerPackUpTime How long it takes the owning vehicle to convert.
	 */
	void StartPackUpAllExpansions(const float TotalOwnerPackUpTime) const;

	void CancelPackUpExpansions() const;

	/**
	 * @brief Destroys all expansions with status packed up
	 */
	void FinishPackUpAllExpansions() const;

private:
	/**
	 * @brief Setsup an array entry of the BuildingExpansion Data.
	 * @param BuildingExpansionIndex Index in the array of building expansions.
	 * @param BxpConstructionRulesAndType The type of building expansion.
	 * @param BuildingExpansionStatus The status of the building expansion.
	 * @param BuildingExpansion The building expansion to add.
	 * @note Called when a building expansion is created by the owner.
	 */
	void InitBuildingExpansionEntry(
		const int BuildingExpansionIndex,
		const FBxpOptionData& BxpConstructionRulesAndType,
		const EBuildingExpansionStatus BuildingExpansionStatus,
		ABuildingExpansion* BuildingExpansion) const;

	void ResetEntryAndUpdateUI(
		const int32 Index,
		const bool bResetForPackedUpExpansion = false) const;
	bool GetHasAlreadyBuiltType(EBuildingExpansionType BuildingExpansionType) const;

	void Error_BxpSetToSocketNoName(const ABuildingExpansion* Bxp) const;
	void Error_BxpNoSocketRuleButHasSocketName(const ABuildingExpansion* Bxp, const FName& SocketName) const;

	bool BatchBxp_EnsureInstantPlacementRequestValid(
		TArray<ABuildingExpansion*> SpawnedBxps,
		const ACPPController* PlayerController,
		const TScriptInterface<IBuildingExpansionOwner>& OwnerSmartPointer) const;

	void BatchBxp_OnBuildingCanNoLongerExpand(
		TArray<ABuildingExpansion*> SpawnedBxps, TArray<int32> IndicesToReset) const;

	
};
