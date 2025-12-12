// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingExpansionEnums.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "Materials/MaterialInstance.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Buildings/BuildingAttachments/BuildingAttachments.h"
#include "RTS_Survival/Collapse/DestroySpawnActorsParameters.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"

#include "BuildingExpansion.generated.h"


class UWeaponState;
struct FCollapseFX;
struct FCollapseForce;
struct FCollapseDuration;
class UGeometryCollection;
enum class EBuildingExpansionStatus : uint8;
enum class EBuildingExpansionType : uint8;
class RTS_SURVIVAL_API IBuildingExpansionOwner;
class RTS_SURVIVAL_API UTimeProgressBarWidget;

/**
 * A building expansion actor can be added to a building in an expansion slot.
 * These slots are stored in a TArray in the building expansion owner component called UBuildingExpansionOwnerComp.
 * To interact with this component accross multiple different derived actor classes we use the IBuildingExpansionOwner interface.
 *
 * @note Bxps are spawned async using the ARTSAsyncSpawner class.
 * @note The previewmesh is loaded sync with game thread.
 */
UCLASS()
class RTS_SURVIVAL_API ABuildingExpansion : public ASelectableActorObjectsMaster, public ITurretOwner
{
	GENERATED_BODY()

public:

	ABuildingExpansion(FObjectInitializer const& ObjectInitializer);

	/**
	 * @brief Initializes the building expansion with necessary status. 
	 * @param NewOwner The owner of the building expansion.
	 * @param NewStatus The status of the building expansion.
	 * @pre The expansion was spawned async and the owner is set.
	 */
	void OnBuildingExpansionCreatedByOwner(const TScriptInterface<IBuildingExpansionOwner>& NewOwner, EBuildingExpansionStatus NewStatus);

	inline EBuildingExpansionType GetBuildingExpansionType() const { return M_BuildingExpansionType; };
	inline EBuildingExpansionStatus GetBuildingExpansionStatus() const { return M_StatusBuildingExpansion; };
	inline TScriptInterface<IBuildingExpansionOwner> GetBuildingExpansionOwner() const { return M_Owner; }

	/**
	 * Unhides the building expansion and sets the location.
	 * @param Location The location to start the construction.
	 * @param Rotation The rotation of the building expansion.
	 * @param AttachedToSocketName The socket name this bxp is attached to. Should only be populated
	 * if the bxp is of construction type attached to socket.
	 * @note Either sets status to BXS_BeingBuild or BXS_BeingUnpacked.
	 */
	void StartExpansionConstructionAtLocation(
		const FVector& Location,
		const FRotator& Rotation,
		const FName& AttachedToSocketName);

	/**
	 * @brief Starts building expansion animation.
	 * @param TotalTime How long until the animation must be complete
	 * @post The building mesh materials are cached.
	 */
	void StartPackUpBuildingExpansion(const float TotalTime);

	/**
	 * @brief Called when the owner cancels packing. This will restart the building expansion construction if the
	 * bxp was not completely constructed before packing.
	 */
	void CancelPackUpBuildingExpansion();

	/**
	 * @brief Sets status to packedUp and destroys this bxp!
	 */
	void FinishPackUpExpansion();

	inline EBuildingExpansionStatus GetStatus() const { return M_StatusBuildingExpansion; }
	TArray<UWeaponState*> GetAllWeapons() const;
	
protected:
	virtual void BeginDestroy() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void UnitDies(const ERTSDeathType DeathType) override;

	/**
	 * @brief Destroy the bxp and spawn actors around the location.
	 * @param SpawnParams Defines what actors to spawn and where.
	 * @param CollapseFX Optional FX to play.
	 */
	virtual void DestroyAndSpawnActors(
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX) override final;
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnUnitDies();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DisableAllWeapons();

	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	void CollapseMesh(
		UGeometryCollectionComponent* GeoCollapseComp,
		TSoftObjectPtr<UGeometryCollection> GeoCollection,
		UMeshComponent* MeshToCollapse,
		FCollapseDuration CollapseDuration,
		FCollapseForce CollapseForce,
		FCollapseFX CollapseFX
	);

	/**
	 * Setup all properties for the building expansion.
	 * @param Abilities
	 * @param NewConstructionMesh The mesh used during construction for animating.
	 * @param NewBuildingMesh The complete (high poly) mesh of the building.
	 * @param NewProgressBar The progress bar widget used to show the building progress.
	 * @param NewBuildingTime How long it takes to build the building.
	 * @param SmokeSystems The smoke systems used for the building expansion.
	 * @param NewAmountSmokes The amount of smoke systems used for the building expansion per material application.
	 * @param NewSmokeRadius The radius of the smoke systems used for the building expansion.
	 * @param NewConstructionAnimationMaterial The material used for the construction animation.
	 * @param NewBuildingExpansionType The type of building expansion.
	 * @param NewBuildingAttachments The attachments to spawn on the building expansion.
	 * @param bLetBuildingMeshAffectNavMesh
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts", meta = (BlueprintProtected = "true"))
	void InitBuildingExpansion(
		TArray<EAbilityID> Abilities,
		UStaticMesh* NewConstructionMesh,
		UStaticMesh* NewBuildingMesh,
		UTimeProgressBarWidget* NewProgressBar,
		const float NewBuildingTime,
		TArray<UNiagaraSystem*> SmokeSystems,
		const int NewAmountSmokes,
		const float NewSmokeRadius,
		UMaterialInstance* NewConstructionAnimationMaterial,
		EBuildingExpansionType NewBuildingExpansionType,
		TArray<FBuildingAttachment> NewBuildingAttachments, const bool bIsStandAlone = false, const bool bLetBuildingMeshAffectNavMesh = false
	);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Component")
	UStaticMeshComponent* BuildingMeshComponent;

	// To notify blueprint of construction state.
	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingExpansion")
	void BP_OnStartBxpConstruction();

	// To notify blueprint of construction state.
	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingExpansion")
	void BP_OnFinishedExpansionConstruction();

	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingExpansion")
	void BP_OnStartPackUpBxp();

	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingExpansion")
	void BP_OnCancelledPackUpBxp();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "BuildingExpansion")
	void BP_OnFinishedPackupBxp();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Turrets")
	void SetTurretsToAutoEngage();

	/** @brief Adds the provided turret to the array keeping track of all turrets on this bxp. */
	UFUNCTION(BlueprintCallable)
	 void SetupTurret(ACPPTurretsMaster* NewTurret);

	
	/** @brief Adds the provided HullWeapon to the array keeping track of all HullWeapons on this bxp. */
	UFUNCTION(BlueprintCallable)
	inline void SetupHullWeapon(UHullWeaponComponent* NewHullWeapon) { HullWeapons.Add(NewHullWeapon); }

	// Turret owner interface functions
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override final;
	virtual void OnTurretOutOfRange(const FVector TargetLocation, ACPPTurretsMaster* CallingTurret) override final;
	virtual void OnMountedWeaponTargetDestroyed(ACPPTurretsMaster* CallingTurret, UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor, const bool
	                                            bWasDestroyedByOwnWeapons) override final;
	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnProjectileHit(const bool bBounced) override;
	virtual int32 GetOwningPlayer() override final;
	virtual FRotator GetOwnerRotation() const override final;

	// ICommand functions
	virtual void ExecuteAttackCommand(AActor* TargetActor) override final;
	virtual void TerminateAttackCommand() override final;

private:

	// Turrets mounted on this building expansion.
	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> M_TTurrets;

	// The Hull Weapons mounted on this bxp.
	UPROPERTY()
	TArray<UHullWeaponComponent*> HullWeapons;

	bool GetIsValidHullWeapon(const UHullWeaponComponent* HullWeapon) const;

	// The mesh used during construction for animating.
	UPROPERTY()
	UStaticMesh* M_ConstructionMesh;

	// The complete (high poly) mesh of the building.
	UPROPERTY()
	UStaticMesh* M_BuildingMesh;

	// The progress bar widget used to show the building progress.
	UPROPERTY()
	TWeakObjectPtr<UTimeProgressBarWidget> M_ProgressBarWidget;

	float M_BuildingTime;

	UPROPERTY()
	TArray<UNiagaraSystem*> M_SmokeSystems;

	int M_AmountSmokes;
	float M_SmokeRadius;

	// The material applied to the expansion for construction animation.
	UPROPERTY()
	UMaterialInstance* M_ConstructionAnimationMaterial;

	// We use the interface to talk to the bxp component.
	UPROPERTY()
	TScriptInterface<IBuildingExpansionOwner> M_Owner;

	EBuildingExpansionType M_BuildingExpansionType;

	EBuildingExpansionStatus M_StatusBuildingExpansion;

	UPROPERTY()
	FTimerHandle M_BuildingTimerHandle;

	UPROPERTY()
	TArray<FBuildingAttachment> M_BuildingAttachments;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_SpawnedAttachments;

	void OnFinishedExpansionConstruction();

	// Goes through the attachments array and spanws the actors at the sockets.
	void SpawnBuildingAttachments();

	// Destroys all spawned attachments.
	void DestroyBuildingAttachments();

	/**
	 * @brief Sets the status of the building expansion and propagates the status to the owner.
	 * Will also update game UI if needed.
	 * @param NewStatus The new status of the building expansion.
	 */
	void SetStatusAndPropagateToOwner(const EBuildingExpansionStatus NewStatus);

	void OnNewConstructionLocationPropagateStatus(
		const EBuildingExpansionStatus NewStatus,
		const FRotator& NewRotation,
		const FName& NewSocketName);


	// Cache for original materials of the building mesh
	// Always empty by calling ResetCachedMaterials as the index also needs to be reset!
	UPROPERTY()
	TArray<UMaterialInterface*> M_CachedOriginalMaterials;

	// Timer handle for reapplying materials
	FTimerHandle MaterialReapplyTimerHandle;

	// The current material index to reapply a material to.
	UPROPERTY()
	int32 M_MaterialIndex;

	// Keeps track of how much time has elapsed when the construction was interrupted by packing up the owner.
	// Is set to zero upon completion of the construction.
	float M_TimeElapsedWhenConstructionCancelled;

	// To save original materials of the building mesh.
	void CacheOriginalMaterials();

	// Set all materials on the construction mesh to the construction material.
	void ApplyConstructionMaterial() const;

	// Called by timer to reapply original materials one by one.
	void ApplyCachedMaterials();

	// To stop reapplying materials and finish building.
	void FinishReapplyingMaterials();

	// Also resets the class-global material index in addition to the dynamic cache array.
	void ResetCachedMaterials();

	/**
	 * Creates random smoke systems in the radius of the building expansion.
	 * @param AmountSystemsToSpawn 
	 */
	void CreateSmokeSystems(const int AmountSystemsToSpawn) const;

	void CreateRandomSmokeSystemAtTransform(const FTransform Transform) const;

	/**
	 * @brief Will cache the building materials, apply construction materials to all slots
	 * and start the construction animation
	 * @param BuildingTime The time it takes for the animation to complete.
	 * @param bCacheOriginalMaterials Always leave this at true, unless we cancelled packing up after cancelling construction.
	 * @post Will eventually call OnFinishedExpansionConstruction if the status is set to beingbuild.
	 */
	void StartConstructionAnimation(
		const float BuildingTime,
		const bool bCacheOriginalMaterials = true);

	/**
	 * @brief Start the procedural mesh animation over all material slots.
	 * @param Interval In seconds, how long between animating each slot.
	 * @param bStartAtMaterialZero If true, the first material slot will be animated first for a regular bottom up animation
	 * if false, we start with the last material slot for a top down animation (when packing up).
	 * @post Calls On finished construction if the status indicates this was a construction animation.
	 * Otherwise we wait for the owner to finalise packing up all expansions.
	 */
	void StartAnimationTimer(const float Interval, const bool bStartAtMaterialZero);

	// The actor specifically targeted by this bxp.
	UPROPERTY()
	AActor* M_TargetActor;

	// Whether this bxp is a stand alone building that has no owner.
	bool bM_IsStandAlone;

	void OnBxpStandAlone();

	void SetAllTurretsDisabled();

	void SetMeshToBuildingMesh();
	void SetMeshToConstructionMesh();

	/**
	 * @brief IsValid checks the progress bar widget. 
	 * @param FunctionName The function at which we perform this check.
	 * @return true if the progress bar widget is valid.
	 */
	bool EnsureProgressBarIsValid(const FString& FunctionName) const;

	/**
	 * @brief Checks the provided world paramter for validity.
	 * @param World The world to check. 
	 * @param FunctionName At what function we call for this check.
	 * @return true if the world is valid.
	 */
	bool EnsureWorldIsValid(const UWorld* World, const FString& FunctionName) const;

	/**
	 * 
	 * @param TargetActor The main target actor for this building expansion. 
	 * @param KilledActor The actor killed by the turret of the building expansion.
	 * @return 
	 */
	bool DidKillTargetActorOrTargetNoLongerValid(AActor* TargetActor, AActor* KilledActor);

	/**
	 * @brief Checks if the building mesh component is valid.
	 * @param FunctionName The function at which we perform this check.
	 * @return true if the building mesh component is valid.
	 */
	bool EnsureBuildingMeshComponentIsValid(const FString& FunctionName) const;

	void OnTurretTargetDestroyed(ACPPTurretsMaster* CallingTurret, AActor* DestroyedActor);
	void OnHullWeaponKilledActor(UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor);

	void OnInitBuildingExpansion_SetupCollision(const bool bLetBuildingComponentAffectNavmesh) const;
	void BeginPlay_NextFrameInitAbilities();

	void DebugDisplayMessage(const FString& Message) const;

};
