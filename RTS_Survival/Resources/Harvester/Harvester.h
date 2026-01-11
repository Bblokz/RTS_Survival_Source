// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Resources/HarvesterCargoSlot/HarvesterCargoSlot.h"
#include "RTS_Survival/Resources/HarvesterPosition/HarvestLocation.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Harvester.generated.h"


class UAnimatedTextWidgetPoolManager;
class UAsyncFindClosestResource;
class AAIController;
class IHarvesterInterface;
class UResourceDropOff;
struct FAIRequestID;
struct FPathFollowingResult;
enum EResourceValidation : uint8;
enum EHarvesterAIAction : uint8;
class ICommands;
class UGameResourceManager;
class UResourceComponent;
class UPlayerResourceManager;
class UAsyncFindClosestDropOff;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class RTS_SURVIVAL_API UHarvester : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UHarvester();


	/** @return the resource types this harvester can harvest. */
	TArray<ERTSResourceType> GetAllowedResourceTypes();

	inline bool GetIsAbleToHarvest() const { return bM_IsAbleToHarvest; }

	TWeakObjectPtr<UResourceComponent> GetTargetResource() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline ERTSResourceType GetTargetResourceType() const { return M_TargetResourceType; }

	/**
	 * @return The amount of the specific resource type that was last harvested by this harvester that is
	 * now in the capacity and can be dropped off.
	 * @note This function is used when searcing for a drop off as we need to know how much the harvester needs to
	 * drop off.
	 */
	int32 GetDropOffAmount();

	/**
	 * Setup the internal target used for the resource.
	 * @param NewTargetResource The resource to harvest.
	 * @return false if the resource is invalid or the harvester cannot harvest this type.
	 */
	bool SetTargetResource(TWeakObjectPtr<UResourceComponent> NewTargetResource);


	inline TObjectPtr<UGameResourceManager> GetGameResourceManager() const { return M_GameResourceManager; }

	void ExecuteHarvestResourceCommand(UResourceComponent* NewTargetResource);
	void ExecuteReturnCargoCommand();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnHarvestAnimationFinished();

	// Called by owner to stop any harvesting logic.
	// Determines what needs to be done based on the current state of the harvester set by the last taken action.
	void TerminateHarvestCommand();

	/** @return The max capacity of the harvester for the targeted resource, if no resource target is set we return the first
	 * non zero field in the HarvesterCargoSpace
	 * @param OutCurrentAmount The amount of the resource currently in the cargo.
	 * @param OutResource
	 */
	int32 GetMaxCapacityForTargetResource(int32& OutCurrentAmount, ERTSResourceType& OutResource) const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; 
	/**
	 * 
	 * @param NewAllowedResources What resource types this harvester can harvest.
	 * @param MaxCargoPerResource The maximum amount of each resource type that can be stored in the cargo.
	 * @param AcceptanceRadiusResource For movement; the acceptance radius to reach the location. (TODO) 
	 * @param OwnerMeshComponent The mesh component of the owner of this component.
	 * @param HarvesterRadius The radius from the harvester location to the end of the tools; Used to determine the right
	 * harvesting position.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupHarvester(
		TArray<ERTSResourceType> NewAllowedResources,
		TMap<ERTSResourceType, int32> MaxCargoPerResource,
		const float AcceptanceRadiusResource,
		UMeshComponent* OwnerMeshComponent,
		const float HarvesterRadius = 100
	);

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	TWeakObjectPtr<UResourceComponent> TargetResource;

private:
	void HarvestAIExecuteAction(EHarvesterAIAction Action);

	// Calls DoneExecutingCommand(EAbilityID::IdHarvestResource) on the owner to finish the harvest command.
	void FinishHarvestCommand() const;

	void FinishReturnCargoCommand() const;

	/**
	 * @brief to check if we can harvest the currently targeted resource.
	 * @return Resource validation status that indicates whether we can harvest the resource.
	 */
	EResourceValidation GetCanHarvestResource();

	EHarvesterAIAction M_HarvestStatus;

	/**
	 * @brief Translates the resource validation provided into the action to take for the harvester AI.
	 * @param ResourceValidation The validation status of the resource.
	 * @return Will move to drop off if invalid resource but has some resources in cargo. Will only terminate
	 * if the resource is invalid and there is no cargo.
	 */
	EHarvesterAIAction ResourceValidationIntoAction(EResourceValidation ResourceValidation);

	/** @return Resource that is most full in cargo space, resource none if all empty. */
	ERTSResourceType GetResourceTypeOfMostFullCargo();

	/**
	 * @brief Initiates movement towards the target resource.
	 */
	void HarvestAIAction_MoveToTargetResource();

	//
	void OnUnableToMoveToResource(const ECannotMoveToResource Reason);

	// Plays the animation for the harvester.
	void HarvestAIAction_PlayHarvesterAnimation();

	/**
	 * @brief Callback function for when the move to the target resource is finished.
	 * @param RequestID The ID of the move request.
	 * @param Result The result of the path following.
	 */
	void OnMoveToTargetResourceFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

	/**
	 * @brief Asynchronously starts the request to get the closest resource to harvest.
	 */
	void HarvestAIAction_AsyncGetResource();

	/**
	* @brief Callback function to handle the result of the asynchronous request to get the closest resource.
	* @param Resources The closest resource found.
	*/
	void AsyncOnReceiveResource(TArray<TWeakObjectPtr<UResourceComponent>> Resources);

	/**
	 * @brief Asynchronously starts the request to get the closest resource drop-off.
	 */
	void HarvestAIAction_AsyncGetDropOff();

	/**
	 * @brief Callback function to handle the result of the asynchronous request to get the closest drop-off.
	 * @param DropOffs The closest drop-offs found.
	 */
	void AsyncOnReceivedDropOffs(const TArray<TWeakObjectPtr<UResourceDropOff>>& DropOffs);

	/**
	 * @brief Initiates movement towards the closest resource drop-off.
	 */
	void HarvestAIAction_MoveToDropOff();

	/**
	 * @brief Callback function for when the move to the drop-off is finished.
	 * @param RequestID The ID of the move request.
	 * @param Result The result of the path following.
	 */
	void OnMoveToDropOffFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);


	// Harvests the target resource.
	void HarvestAIAction_HarvestTargetResource();

	void HarvestAIAction_DropOffResources();

	void OnDroppedOffAllTargetResources();


	void UnstuckHarvesterTowardsLocation(
		const FVector& HarvesterLocation,
		const FVector& GoalLocation,
		const EHarvesterAIAction ActionOnUnstuck);

	void OnFinishedUnStuck(FAIRequestID RequestID, const FPathFollowingResult& Result);

	/**
	 * @brief Attempts to teleport the harvester in the direction of the provided GoalLocation.
	 *
	 * We take a 200 unit vector in the diretion of the goal from the harvester and teleport to the projected location
	 * of that direction. If the projection fails we attempt to teleport 200 units in the direction directly.
	 * If both options fail, execute FinishHarvestCommand.
	 * @param HarvesterLocation Where the harvester is.
	 * @param GoalLocation Where the harvester tries to move.
	 * @param ActionOnTeleportSuccessful Whether we are moving towards the resource or the drop off.
	 * @note With the new implementation we track what we teleport **towards** (resource vs drop-off),
	 *       allow at most **one** teleport per current goal, and blacklist that goal if a second teleport
	 *       would be required.
	 */
	void TeleportHarvesterTowardsLocation(
		const FVector& HarvesterLocation,
		const FVector& GoalLocation,
		const EHarvesterAIAction ActionOnTeleportSuccessful);

	/** @return Whether we successfully teleported the harvester to the location. */
	bool TryTeleportHarvesterToLocation(const FVector& TeleportLocation);


	TArray<ERTSResourceType> M_AllowedResources;

	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	/** @return Whether the AI controller can safely be used. */
	bool GetIsValidAIController();

	// AI controller of the owner of this component.
	UPROPERTY()
	AAIController* M_AIController;

	// Whether the owner of this component can currently harvest.
	bool bM_IsAbleToHarvest;

	ERTSResourceType M_TargetResourceType;
	// Set when we click a resource to harvest but first need to drop off cargo of a different resource type.
	ERTSResourceType M_TargetResourceTypeAfterDropOff = ERTSResourceType::Resource_None;


	UPROPERTY()
	UMeshComponent* M_OwnerMeshComponent;


	/** @return Whether the resource is valid and if it contains resources. */
	bool GetIsTargetResourceHarvestable() const;
	/** @return Whether the target drop off is valid, active, and if it has non-zero capacity left for the target resource. */
	bool GetHasTargetDropOffCapacity() const;

	// Maps each supported resource type to a cargoslot which has a current amount, max capacity and the resource type.
	TMap<ERTSResourceType, FHarvesterCargoSlot> M_CargoSpace;

	UPROPERTY()
	TObjectPtr<UGameResourceManager> M_GameResourceManager;

	float M_ResourceAcceptanceRadius;
	float M_HarvesterRadius;

	FVector GetHarvesterLocation() const;

	// How long it takes to harvest.
	float M_HarvestingTime;

	UPROPERTY()
	TWeakObjectPtr<UResourceDropOff> M_TargetDropOff;

	TWeakInterfacePtr<ICommands> M_CommandOwner;
	TWeakInterfacePtr<IHarvesterInterface> M_IOwnerHarvesterInterface;


	// Returns whether the harvester has no more cargo space left for the provided resource type.
	bool GetIsCargoFull(const ERTSResourceType ResourceType) const;

	/**
	 * @brief Adds the resources to the cargo space of the harvester.
	 * @param Amount How much of the resource was harvested.
	 * @param ResourceType The type of resource harvested.
	 * @return Whether the cargo is full after adding, also true if the resource type is not in the cargo space.
	 */
	bool AddHarvestedResourcesToCargo(int32 Amount, ERTSResourceType ResourceType);

	/** @return Cargo space max - cargo space current amount for target resource */
	int32 SpaceLeftForTargetResource();

	void RotateTowardsTargetLocation(const FVector& TargetLocation);

	// Sets both the target resource as well as the target resource type to zero.
	void ResetHarvesterTargets();

	/** @return the percentage of space */
	int32 GetPercentageCapacityFilled(ERTSResourceType ResourceType) const;
	
	bool CheckHasOtherCargo(TWeakObjectPtr<UResourceComponent> NewTargetResource);

	// This is set after we obtained the target location for harvesting; we keep track of this to
	// deregister this point from the resource once this harvester is done harvesting and another harvester can take that
	// stop instead.
	FHarvestLocation M_OccupiedHarvestingLocation;


	const TArray<FVector> DebugOffsets = {
		FVector(0, 0, 0), FVector(0, 0, 75), FVector(0, 0, 150), FVector(0, 0, 225),
		FVector(0, 0, 300), FVector(0, 0, 375)
	}; // FVector(0,0,450), FVector(0,0,525), FVector(0,0,600), FVector(0,0,675};
	mutable int32 DebugOffsetIndex = 0;
	mutable int32 AmountDebugs = 0;

	inline void HarvestDebug(FString Message, FColor Color) const
	{
		if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
		{
			Message = FString::FromInt(AmountDebugs) + " " + Message;
			DrawDebugString(GetWorld(), GetHarvesterLocation() + FVector(0, 0, 400) + DebugOffsets[DebugOffsetIndex],
			                Message, nullptr, Color, 15.f,
			                false);
			DebugOffsetIndex = (DebugOffsetIndex + 1) % DebugOffsets.Num();
			++AmountDebugs;
		}
	}

	// Reset the occupied harvesting location on the target resource.
	void ResetHarvesterLocation();

	void OnCargoNotFullAfterHarvesting();

	void SetupPlayerResourceManagerReference(const UWorld* World);
	void SetupGameResourceManagerReference(const UWorld* World);

	TArray<TWeakObjectPtr<UResourceComponent>> M_NotReachableResources;

	void OnHarvestedAddReturnCargoAbility() const;
	void OnCargoEmptyRemoveReturnCargoAbility()const;

	// Used to check very so many seconds to see if the harvester can return cargo or harvest resources
	// after it became idle.
	FTimerHandle M_CheckHarvesterIdleTimer;

	// Will try to find resources of the previously targeted resource type if possible or execute return cargo
	// if the harvester does have cargo.
	void CheckHarvesterIdle();
	bool GetIsHarvesterIdle() const;
	void IdleHarvester_AsyncFindTargetResource();
	// If a valid resource was found and the harvester is still idle then we issue a command through the
	// ICommands interface to start harvesting resources again starting with the found resource.
	void AsyncOnReceiveResourceForIdle(
		TArray<TWeakObjectPtr<UResourceComponent>> Resources) const;

	// ---------- NEW: Teleport guards + blacklists ----------
	/** @brief Resource goals we already teleported towards once (per 30s window). */
	TSet<TWeakObjectPtr<UResourceComponent>> M_TeleportedOnce_Resource;
	/** @brief DropOff goals we already teleported towards once (per 30s window). */
	TSet<TWeakObjectPtr<UResourceDropOff>>   M_TeleportedOnce_DropOff;

	/** @brief Resources we must not pick again until blacklists are cleared. */
	TSet<TWeakObjectPtr<UResourceComponent>> M_ResourceBlacklist;
	/** @brief DropOffs we must not pick again until blacklists are cleared. */
	TSet<TWeakObjectPtr<UResourceDropOff>>   M_DropOffBlacklist;

	FTimerHandle M_BlacklistClearTimer;

	/**
	 * @brief Clear both blacklists and the "teleported once" sets.
	 * Runs every 30s.
	 */
	void ClearHarvestBlacklists();

	/**
	 * @brief Consume the teleport allowance for the current goal, or blacklist if we already teleported once.
	 * @param ActionGroup The context (resource vs drop-off).
	 * @return true  -> teleport is allowed now (first and only time for this goal in the current window).
	 *         false -> goal was blacklisted (do not teleport; pick a new goal).
	 */
	bool ConsumeTeleportAllowanceOrBlacklist(const EHarvesterAIAction ActionGroup);

	void OnNoDropOffsFound_DisplayMessage();

	// keeps track of when the last message was so we do not spam.
	float M_LastNoDropOffMessageTime = 0.f;


	void BeginPlay_SetupAnimatedTextWidgetPoolManager();
	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextWidgetPoolManager;
};
