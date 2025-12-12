// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AsyncBxpBatchRequest/AsyncBxpBatchRequest.h"
#include "AsyncBxpLoadingType/AsyncBxpLoadingType.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "RTSAsyncSpawner.generated.h"

class UTrainerComponent;
class IBuildingExpansionOwner;
class ACPPController;
enum class EBuildingExpansionType : uint8;
class ABuildingExpansion;
class AActor;


USTRUCT()
struct FAsyncOptionQueueItem
{
	GENERATED_BODY()

	FTrainingOption TrainingOption;
	TWeakObjectPtr<UTrainerComponent> Trainer;
};

// Denotes what type of request is being made.
UENUM()
enum class EAsyncRequestType : uint8
{
	AReq_None,
	AReq_Bxp,
	AReq_Training
};

/**
 * Simple setup row for Nomadic subtype → unit class.
 */
USTRUCT(BlueprintType)
struct FNomadicTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic")
	ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/**
 * Simple setup row for Tank subtype → unit class.
 */
USTRUCT(BlueprintType)
struct FTankTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Tank")
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Tank")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/**
 * Simple setup row for Squad subtype → unit class.
 */
USTRUCT(BlueprintType)
struct FSquadTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Squad")
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Squad")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};

/**
 * Simple setup row for Aircraft subtype → unit class.
 */
USTRUCT(BlueprintType)
struct FAircraftTrainingOptionSetup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft")
	EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft")
	TSoftClassPtr<AActor> UnitClass = nullptr;
};


UCLASS()
class RTS_SURVIVAL_API ARTSAsyncSpawner : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	ARTSAsyncSpawner(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Cancels the active load request if there is one.
	 * @param RequestType specifies the type of request to cancel.
	 * @return Whether there was an active load request to cancel.
	 */
	bool CancelActiveLoadRequest(EAsyncRequestType RequestType);

	/**
	 * @brief Asynchronously spawns a building expansion of the specified type.
	 * @param BuildingExpansionTypeData The type of building expansion to spawn.
	 * @param BuildingExpansionOwner The owner of the building expansion.
	 * @param ExpansionSlotIndex The index of the expansion slot to spawn the expansion in.
	 * @param BxpLoadingType Whether the expansion is an unpacked expansion or not.
	 * @pre The BuildingExpansionType is set to the correct mapping in the BuildingExpansionMap.
	 * @return Whether the request was valid and the loading process was started.
	 */
	bool AsyncSpawnBuildingExpansion(
		const FBxpOptionData& BuildingExpansionTypeData,
		TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
		const int ExpansionSlotIndex,
		const EAsyncBxpLoadingType BxpLoadingType);


	/**
	 * Starts the async spwaning process for the training option.
	 * @param TrainingOption The option to spawn.
	 * @param TrainerComponent The trainer that owns the option.
	 * @return Whether the request could be made successfully.
	 */
	bool AsyncSpawnTrainingOption(
		FTrainingOption TrainingOption,
		const TWeakObjectPtr<UTrainerComponent> TrainerComponent);

	/**
     * @brief Asynchronously spawns a training option at the specified location.
     * @param TrainingOption The training option to spawn.
     * @param Location The location where the unit should be spawned.
     * @param CallbackOwner The owner of the callback function.
     * @param SpawnID
     * @param OnSpawnedCallback The callback function to call when the unit is spawned.
     * @return Whether the request could be made successfully.
     */
	bool AsyncSpawnOptionAtLocation(
		const FTrainingOption TrainingOption,
		const FVector& Location,
		TWeakObjectPtr<UObject> CallbackOwner,
		const int32 SpawnID,
		TFunction<void(const FTrainingOption&, AActor* SpawnedActor, const int32 ID)> OnSpawnedCallback);


	UFUNCTION(BlueprintCallable, Category = "ReferenceCasts")
	void InitRTSAsyncSpawner(ACPPController* PlayerController);

	/**
	 * Syncronously gets the preview mesh of the building expansion type.
	 * @param BuildingExpansionType The type of building expansion to get the preview mesh of.
	 * @return The static mesh pointer of the preview mesh.
	 * @pre The ExpansionType mapping is set to the correct preview mesh.
	 */
	UStaticMesh* SyncGetBuildingExpansionPreviewMesh(EBuildingExpansionType BuildingExpansionType);

	void CancelLoadRequestForTrainer(const TWeakObjectPtr<UTrainerComponent> TrainerComponent);

    /** 
     * Begins loading (and immediately spawning) a whole batch of expansions.
     * Once all are done, calls OnInstantPlacementBxpsSpawned on the owner.
     */
	void AsyncBatchLoadInstantPlaceExpansions(
		TArray<EBuildingExpansionType> TypesToLoad,
		TArray<int32> BxpItemIndices, const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner, TArray<
			FBxpOptionData> BxpTypesAndConstructionRules);


protected:
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;


	// Associates the building expansion type with the class to spawn using a hashmap.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning")
	TMap<EBuildingExpansionType, TSoftClassPtr<ABuildingExpansion>> BuildingExpansionMap;

	//--------------------------- TRAINING OPTION SETUP (Blueprint‐facing) --------------------

	// Tanks – split by weight class and faction for better overview.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Light|Rus")
	TArray<FTankTrainingOptionSetup> SetupLightRusVehicles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Light|Ger")
	TArray<FTankTrainingOptionSetup> SetupLightGerVehicles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Medium|Rus")
	TArray<FTankTrainingOptionSetup> SetupMediumRusVehicles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Medium|Ger")
	TArray<FTankTrainingOptionSetup> SetupMediumGerVehicles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Heavy|Rus")
	TArray<FTankTrainingOptionSetup> SetupHeavyRusVehicles;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Tank|Heavy|Ger")
	TArray<FTankTrainingOptionSetup> SetupHeavyGerVehicles;

	// Infantry squads – split by faction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Squad|Rus")
	TArray<FSquadTrainingOptionSetup> SetupRusSquads;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Squad|Ger")
	TArray<FSquadTrainingOptionSetup> SetupGerSquads;

	// Nomadics – split by faction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic|Rus")
	TArray<FNomadicTrainingOptionSetup> SetupRusNomadics;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Nomadic|Ger")
	TArray<FNomadicTrainingOptionSetup> SetupGerNomadics;

	// Aircraft – split by faction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft|Rus")
	TArray<FAircraftTrainingOptionSetup> SetupRusAircraft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning|Training|Aircraft|Ger")
	TArray<FAircraftTrainingOptionSetup> SetupGerAircraft;


	// Associates the building expansion type with the associated preview mesh using a hashmap.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Async Spawning")
	TMap<EBuildingExpansionType, UStaticMesh*> BxpPreviewMeshMap;

private:
	// Associates the concrete training option with the class to spawn using a hashmap.
	TMap<FTrainingOption, TSoftClassPtr<AActor>> M_TrainingOptionMap;

	// Used to load assets asynchronously.
	FStreamableManager M_StreamableManager;

	UPROPERTY()
	ACPPController* M_PlayerController;

	bool EnsurePlayerControllerIsValid() const;

	// Rebuilds M_TrainingOptionMap from all blueprint setup arrays.
	void SetupTrainingOptionsFromConfig();

	// Helper per unit type.
	void AddTrainingOptionsForTankArray(const TArray<FTankTrainingOptionSetup>& SetupArray);
	void AddTrainingOptionsForSquadArray(const TArray<FSquadTrainingOptionSetup>& SetupArray);
	void AddTrainingOptionsForNomadicArray(const TArray<FNomadicTrainingOptionSetup>& SetupArray);
	void AddTrainingOptionsForAircraftArray(const TArray<FAircraftTrainingOptionSetup>& SetupArray);

	/**
	 * @brief Adds a mapping entry if valid and unique.
	 *        Logs and skips invalid / duplicate mappings.
	 */
	void AddTrainingOptionMapping(
		const FTrainingOption& TrainingOption,
		const TSoftClassPtr<AActor>& UnitClass);


	// StreamableHandle that is currently loading bxp assets.
	// invalidating this handle will cancel the loading process.
	TSharedPtr<FStreamableHandle> M_CurrentBxpLoadingHandle;

	/** @return Whether the bxp owner is valid and the bxp type is not none. */
	bool IsBxpLoadRequestValid(
		EBuildingExpansionType BuildingExpansionType,
		const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner) const;

	/**
	 * @brief Handles the loaded hard reference to a bxp.
	 * will attempt to spawn the bxp and propagate to the player controller using OnBuildingExpansionSpawned.
	 * @param AssetPath Path to the asset to load.
	 * @param BuildingExpansionTypeData The type of building expansion to spawn.
	 * @param BuildingExpansionOwner The owner of the building expansion.
	 * @param ExpansionSlotIndex The index of the expansion slot to spawn the expansion in.
	 * @param BxpLoadingType The expansion is an unpacked expansion or not.
	 * @note Makes no callback when the assset fails to spawn.
	 */
	void OnAsyncBxpLoadComplete(
		FSoftObjectPath AssetPath,
		const FBxpOptionData BuildingExpansionTypeData,
		TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
		const int ExpansionSlotIndex,
		const EAsyncBxpLoadingType BxpLoadingType);

	void OnFailedAssetOrClassLoadBxp(
		const UObject* LoadedAsset,
		const UClass* AssetClass,
		const EBuildingExpansionType ExpansionType) const;

	/** @brief Notifies the playercontroller that the building expansion was spawned. */
	void OnBuildingExpansionSpawned(
		AActor* SpawnedActor,
		TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
		const FBxpOptionData& BxpConstructionRulesAndType,
		const int ExpansionSlotIndex,
		const EAsyncBxpLoadingType BxpLoadingType) const;

	/**
	 * @brief Called after the loading of the training option is complete for AsyncSpawnOptionAtLocation.
	 * @param AssetPath The path of the loaded asset.
	 * @param TrainingOption The option that was loaded.
	 * @param Location The location where the unit should be spawned.
	 * @param ID
	 */
	void OnAsyncSpawnOptionAtLocationComplete(
		const FSoftObjectPath& AssetPath,
		const FTrainingOption TrainingOption,
		const FVector& Location, const int32 ID);

	bool IsTrainerNotInQueue(const TWeakObjectPtr<UTrainerComponent> TrainerComponent);

	void LoadNextTrainingOptionInQueue();

	/**
	 * Checks the validity of the trainer and the option provided
	 * @param TrainingOption The option to spawn.
	 * @param TrainerComponent The trainer that owns the option.
	 * @return Wheter it is safe to spawn the option.
	 */
	bool IsSpawnOptionValid(const FTrainingOption TrainingOption,
	                        const TWeakObjectPtr<UTrainerComponent> TrainerComponent) const;


	/**
	 * @brief Called after the loading of the training option is complete.
	 * Invalides the M_CurrentTrainingOptionLoadingHandle.
	 * @param AssetPath The path of the loaded asset.
	 * @param TrainingOption The option that was loaded.
	 * @param TrainerComponent The owner of the training option.
	 */
	void HandleAsyncTrainingLoadComplete(
		const FSoftObjectPath AssetPath,
		FTrainingOption TrainingOption,
		TWeakObjectPtr<UTrainerComponent> TrainerComponent);

	/**
	 * @brief Notifies the trainer that the trained unit is spawned.
	 * Also hides and disables collision on the spawned actor.
	 * The spawned actor is propagated to the trainer.
	 * @param SpawnedActor The actor that was spawned.
	 * @param TrainerComponent The trainer that trained this actor.
	 * @param TrainingOption The option that was trained.
	 * @pre The actor is valid.
	 */
	void OnTrainingOptionSpawned(
		AActor* SpawnedActor,
		TWeakObjectPtr<UTrainerComponent> TrainerComponent,
		const FTrainingOption& TrainingOption) const;


	// StreamableHandle that is currently loading training option assets.
	TSharedPtr<FStreamableHandle> M_CurrentTrainingOptionLoadingHandle;

	TQueue<FAsyncOptionQueueItem> M_TrainingQueue;

	AActor* AttemptSpawnAsset(const FSoftObjectPath& AssetPath) const;


	// Callbacks used to the player profile loader to indicate when a unit has been spawned.
	TMap<FSoftObjectPath, TPair<TWeakObjectPtr<UObject>,
	                            TFunction<void(const FTrainingOption&, AActor* SpawnedActor, const int32 ID)>>>
	M_SpawnCallbacks;



	//--------------------------- BATCH BXP LOADING ------------------------------------
	

    // Counter for assigning unique IDs to each batch request.
    int32 M_NextInstantPlacementRequestId = 0;

    // All outstanding “instant-placement” requests, keyed by that ID.
    TMap<int32, FInstantPlacementBxpBatch> M_InstantPlacementRequests = {};

    // Called when a batch’s async load finishes.
    void OnBatchBxpLoadComplete(int32 RequestId);

	// Extracts and removes the batch struct for a given RequestId.
    FInstantPlacementBxpBatch ExtractAndRemoveBatch(int32 RequestId);
    
    // Attempts to resolve the class & spawn one Bxp, or returns nullptr.
    // Logs any errors along the way.
    ABuildingExpansion* BatchBxp_TryResolveAndSpawnBxp(EBuildingExpansionType Type) const;
    
    // Helper to look up the soft‐class and resolve it to UClass*
    UClass* ResolveExpansionClass(EBuildingExpansionType Type) const;
    
    // Helper to actually SpawnActor<AActor> and cast to ABuildingExpansion.
    ABuildingExpansion* BatchBxp_SpawnExpansionActor(UClass* Cls) const;
};
