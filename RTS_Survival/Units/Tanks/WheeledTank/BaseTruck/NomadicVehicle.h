// Copyright Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "AINomadicVehicle.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Player/ConstructionPreview/StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/Buildings/BuildingAttachments/BuildingAttachments.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"
#include "RTS_Survival/GameUI/TrainingUI/Interface/Trainer.h"
#include "RTS_Survival/RTSComponents/SelectionDecalSettings/SelectionDecalSettings.h"
// For the resource storage visualisation TMaps.
#include "ResourceConversionHelper/FResourceConversionHelper.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageLevel.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "TrainingPreviewHelpers/TrainingPreviewHelper.h"

#include "NomadicVehicle.generated.h"

class UAircraftOwnerComp;
class UResourceDropOff;
class UBoxComponent;
class UBuildRadiusComp;
class UEnergyComp;
class RTS_SURVIVAL_API UBuildingExpansionOwnerComp;
// Forward declarations.
class RTS_SURVIVAL_API UTimeProgressBarWidget;

DECLARE_MULTICAST_DELEGATE(FOnNomadConvertToBuilding);
DECLARE_MULTICAST_DELEGATE(FOnNomadConvertToVehicle);


UENUM()
enum class ENomadStatus
{
	Truck,
	Building,
	// Rotating to align with the building location.
	CreatingBuildingRotating,
	// Animating the truck expanding.
	CreatingBuildingTruckAnim,
	// Animating the building mesh construction.
	CreatingBuildingMeshAnim,
	CreatingTruck
};

USTRUCT(Blueprintable)
struct FNomadicCargoSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxSupportedSquads = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector CargoPositionsOffset = FVector::ZeroVector;
};


/**
 * @brief A vehicle that can deploy into a building and pack it up back into a vehicle.
 * @note Set "Allow CPUAccess" in the static mesh editor for the building mesh to allow for smoke effects.
 * @note ---------------------------------------------------------------
 * @note Follow the conversion path:
 * @note 1) StartBuildingConstruction BP: OnMoveTruckToBuildingLocationBP
 * @note The truck is now moved to the building location and the chasis teleported
 * @note 2) OnFinishedStandaloneRotation
 * @note The truck is now aligned with the building location.
 * @note 3) BP: BeginBuildingTruckAnimationMontage
 * @note This starts the montage animation in the blueprint.
 * @note 4) OnTruckMontageFinished BP: BPOnStartMeshAnimation
 * @note Now starts the material animation using timers.
 * @note 5) FinishedConvertingToBuilding.
 * Creates building attachments and opens command queue again.
 * @note ---------------------------------------------------------------
 * @note Keeps the EAbilityID::IdCreateBuilding in the queue during the whole process.
 * @note Sets truck to StopCommandsUntilCancel after rotating in place for the corrrect building angle.
 * @note the building process is always cancelled with the terminate command that
 * @note is associated with the EAbilityID::IdCreateBuilding.
 * @note ---------------------------------------------------------------
 * @note for building acceptance radius see AAINomadicVehicle::ConstructionAcceptanceRad.
 * @note ---------------------------------------------------------------
 * @note For training make sure that the bp overwrites the gettraining component function.
 * @note ---------------------------------------------------------------
 * @note Energy, ResourceDropOff, and Radii Components are automatically set on PostInitializeComponents.
 * @note ---------------------------------------------------------------
 * @note ---- On Resource Storage ----
 * @note The resource visualisation is Restored once the vehicle finished converting into a building.
 * @note The resource visualisation is Destroyed once the vehicle finished converting into a truck.
 */
UCLASS()
class RTS_SURVIVAL_API ANomadicVehicle : public ATrackedTankMaster, public IBuildingExpansionOwner,
                                         public ITrainer
{
	GENERATED_BODY()

	friend struct FTrainingPreviewHelper;
	friend struct FResourceConversionHelper;

public:
	ANomadicVehicle(const FObjectInitializer& ObjectInitializer);

	// Delegate called when converting to building.
	FOnNomadConvertToBuilding OnNomadConvertToBuilding;
	// Delegate called when converting to vehicle.
	FOnNomadConvertToVehicle OnNomadConvertToVehicle;

	UEnergyComp* GetEnergyComp() const;
	// Returns the building radius if available, otherwise 0.
	float GetBuildingRadius() const;

	/**
	 * @brief Starts the building construction process.
	 * 
	 * This function initiates the process of constructing a building by rotating the vehicle to the 
	 * correct orientation and preparing it for the building phase.
	 * 
	 * @note This function is the first step in the building construction sequence.
	 */
	void StartBuildingConstruction();

	virtual float GetBxpExpansionRange() const override;
	virtual FVector GetBxpOwnerLocation() const override;

	/** @return Whether this truck is movable and not a stationary base. */
	inline bool GetIsInTruckMode() const { return M_NomadStatus == ENomadStatus::Truck; }

	inline ENomadStatus GetNomadicStatus() const { return M_NomadStatus; }
	inline UBuildRadiusComp* GetBuildRadiusComp() const { return M_RadiusComp.Get(); }

	/** @return The preview mesh of the building that this unit constructs. */
	inline UStaticMesh* GetPreviewMesh() const { return M_PreviewMesh; }

	void SetStaticPreviewMesh(AStaticPreviewMesh* NewStaticPreviewMesh);

	float GetTotalConstructionTime() const { return M_ConstructionMontageTime + M_MeshAnimationTime; }

	float GetTotalConvertToVehicleTime() const { return M_ConvertToVehicleTime; }

	/**
	 * @brief overwrite from IBuildingExpansionOwner.
	 * @return Whether the building is in a state in which expanding is possible.
	 */
	virtual bool IsBuildingAbleToExpand() const override final;

	/** @return the Trainer component of this nomadic vehicle.
	 * Allowed to return null to indicate that this vehicle cannot train.
	 */
	virtual UTrainerComponent* GetTrainerComponent() override;

	UResourceDropOff* GetResourceDropOff() const { return M_ResourceDropOff; }

	void SetAINomadicVehicle(AAINomadicVehicle* NewAINomadicVehicle);

	// from IBuidling Expansion owner.
	virtual UStaticMeshComponent* GetAttachToMeshComponent() const override final;
	virtual TArray<UStaticMeshSocket*> GetFreeSocketList(const int32 ExpansionSlotToIgnore) const override final;

	virtual float GetUnitRepairRadius() override;

	inline UAircraftOwnerComp* GetAircraftOwnerComp() const { return M_AircraftOwnerComp; }

protected:
	virtual void UnitDies(const ERTSDeathType DeathType) override;
	virtual EAnnouncerVoiceLineType OverrideAnnouncerDeathVoiceLine(const EAnnouncerVoiceLineType OriginalLine) const override final;
	/**
	 * @brief Initializes the NomadicVehicle.
	 * @param NewPreviewMesh The mesh used to choose the building location.
	 * @param NewBuildingMesh The actual high poly mesh of the building.
	 * @param NewConstructionAnimationMaterial The material used to animate the building construction.
	 * @param SmokeSystems The smoke systems to spawn when the building is being constructed.
	 * @param NewConstructionFrames The amount of frames in the construction animation.
	 * @param NewConstructionMontageTime The amount of time spend on montage.
	 * @param NewMeshAnimationTime The amount of time spend on mesh animation.
	 * @param NewConvertToVehicleTime How long it takes to convert from a building back to a vehicle.
	 * @param NewAmountSmokesCovertToVehicle How many smokes to spawn when converting to a vehicle.
	 * @param NewAttachmentsToSpawn The attachments to spawn once the building is constructed.
	 * @param NewNiagaraAttachmentsToSpawn The niagara attachments to spawn once the building is constructed.
	 * @param NewSoundAttachmentsToSpawn The sound attachments to spawn once the building is constructed.
	 * @param NewConversionSmokeRadius In how big of a circle surrounding the vehicle we create smokes when starting conversion.
	 * @param NewConversionProgressBar The progress bar to display the conversion progress which is a derived blueprint
	 * of which the cpp class is UTimeProgressBarWidget.
	 * @param NewTruckUIOffset The offset to apply to the truck UI elements when converting to a vehicle.
	 * @param NewSelectionDecalSettings To define decal size, offset and materials when switching between truck and building.
	 * @param NewExpansionBuildRadius
	 * @param NewBuildingMeshComponent
	 * @param NewBuildingNavModifiers
	 * @note Truck selection materials are set on the selection component.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts")
	void InitNomadicVehicle(UStaticMesh* NewPreviewMesh, UStaticMesh* NewBuildingMesh,
	                        UMaterialInstance* NewConstructionAnimationMaterial,
	                        TArray<UNiagaraSystem*> SmokeSystems,
	                        const float NewConstructionFrames,
	                        const float NewConstructionMontageTime,
	                        const float NewMeshAnimationTime,
	                        const float NewConvertToVehicleTime,
	                        const int NewAmountSmokesCovertToVehicle,
	                        TArray<FBuildingAttachment> NewAttachmentsToSpawn,
	                        TArray<FBuildingNiagaraAttachment> NewNiagaraAttachmentsToSpawn,
	                        TArray<FBuildingSoundAttachment> NewSoundAttachmentsToSpawn,
	                        TArray<FBuildingNavModifierAttachment> NewBuildingNavModifiers,
	                        const float NewConversionSmokeRadius,
	                        UTimeProgressBarWidget* NewConversionProgressBar,
	                        const FVector NewTruckUIOffset,
	                        const FSelectionDecalSettings NewSelectionDecalSettings,
	                        const float NewExpansionBuildRadius,
	                        TArray<USoundCue*> NewConstructionSounds, UStaticMeshComponent* NewBuildingMeshComponent);


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStartAsConvertedToBuilding = false;

	// Settings used on the cargo component when we setup the cargo component after converting to building.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FNomadicCargoSettings CargoSettings;

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupMountCollision(UMeshComponent* MountMesh);

	// From IBuildingExpansionOwner.
	virtual UBuildingExpansionOwnerComp& GetBuildingExpansionData() const override final;
	virtual void OnBuildingExpansionDestroyed(ABuildingExpansion* BuildingExpansion, const bool bDestroyedPackedExpansion) override;

	virtual void NoQueue_ExecuteSetResourceConversionEnabled(const bool bEnabled) override;
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;
	virtual void OnUnitIdleAndNoNewCommands() override final;
	virtual void TerminateMoveCommand() override;




	// Static Mesh for the building representation
	UPROPERTY()
	UStaticMeshComponent* BuildingMeshComponent;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UStaticMeshComponent* GetBuildingMeshComponent() const { return BuildingMeshComponent; }

	/**
	 * @brief Creates a building with the provided rotation at the provided location.
	 * @param BuildingLocation The location to place the building.
	 * @param BuildingRotation The Rotation to place the building with.
	 */
	virtual void ExecuteCreateBuildingCommand(
		const FVector BuildingLocation,
		const FRotator BuildingRotation) override final;


	/** @brief Stops the building creation command and associated logic. */
	virtual void TerminateCreateBuildingCommand() override final;

	virtual void ExecuteConvertToVehicleCommand() override final;

	virtual void TerminateConvertToVehicleCommand() override final;

	/**
	 * @brief Executes the create building command in the blueprint by activating BT.
	 * @param BuildingLocation The location to place the building.
	 * @param BuildingRotation The Rotation to place the building with.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void CreateBuildingAtLocationBP(
		const FVector BuildingLocation,
		const FRotator BuildingRotation);

	/**
	 * @brief Called when the truck has completed moving to the building location and is now getting ready to rotate towards the
	 * direction of the building. (before montage is played)
	 * @param BuildingLocation The location where the building will be placed.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void OnMoveTruckToBuildingLocationBP(const FVector BuildingLocation);

	/** @brief Signals the start of building construction in the blueprint. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "BuildingInConstruction")
	void BeginBuildingTruckAnimationMontage();

	// Stops the montage of the truck expanding.
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "BuildingInConstruction")
	void BPStopTruckAnimationMontage();

	/**
	 * @brief Called when the truck montage animation is finished.
	 * 
	 * This function is triggered after the truck's building animation montage is complete. It hides the 
	 * truck mesh, shows the building mesh, and starts the building material animation.
	 * 
	 * @note This function is the third step in the building construction sequence.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "BuildingInConstruction")
	void OnTruckMontageFinished();

	// Called When the truck starts the material animation
	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingInConstruction")
	void BPOnStartMeshAnimation();

	/**
	 * @brief Handles actions after standalone rotation is completed.
	 * 
	 * This function is called when the vehicle has finished rotating to the correct orientation for building. 
	 * It then starts the animation and progress bar for building construction.
	 * 
	 * @note This function is the second step in the building construction sequence.
	 */
	virtual void OnFinishedStandaloneRotation() override final;

	/**
	 * @brief Handles logic when the mesh animation was stopped due to cancel building.
	 * Sets truck attachments to visible if needed.
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Truck")
	void OnMeshAnimationCancelled();

	// The play rate on the montage.
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure, Category = "BuildingInConstruction")
	float GetConstructionTimeInPlayRate() const;

	// For any bp specific logic like attached crystals.
	UFUNCTION(BlueprintImplementableEvent, Category = "ConvertToBuilding")
	void BP_OnFinishedConvertingToBuilding();

	// For any bp specific logic like attached crystals.
	UFUNCTION(BlueprintImplementableEvent, Category = "ConvertToBuilding")
	void BP_OnStartedConvertingToVehicle();

	// For any bp specific logic like attached crystals.
	UFUNCTION(BlueprintImplementableEvent, Category = "ConvertToBuilding")
	void BP_OnCancelledConvertingToVehicle();

	UFUNCTION(BlueprintImplementableEvent, Category="ConvertToTruck")
	void BP_OnFinishedConvertingToVehicle();

	UPROPERTY(BlueprintReadOnly)
	UBuildingExpansionOwnerComp* BuildingExpansionComponent;

	virtual void SetupChassisMeshCollision() override;

	// ------------------------------ Trainer functions ------------------------------
	/**
	 * @brief Called when the progress of training of an option is started or resumed
	 * @param StartedOption What is being trained.
	 */
	virtual void OnTrainingProgressStartedOrResumed(const FTrainingOption& StartedOption) override;
	// Called when a new option is added to the queue.
	virtual void OnTrainingItemAddedToQueue(const FTrainingOption& AddedOption) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Trainer")
	void BpOnTrainingProgressStartedOrResumed(const FTrainingOption& StartedOption);

	/**
	 * @brief Called when training of an option is completed.
	 * @param CompletedOption What is completed.
	 * @param SpawnedUnit The unit that was spawned.
	 */
	virtual void OnTrainingComplete(
		const FTrainingOption& CompletedOption,
		AActor* SpawnedUnit) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Trainer")
	void BPOnTrainingComplete(const FTrainingOption& CompletedOption);


	/**
	 * @brief Called when the training of an option is cancelled.
	 * @param CancelledOption The option that was cancelled.
	 * @param bRemovedActiveItem
	 */
	virtual void OnTrainingCancelled(const FTrainingOption& CancelledOption, const bool bRemovedActiveItem) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Trainer")
	void BPOnTrainingCancelled(const FTrainingOption& CancelledOption);

	// ------------------------------ End Trainer functions ------------------------------

	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	AAINomadicVehicle* AINomadicVehicle;

	UFUNCTION(BlueprintImplementableEvent, Category="OnSpawn")
	void BPOnRTSUnitSpawned(const bool bSetDisabled);


	// ------------------------------ Harvesting and Resources ------------------------------

	virtual void
	OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType) override;


	/**
	 * @brief Setup to enable resource storage visualization on this nomadic vehicle
	 * @param HighToLowLevelsPerResource Mapping from resource to levels of storage for that resource.
	 * @param ResourceToSocketMap Mapping from resource to the socket at wich the resource's storage needs to be placed.
	 *
	 * @note For each resource type supported HighToLowLevelsPerResource contains the array of levels from high to low
	 * for that resource, Where each level is an integer determining the percentage of resources
	 * filled at which the mesh is placed.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitResourceStorageLevels(
		TMap<ERTSResourceType, FResourceStorageLevels> HighToLowLevelsPerResource,
		TMap<ERTSResourceType, FName> ResourceToSocketMap);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupNomadicHealthData(FResistanceAndDamageReductionData TruckHealthData,
	                            FResistanceAndDamageReductionData BuildingHealthData,
	                            const float TruckMaxHealth,
	                            const float BuildingMaxHealth);
private:

	FResistanceAndDamageReductionData M_TruckHealthData;
	FResistanceAndDamageReductionData M_BuildingHealthData;
	float M_TruckMaxHealth;
	float M_BuildingMaxHealth;
	
	/** For each resource type supported this map contains the array of levels from high to low for that resource,
	*  Where each level is an integer determining the percentage of resources filled at which the mesh is placed.
	* @note The resource visualisation is Restored once the vehicle finished converting into a building.
	* @note The resource visualisation is Destroyed once the vehicle finished converting into a truck.
	*/
	TMap<ERTSResourceType, FResourceStorageLevels> M_HighToLowLevelsPerResource;

	// Maps the resource type provided to the mesh component to place the storage level for that resource.
	TMap<ERTSResourceType, TWeakObjectPtr<UStaticMeshComponent>> M_ResourceLevelComponents;

	// Provides the socket name for the resource on the Building Mesh.
	TMap<ERTSResourceType, FName> M_ResourceToSocketMap;

	
	/**
	 * @brief Updates the resource visuals for the provided resource type, May create a new mesh component to hold the resource visual.
	 * @param ResourceType The type of resource to update the visuals for.
	 * @param PercentageResourcesFilled The percentage of resources filled.
	 */
	void UpdateResourceVisualsForType(const ERTSResourceType ResourceType, const int32 PercentageResourcesFilled);
	/**
	 * @brief Sets the mesh on the resource storage visualization component.
	 * @param Mesh The mesh to use.
	 * @param ResourceType What type of resource to set the mesh for.G
	 */
	void SetResourceMeshVisual(UStaticMesh* Mesh, const ERTSResourceType ResourceType);

	/**
	 * @brief Called by SetResourceMeshVisual if the component is not valid; creates a component on the BuildingMesh
	 * for the provided type.
	 * @param ResourceType The type to create the component for.
	 */
	void CreateResourceStorageMeshComponent(const ERTSResourceType ResourceType);
	/**
	 * @brief Destroys all resource storage mesh components.
	 * @pre The resource storage visuals need to be destroyed.
	 */
	void DestroyAllResourceStorageMeshComponents();

	/**
	 * @brief Updates each resource storage visualisation component with the mesh accruing to the percentage filled.
	 * @pre The resource visualisation was destroyed and needs to be restored.
	 * @note Does not create a mesh component for a resource of which the percentage filled is 0.
	 */
	void RestoreResourceStorageVisualisation();


	// The mesh used as building preview.
	UPROPERTY()
	UStaticMesh* M_PreviewMesh;

	// The complete (high poly) mesh of the building.
	UPROPERTY()
	UStaticMesh* M_BuildingMesh;

	// Whether the vehicle is a stationary base or movable.
	UPROPERTY()
	ENomadStatus M_NomadStatus;

	// A reference to the static preview that is placed when the building location is determined.
	UPROPERTY()
	AStaticPreviewMesh* M_StaticPreviewMesh;

	// The transform of the preview mesh used to place the building.
	UPROPERTY()
	FTransform M_BuildingTransform;

	// The material used to animate the building construction.
	UPROPERTY()
	UMaterialInstance* M_ConstructionAnimationMaterial;

	// Amount of time spend on montage.
	UPROPERTY()
	float M_ConstructionMontageTime;

	// Amount of time spend on mesh animation.
	UPROPERTY()
	float M_MeshAnimationTime;

	// How many frames are in the montage.
	UPROPERTY()
	float M_ConstructionFrames;

	// How long it takes to convert from a building back to a vehicle.
	UPROPERTY()
	float M_ConvertToVehicleTime;

	// Disables those components that are immediately disabled when the truck starts converting again.
	void OnStartConvertToVehicle_HandleInstanceSpecificComponents();

	// On cancelling converting to vehicle; handle instance component logic. for training cargo, reinforcements and drop off.
	void OnCancelledConvertToVehicle_HandleInstanceSpecificComponents();

	/**
	 * @brief Disables the building component and enables the vehicle mesh.
	 */
	void OnFinishedConvertingToVehicle();

	void OnFinishedConvertingToVehicle_HandleInstanceSpecificComponents();

	UPROPERTY()
	FTimerHandle ConvertToVehicleTimerHandle;

	

	/**
	 * @brief Finishes the conversion to building process.
	 * 
	 * This function is called when the building construction is fully completed. It stops the progress bar, 
	 * enables the command queue, and creates building attachments.
	 * 
	 * @note This function is the fourth and final step in the building construction sequence.
	 */
	void OnFinishedConvertingToBuilding();

	// Renables the components that are only supposed to work when the vehicle is in building mode.
	void OnFinishedConvertToBuilding_HandleInstanceSpecificComponents();

	// Unpacks the airbase if this vehicle has aircraft logic.
	void OnFinishedConvertToBuilding_UnpackAirbase();

	// Cache for original materials of the building mesh
	// Always empty by calling ResetCachedMaterials as the index also needs to be reset!
	UPROPERTY()
	TArray<UMaterialInterface*> M_CachedOriginalMaterials;

	// Timer handle for reapplying materials
	FTimerHandle MaterialReapplyTimerHandle;

	// To save original materials of the building mesh.
	void CacheOriginalMaterials();

	/**
	 * Applies construction material to all non truck material slots.
	 * @return The amount of materials that occur in both the truck and the building and are excluded.
	 * from the construction material animation.
	 * @param bOnlyCalculateExcludedMaterials Whether to only calculate the excluded materials and do not
	 * change any materials on the building mesh.
	 */
	uint32 ApplyConstructionMaterial(const bool bOnlyCalculateExcludedMaterials) const;

	// Called by timer to reapply original materials one by one.
	void ReapplyOriginalMaterial();

	// To stop reapplying materials and finish building.
	void FinishReapplyingMaterials();

	/**
	 * Calculates the mean location of the material at the provided index.
	 * @param MaterialIndex The index of the material to calculate the mean location for.
	 * @param VertexPositions The vertex positions of the building mesh for this material.
	 * @param TransformOfBuildingMesh The transform of the building mesh.
	 * @return The mean location of the material.
	 */
	FVector CalculateMeanMaterialLocation(const int32 MaterialIndex,
	                                      const TArray<FVector3f>& VertexPositions,
	                                      const FTransform& TransformOfBuildingMesh) const;

	/**
	 * Computes the size (extent) of each mesh part associated with a material index.
	 * It's used to determine the relative scale for the smoke effects.
	 * @param VertexPositions 
	 * @return The size of the mesh part.
	 */
	FVector CalculateMeshPartSize(const TArray<FVector3f>& VertexPositions) const;

	// The current material index to reapply a material to.
	UPROPERTY()
	int32 M_MaterialIndex;

	UPROPERTY()
	TArray<FTransform> M_CreateSmokeTransforms;

	/**
	 * @brief Creates an array of smoke locations for all materials, uses async to find material means.
	 * @note Requires the final mesh of the BuildingMeshComponent to be set to "Allow CPUAccess" in the static mesh editor.
	 */
	void InitSmokeLocations();

	// Calculates random locations in the box of the building mesh to spawn smoke at.
	void SetSmokeLocationsToRandomInBox();

	UPROPERTY()
	// The smoke system(s) to spawn when the building is being constructed.
	TArray<UNiagaraSystem*> M_SmokeSystems;

	// Spawns a smoke system at the provided location.
	void CreateRandomSmokeSystemAtTransform(const FTransform& Transform) const;

	void CancelBuildingMeshAnimation();

	// Attaches all the attachments to the building mesh.
	void CreateBuildingAttachments();

	/**
	 * @brief Goes through the array of BuildingAttachments and spawns them.
	 * @pre assumes BuildingMeshComponent is valid.
	 */
	void CreateChildActorAttachments();

	/**
	 * @brief Goes through the array of Niagara attachments and spawns them.
	 * @pre Assumes BuildingMeshComponent is valid.
	 */
	void CreateNiagaraAttachments();

	/**
	 * @brief Goes through the array of sound attachments and spawns them.
	 * @pre Assumes BuildingMeshComponent is valid.
	 */
	void CreateSoundAttachments();

	// Destroys all building attachments.
	void DestroyAllBuildingAttachments();

	// The spawned attachments to destroy when the building is destroyed.
	UPROPERTY()
	TArray<AActor*> M_SpawnedAttachments;

	// Array collection of attachments to spawn once the building is constructed.
	UPROPERTY()
	TArray<FBuildingAttachment> M_AttachmentsToSpawn;

	// Array collection of niagara attachments to spawn once the building is constructed.
	UPROPERTY()
	TArray<FBuildingNiagaraAttachment> M_NiagaraAttachmentsToSpawn;

	// Array collection of sound attachments to spawn once the building is constructed.
	UPROPERTY()
	TArray<FBuildingSoundAttachment> M_SoundAttachmentsToSpawn;

	UPROPERTY()
	TArray<FBuildingNavModifierAttachment> M_NavModifierAttachmentsToSpawn;

	// Keeps track of the currently attached nav modifiers.
	UPROPERTY()
	TArray<UBoxComponent*> M_AttachedNavModifiers;

	/** Creates nav modifier attachments */
	void CreateNavModifierAttachments();

	/** Destroys nav modifier attachments */
	void DestroyNavModifierAttachments();

	// Keeps track of the currently attached niagara particle systems.
	UPROPERTY()
	TArray<UNiagaraComponent*> M_SpawnedNiagaraSystems;

	// Keeps track of the currently attached sound cues.
	UPROPERTY()
	TArray<UAudioComponent*> M_SpawnedSoundCues;

	// Also resets the class-global material index.
	void ResetCachedMaterials();

	// Creates the smoke for the vehicle conversion.
	void CreateSmokeForVehicleConversion() const;

	UPROPERTY()
	int M_AmountSmokesCovertToVehicle;

	// Instantly sets all the building materials on the building mesh to the material array saved
	// in the cache.
	void SetAllBuildingMaterialsToCache();

	// In how big of a circle surrounding the vehicle we create smokes when starting conversion.
	UPROPERTY()
	float M_ConversionSmokeRadius;

	UPROPERTY()
	UTimeProgressBarWidget* M_ConversionProgressBar;

	/**
	 * @brief Starts the mesh material animation.
	 * @param Interval The interval in seconds between updating each material slot.
	 */
	void SetAnimationTimer(const float Interval);

	/**
	 * @brief Applies local offset to the truck UI elements depending on the conversion.
	 * @param bMoveToBuildingPosition If true move the UI elements to the truck position,
	 * otherwise move them to the building position.
	 */
	void MoveTruckUIWithLocalOffsets(const bool bMoveToBuildingPosition);

	void OnConvertToBuilding_PlacePackedBxps();

	// The offset the user wants from the base of the building mesh.
	FVector M_DesiredTruckUIOffset;

	// Stores the locations of the truck UI elements.
	TPair<FVector, FVector> M_TruckUIElementLocations;

	UPROPERTY()
	FSelectionDecalSettings M_SelectionDecalSettings;

	FName M_TrainingSpawnSocketName;

	// May not be set if the vehicle does not provide a build radius.
	UPROPERTY()
	TObjectPtr<UBuildRadiusComp> M_RadiusComp;

	// May not be set if the vehicle does not consume nor provide energy in building mode. 
	UPROPERTY()
	TObjectPtr<UEnergyComp> M_EnergyComp;

	// May not be set if hte vehicle does not use squad reinforcement.
	UPROPERTY()
	TObjectPtr<UReinforcementPoint> M_ReinforcementPoint;

	// The drop off component that can be added to grandchild derived blueprints, is automatically set on
	// PostInitializeComponents.
	UPROPERTY()
	UResourceDropOff* M_ResourceDropOff;

	// May not be set if the vehicle does not consume nor provide energy in building mode. 
	UPROPERTY()
	TObjectPtr<UTrainerComponent> M_TrainerComponent;

	// May not be set if the vehicle does not use cargo/garrison of infantry.
	UPROPERTY()
	TObjectPtr<UCargo> M_CargoComp;

	// Used to disable the cargo UI at begin play when the nomadic vehicle has not setup the cargo component yet as the building mesh
	// is not known.
	void HideCargoUI();
	


	void AdjustSelectionDecalToConversion(const bool bSetToBuildingPosition) const;

	/**
	 * @brief Handles physics, visibility, collision and overlaps for the chaos vehicle.
	 * @param bDisable Whether to disable or enable the chaos vehicle mesh.
	 */
	void SetDisableChaosVehicleMesh(const bool bDisable);

	/**
	 * @brief Sets the training to enabled if this nomadic vehicle has a training component.
	 * @param bSetEnabled Whether to enable training.
	 * @post If we have a training component the training UI is hidden when we are primary selected.
	 * @post The flag is set on the training component and the queue paused if disabled.
	 */
	void SetTrainingEnabled(const bool bSetEnabled);
	// Enable or disable the cargo for garrison on this vehicle.
	void SetCargoEnabled(const bool bSetEnabled) const;

	void AdjustMaxHealthForConversion(const bool bSetToTruckMaxHealth) const;

	UPROPERTY()
	float M_ExpansionBuildRadius;

	UPROPERTY()
	TArray<USoundCue*> M_ConstructionSounds;

	int32 M_LastPlayedSoundIndex;

	void PlayRandomConstructionSound();

	// Activate or Deactivates the resource drop off component if this vehicle has one.
	void SetResourceDropOffActive(const bool bIsActive) const;
	// Activate or Deactivate the energy component if this vehicle has one.
	void SetEnergyComponentActive(const bool bIsActive) const;
	// Activate or Deactive the radius component if this vehicle has one.
	void SetRadiusComponentActive(const bool bIsActive) const;
	// Activate or Deactivate the reinforcement point if this vehicle has one.
	void SetReinforcementPointActive(const bool bIsActive) const;

	void StartAsConvertedBuilding();

	UPROPERTY()
	UAircraftOwnerComp* M_AircraftOwnerComp = nullptr;

	// Validation helper (rule 0.5)
	bool GetIsValidAircraftOwnerComp() const;

	UPROPERTY()
	FTrainingPreviewHelper M_TrainingPreviewHelper;

	bool UseTrainingPreview() const;

	// Used by FResourceConversionHelper to sync all resource conversion components of this nomadic vehicle.
	FNomadicResourceConversionState M_ResourceConversionState;

	void AnnounceConversion() const;
};
