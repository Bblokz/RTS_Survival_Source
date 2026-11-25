// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameResourceManager.generated.h"

class UPlayerResourceManager;
class FGetAsyncResource;
enum class ERTSResourceType : uint8;
class RTS_SURVIVAL_API UResourceComponent;
class RTS_SURVIVAL_API UResourceDropOff;

/** A registered drop-off and the player it was registered for; harvesters only find drop-offs of their own player. */
USTRUCT()
struct FRegisteredResourceDropOff
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UResourceDropOff> DropOff;

	// Owner at registration. Only drop-offs of player 1 are known to the human's UPlayerResourceManager.
	int8 OwningPlayer = 0;
};

/**
 * @brief Lives on the game state; every resource field and drop-off of any player registers here, and harvesters ask
 * it (on the async thread) for the closest field of a type and the closest drop-off of their own player.
 */
UCLASS()
class RTS_SURVIVAL_API UGameResourceManager : public UActorComponent
{
	GENERATED_BODY()
	UGameResourceManager(const FObjectInitializer& ObjectInitializer);

public:
	/**
	 * @brief Either adds or removes the provided resource from the map resources.
	 * @param Resource The resource to add or remove.
	 * @param bRegister Whether to add or remove the resource.
	 */
	void RegisterMapResource(
		const TObjectPtr<UResourceComponent>& Resource,
		bool bRegister);

	/**	
	 * @brief Either adds or removes the provided resource drop off from the map resources.
	 * Drop-offs of every player register; only those of player 1 also count for the human's resource storage.
	 * @param ResourceDropOff The resource drop off to add or remove.
	 * @param bRegister Whether to add or remove the resource drop off.
	 * @param OwningPlayer The drop-off's owner now; when removing, the human's storage only changes if this and the
	 * owner it registered with are both 1.
	 */
	void RegisterResourceDropOff(
		const TWeakObjectPtr<UResourceDropOff>& ResourceDropOff,
		bool bRegister, const int8 OwningPlayer);

	/** 
	 * @brief Copies the current drop-offs of all players into an array and returns it. 
	 *  ONLY USE ON GAME THREAD
	 */
	TArray<TWeakObjectPtr<UResourceDropOff>> GetCopyOfResourceDropOffs() const;

	/** @return The drop-offs registered for this player. ONLY USE ON GAME THREAD. */
	TArray<TWeakObjectPtr<UResourceDropOff>> GetCopyOfResourceDropOffsOfPlayer(int8 OwningPlayer) const;

	TArray<TWeakObjectPtr<UResourceComponent>> GetCopyOfMapResources() const;

	void AsyncRequestClosestDropOffs(
		const FVector& HarvesterLocation,
        		int32 NumDropOffs,
        		uint8 OwningPlayer,
        		int32 DropOffAmount,
        		ERTSResourceType ResourceType,
		const TFunction<void(const TArray<TWeakObjectPtr<UResourceDropOff>>&)>
		& Callback
        	);

	void AsyncRequestClosestResource(
		const FVector& HarvesterLocation,
		const int32 NumResources,
		ERTSResourceType ResourceType,
		const TFunction<void(const TArray<TWeakObjectPtr<UResourceComponent>>&)>
		& Callback
		);

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	

private:

	// -------------- ASYNC RESOURCE FINDING ---------------

	// Setup the thread and pointer to it.
	void InitAsyncResourceThread();
	// Clean up the thread.
	void ShutDownAsyncResourceThread();

	void UpdateDropOffDataOnAsyncThread();

	void UpdateResourcesDataAsyncThread();

	// Pointer to the async resource thread that handles GetResource and GetDropOff requests.
	FGetAsyncResource* M_GetAsyncResourceThread;

	FTimerHandle M_UpdateDropOffDataHandle;
	FTimerHandle M_UpdateResourcesDataHandle;
	FTimerHandle M_CleanInvalidPointersInterval;

	float M_UpdateDropOffDataInterval;
	float M_UpdateResourcesDataInterval;
	
	
	// Contains all the components of units (of any player) that are able to have resources dropped off.
	UPROPERTY()
	TArray<FRegisteredResourceDropOff> M_ResourceDropOffs;

	int32 FindRegisteredDropOffIndex(const TWeakObjectPtr<UResourceDropOff>& DropOff) const;

	UPROPERTY()
	TArray<TWeakObjectPtr<UResourceComponent>> M_MapResources;

	// Keeps track of the resources the player has and also needs the dropoffs as it uses them to visual change
	// Where the resources are stored.
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;

	/** @return Whether the playerResourceManager is valid, if not an error message will be pushed. */
	bool GetIsValidPlayerResourceManager();

	void RegisterDropOffWithPlayerResourceManager(const bool bRegister, const TWeakObjectPtr<UResourceDropOff> DropOff);

	void CleanUpInvalidResourcesAndDropOffs();

};
