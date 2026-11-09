#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AsyncSpawnMissionVehicle.generated.h"

class ATankMaster;
class UMissionBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionVehicleSpawnResult, ATankMaster*, SpawnedTank);

/**
 * @brief Gives mission Blueprints one node that spawns an exact vehicle class (any variant, whatever the player
 * picked) and fires On Spawned with the tank where the call is wired, so no ID switch or bound event is needed.
 * On Spawned fires the tick after the spawn, when orders on the tank take, like the mission command queue spawn.
 */
UCLASS()
class RTS_SURVIVAL_API UAsyncSpawnMissionVehicle : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Fires once the tank is spawned, rotated and ready for orders.
	UPROPERTY(BlueprintAssignable)
	FOnMissionVehicleSpawnResult OnSpawned;

	// Fires when the class is empty, could not load or did not spawn; SpawnedTank is then null.
	UPROPERTY(BlueprintAssignable)
	FOnMissionVehicleSpawnResult OnFailed;

	/**
	 * @brief Spawns this exact vehicle Blueprint asynchronously; pick the variant BP itself to get that variant.
	 * @param Mission The mission whose event graph the node is in; filled in as self.
	 * @param VehicleClass The vehicle Blueprint to spawn, e.g. a variant child of a base tank.
	 * @param SpawnLocation World location of the spawn.
	 * @param SpawnRotation Rotation applied on spawn and again once spawned.
	 * @return The action; its output pins fire later.
	 * @note Parameters are by value so the pins keep inline defaults; const& pins would have to be wired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Spawn",
		meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "Mission", HidePin = "Mission",
			DisplayName = "Spawn Vehicle Variant"))
	static UAsyncSpawnMissionVehicle* SpawnVehicleVariant(
		UMissionBase* Mission,
		const TSoftClassPtr<ATankMaster> VehicleClass,
		const FVector SpawnLocation,
		const FRotator SpawnRotation);

	virtual void Activate() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UMissionBase> M_Mission;

	UPROPERTY()
	TSoftClassPtr<ATankMaster> M_VehicleClass;

	UPROPERTY()
	FVector M_SpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator M_SpawnRotation = FRotator::ZeroRotator;

	// Held between the spawn callback and the next-tick broadcast; null means the spawn failed.
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_SpawnedTank;

	// The spawner may call back synchronously, also when the request fails; this keeps the result to one broadcast.
	bool bM_HasResult = false;

	bool StartAsyncSpawn();
	void OnVehicleActorSpawned(AActor* SpawnedActor);
	void FinishWithResult(ATankMaster* SpawnedTank);
	void FinishAsFailed(const FString& Reason);
	void BroadcastResult();
	FString GetVehicleClassName() const;
	bool GetIsValidMission() const;
};
