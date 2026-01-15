// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "SquadControllerDataCallback/SquadControllerDataCallback.h"
#include "Squads/SquadPathFinding/SquadPathFindingError.h"
#include "Squads/SquadWeaponSwitch/SquadWeaponSwitch.h"
#include "SquadController.generated.h"

class UBehaviourComp;
class RTS_SURVIVAL_API UFieldConstructionAbilityComponent; 
class UWeaponState;
enum class EWeaponName : uint8;
class USquadHealthComponent;
class UCargoSquad;
class URTSExperienceComp;
enum class ESquadSubtype : uint8;
class AItemsMaster;
class ASquadUnit;
class UGrenadeComponent;
class ACPPController;
class AWeaponPickup;
class UReinforcementPoint;
class UHealthComponent;
class URTSComponent;
class AScavengeableObject;
struct FSquadWeaponSwitch;
class USquadUnitHealthComponent;
class USquadReinforcementComponent;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
enum class ERTSDeathType : uint8;

// Will only start to exe the action once the squad is fully loaded.
USTRUCT()
struct FSquadStartGameAction
{
	GENERATED_BODY()
	FSquadStartGameAction();

	void InitStartGameAction(const EAbilityID InAbilityID,
	                         AActor* InTargetActor,
	                         const FVector& InTargetLocation,
	                         ASquadController* InMySquad);

	void OnUnitDies(UWorld* World);

	EAbilityID StartGameAction;

	UPROPERTY()
	FTimerHandle ActionTimer;

	UPROPERTY()
	AActor* TargetActor = nullptr;

	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

protected:
	UPROPERTY()
	TWeakObjectPtr<ASquadController> M_MySquad = nullptr;

	void TimerIteration();
	bool GetIsValidSquadController() const;
	bool GetIsSquadFullyLoaded() const;
	bool ExecuteStartAbility() const;
};

USTRUCT(BlueprintType)
struct FDamageSquadGameStart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|GameStart")
	int32 AmountSquadUnitsInstantKill = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|GameStart")
	int32 AmountSquadUnitsPercentageDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|GameStart")
	float PercentageLifeLeft = 0.0f;

	void ApplyStartDamageToSquad(const TArray<ASquadUnit*>& SquadUnits) const;

protected:
	USquadUnitHealthComponent* GetValidSquadUnitHealthComponent(const ASquadUnit* SquadUnit) const;
	void KillSquadUnits(const TArray<ASquadUnit*>& SquadUnits, const int32 UnitsToKill) const;
	void DamageUnitsToPercentage(const TArray<ASquadUnit*>& SquadUnits, const int32 UnitsToDamage, int32 InstantKilledUnits) const;
};
// Work around to set the spawn location for the squad from the trainer.
USTRUCT()
struct FSquadSpawnLocation
{
	GENERATED_BODY()

	bool bIsSetByTrainer = false;

	FVector SpawnLocation = FVector::ZeroVector;
	
};

DECLARE_MULTICAST_DELEGATE(FOnWeaponPickup);

USTRUCT()
struct FTargetPickupItemState
{
	GENERATED_BODY()

	FTargetPickupItemState();

	void Reset()
	{
		M_TargetPickupItem = nullptr;
		bIsBusyPickingUp = false;
	}

	UPROPERTY()
	AItemsMaster* M_TargetPickupItem = nullptr;

	UPROPERTY()
	bool bIsBusyPickingUp = false;
};

USTRUCT()
struct FTargetScavengeState
{
	GENERATED_BODY()

	FTargetScavengeState();

	void Reset()
	{
		M_TargetScavengeObject = nullptr;
		bIsBusyScavenging = false;
	}

	UPROPERTY()
	AScavengeableObject* M_TargetScavengeObject = nullptr;

	// To notify the controller of the first squad unit that reaches the scavengable object.
	// Further  squad units that reach will not trigger the start scavenging logic as by then this
	// bool has been set to true.
	UPROPERTY()
	bool bIsBusyScavenging = false;
};

USTRUCT()
struct FRepairState
{
	GENERATED_BODY()

	FRepairState();

	void Reset()
	{
		M_TargetRepairObject = nullptr;
	}

	UPROPERTY()
	AActor* M_TargetRepairObject = nullptr;
};

USTRUCT()
struct FCaptureState
{
	GENERATED_BODY()

	FCaptureState();

	void Reset()
	{
		M_TargetCaptureActor = nullptr;
		bIsCaptureInProgress = false;
	}

	UPROPERTY()
	AActor* M_TargetCaptureActor = nullptr;

	// True once a capture has started (units have been committed).
	UPROPERTY()
	bool bIsCaptureInProgress = false;
};

USTRUCT(BlueprintType)
struct FPickupSoundSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup Sound")
	TObjectPtr<USoundBase> PickupSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup Sound")
	TObjectPtr<USoundAttenuation> PickupSoundAttenuation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Pickup Sound")
	TObjectPtr<USoundConcurrency> PickupSoundConcurrency = nullptr;
};


USTRUCT()
struct FSquadLoadingStatus
{
	GENERATED_BODY()
	bool bM_HasInitializedData = false;
	bool bM_HasFinishedLoading = false;
	int32 M_AmountOfSquadUnitsToLoad = 0;
};

/**
 * Call set SquadVisionRange to start the Fow on the squad units. 
 */
UCLASS()
class RTS_SURVIVAL_API ASquadController : public AActor, public ICommands,
                                          public IRTSUnit, public IExperienceInterface,
                                          public IHealthBarOwner
{
	GENERATED_BODY()

public:
	friend struct FSquadWeaponSwitch;
	friend class RTS_SURVIVAL_API AWeaponPickup;
	friend class RTS_SURVIVAL_API AScavengeableObject;
	friend class RTS_SURVIVAL_API URepairComponent;
	friend class RTS_SURVIVAL_API UCargoSquad;
	friend class RTS_SURVIVAL_API UGrenadeComponent;
	friend class RTS_SURVIVAL_API USquadReinforcementComponent;
	friend class RTS_SURVIVAL_API UFieldConstructionAbilityComponent;
	ASquadController();


	TArray<UWeaponState*> GetWeaponsOfSquad();

	void SetSquadSpawnLocation(const FVector& SpawnLocation);
	/**
	 * @brief Request squad move using the controller's coordinated pathing for a specific ability.
	 * @param MoveToLocation Destination to move.
	 * @param AbilityID Ability context for completion callbacks.
	 */
	virtual void RequestSquadMoveForAbility(const FVector& MoveToLocation, EAbilityID AbilityID);

	// To register callbacks once the data from blueprint is loaded into the squad.
	FSquadDataCallbacks SquadDataCallbacks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|GameStart")
	FDamageSquadGameStart DamageSquadGameStart;


	bool GetIsSquadFullyLoaded() const;
	void PlaySquadUnitLostVoiceLine();

	// Updates the decal and selection logic for the squad units their selection components.
	void SetSquadSelected(const bool bIsSelected);
	void OnSquadUnitInCombat();

	int32 GetSquadUnitIndex(const ASquadUnit* SquadUnit);

	// Delegate called when the squad picks up any weapon.
	FOnWeaponPickup OnWeaponPickup;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Reference")
	USquadHealthComponent* SquadHealthComponent;


	bool GetIsValidSquadHealthComponent() const;


	/** @return: The Units in the Squad of this UnitSquad */
	TArray<ASquadUnit*> GetSquadUnitsChecked();

	virtual bool GetIsScavenger() override;
	virtual bool GetIsRepairUnit() override;
	virtual float GetUnitRepairRadius() override;

	/**
	 * @brief removes the provided Um from the Squad. If the unit was selected it is removed from
	 * the selection. If this causes the squad to be empty, the squad will be deselected.
	 * @param DeathType Used for last-unit cleanup behavior.
	 */
	virtual void UnitInSquadDied(ASquadUnit* UnitDied, bool bUnitSelected, ERTSDeathType DeathType);

	// Checks if all squad units did complete the command; if so, calls DoneExecutingCommand with the CompletedAbilityID.
	virtual void OnSquadUnitCommandComplete(EAbilityID CompletedAbilityID);

	/**
	 * @brief Initialise spawn grid for reinforcing units around the provided location.
	 * @param OriginLocation Center location used for spawn grid.
	 */
	void BeginReinforcementSpawnGrid(const FVector& OriginLocation);

	/**
	 * @brief Fetch next formation point for a reinforced squad unit.
	 * @return Projected location on the navmesh.
	 */
	FVector GetNextReinforcementSpawnLocation();

	/**
	 * @brief Register a newly spawned reinforcement unit with this squad.
	 * @param ReinforcedUnit Newly spawned squad unit.
	 */
	void RegisterReinforcedUnit(ASquadUnit* ReinforcedUnit);

	void OnSquadFullyReinforced();

	/**
	 * @brief Accessor for the reinforcement component managing this squad.
	 * @return Reinforcement component or nullptr when missing.
	 */
	TObjectPtr<USquadReinforcementComponent> GetSquadReinforcementComponent() const;

	bool GetIsValidGrenadeComponent() const;

	inline URTSComponent* GetRTSComponent() const { return RTSComponent; }

	inline AItemsMaster* GetTargetPickupItem() const { return M_TargetPickupItemState.M_TargetPickupItem; }

	inline AScavengeableObject* GetTargetScavengeObject() const { return M_TargetScavengeState.M_TargetScavengeObject; }

	/** @return Whether the squad has atleast one unit that can pickup items. */
	bool CanSquadPickupWeapon();

	/** @return Whether the squad has at least one secondary weapon */
	bool HasSquadSecondaryWeapons();

	FVector GetSquadLocation();


	void OnMoveToScavengeObjectComplete();

	// Called by scavengable objects when the scavenging timer is complete.
	void OnScavengingComplete();
	void ConsumeSquadOnScavengingComplete();

	// Called once one of the units arrives at the capture actor.
	void OnSquadUnitArrivedAtCaptureActor();

	// Called once one of the units reaches the grenade throw location.
	void OnSquadUnitArrivedAtThrowGrenadeLocation(ASquadUnit* SquadUnit);

	inline int GetSquadUnitsCount() const { return M_TSquadUnits.Num(); }

	/**
	 * @brief Handles switching weapons between squad units when one dies.
	 * @param UnitThatDied The unit that initiated the death event.
	 */
	void HandleWeaponSwitchOnUnitDeath(ASquadUnit* UnitThatDied);

	virtual void
	OnRTSUnitSpawned(const bool bSetDisabled, const float TimeNotSelectable, const FVector MoveTo) override;

	void OnSquadUnitOutOfRange(const FVector& TargetLocation);

	// ICommand interface helpers.
	virtual FVector GetOwnerLocation() const override final;
	virtual FString GetOwnerName() const override final;
	virtual AActor* GetOwnerActor() override final;

	// IHealthBarOwner Interface.
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override final;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetCanCaptureUnits() override final;

	// Will continuously try exe the action until the squad is fully loaded and actually exe it. 
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetSquadStartGameAction(AActor* TargetActor, const FVector TargetLocation, const EAbilityID StartGameAbility);

	/** Array of soft class references to squad units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	TArray<TSoftClassPtr<ASquadUnit>> SquadUnitClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio")
	FPickupSoundSettings PickupSoundSettings;

	UPROPERTY(BlueprintReadWrite, Category = "ReferenceCasts")
	TObjectPtr<ACPPController> PlayerController;

	// Contains OwningPlayer and UnitType.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference")
	TObjectPtr<URTSComponent> RTSComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Reference")
	UBehaviourComp* BehaviourComponent;

	bool GetIsValidBehaviourComponent() const;

	bool GetIsValidRTSComponent() const;
	bool EnsureValidExperienceComponent();

	void InitSquadData_SetupSquadHealthAggregation();


	// ----------------- START EXPERIENCE INTERFACE -----------------
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly)
	TObjectPtr<URTSExperienceComp> ExperienceComponent;

	virtual URTSExperienceComp* GetExperienceComponent() const override final;
	virtual void OnUnitLevelUp() override final;

	// ----------------- END EXPERIENCE INTERFACE -----------------

	/**
	 * @brief Initializes squad data specific for this squad, call in begin play.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnSquadSubtypeSet(
	);


	virtual void PostInitializeComponents() override;

	/**
	 * @brief Allows for custom unit-specific logic when the unit is reset for a new command.
	 * Logic common to all units like stopping behaviour trees and movement is handled by the CommandData struct.
	 */
	virtual void SetUnitToIdleSpecificLogic() override;

	/**
	 * @return The command data used by this unit. Like MoveToLocations and TargetActors.
	 */
	virtual UCommandData* GetIsValidCommandData() override final;

	// Called when the last command was terminated and the unit has no new commands in the queue.
	virtual void OnUnitIdleAndNoNewCommands() override;

	/**
     * @brief Move the unit to the specified location.
     * @param MoveToLocation The location to move to.
     */
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;

	/** @brief Stops all logic used for move commands.*/
	virtual void TerminateMoveCommand() override;

	virtual void ExecuteReinforceCommand(AActor* ReinforcementTarget) override;
	virtual void TerminateReinforceCommand() override;

	virtual void ExecuteCaptureCommand(AActor* CaptureTarget) override;
	virtual void TerminateCaptureCommand() override;

	void RemoveCaptureUnitsFromSquad(const int32 AmountCaptureUnits);

	virtual void ExecuteFieldConstructionCommand(const EFieldConstructionType FieldConstruction, const FVector& ConstructionLocation, const FRotator& ConstructionRotation, AActor* StaticPreviewActor) override;
	virtual void TerminateFieldConstructionCommand(EFieldConstructionType FieldConstructionType, AActor* StaticPreviewActor) override;
	void StartFieldConstructionMove(const FVector& ConstructionLocation);

	/**
	 * @brief Attack the specified target.
	 * @param TargetActor The actor to attack.
	 */
	virtual void ExecuteAttackCommand(AActor* TargetActor) override;

	/** @brief Run When the unit finished their attack command.*/
	virtual void TerminateAttackCommand() override;

	virtual void ExecuteAttackGroundCommand(const FVector GroundLocation) override;
	virtual void TerminateAttackGroundCommand() override;

	/**
	 * @brief Move the unit to the specified location.
	 * @param PatrolToLocation The location to move to.
	 */
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation) override;

	virtual void TerminatePatrolCommand() override;

	virtual void ExecutePickupItemCommand(AItemsMaster* TargetItem) override;

	virtual void TerminatePickupItemCommand() override;

	virtual void ExecuteSwitchWeaponsCommand() override;
	virtual void TerminateSwitchWeaponsCommand() override;

	/**
	 * @brief Force-reverse moves the unit to the specified location.
	 * @param ReverseToLocation The location to reverse move to.
	 */
	virtual void ExecuteReverseCommand(const FVector ReverseToLocation) override;

	/** @brief Run when the unit finished their turn towards command.*/
	virtual void TerminateReverseCommand() override;

	/**
	 * @brief Rotates the unit towards the specified rotator.
	 * @param RotateToRotator The rotator to rotate towards.
	 * @param IsQueueCommand Whether this command needs to call DoneExecutingCommand or is not called from queue
	 * but separately.
	 */
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;

	/** @brief Run when the unit finished their move command.*/
	virtual void TerminateRotateTowardsCommand() override;

	virtual void ExecuteScavengeObject(AActor* TargetObject) override;
	virtual void TerminateScavengeObject() override;

	virtual void ExecuteRepairCommand(AActor* TargetActor) override;
	virtual void TerminateRepairCommand() override;

	virtual void ExecuteThrowGrenadeCommand(const FVector TargetLocation,
	                                        const EGrenadeAbilityType GrenadeAbilityType) override;
	virtual void TerminateThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType) override;
	virtual void ExecuteCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType) override;
	virtual void TerminateCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType) override;

	virtual void ExecuteEnterCargoCommand(AActor* CarrierActor) override;
	virtual void TerminateEnterCargoCommand() override;

	virtual void ExecuteExitCargoCommand() override;
	virtual void TerminateExitCargoCommand() override;
	// Util to stop behaviour trees on any IcommandUnit.
	virtual void StopBehaviourTree() override;

	// Util to stop movement on any ICommand unit.
	virtual void StopMovement() override;

	// Triggered by the pickup item that has overlap with one of the units in this squad.
	void StartPickupWeapon(AWeaponPickup* TargetWeaponItem);

	/** @return True if the object is valid and is scavengable, false otherwise */
	bool ExeScav_EnsureValidObj(const AScavengeableObject* ScavengeableObject);

	/** @brief Depending on the distance to the scavengable object, the squad will either start scavenging or move to it. */
	void ExeScav_StartOrMoveToScavObj(AScavengeableObject* ScavengeableObject, const FVector& SquadUnitLocation);

	// Triggered by the scavengable objects that has overlap with one of the units in the squad.
	void StartScavengeObject(AScavengeableObject* ScavengeableObject);

protected:
	/** Cargo manager component for this squad. */
	UPROPERTY()
	TObjectPtr<UCargoSquad> CargoSquad;

	UPROPERTY()
	TArray<TObjectPtr<UFieldConstructionAbilityComponent>> M_FieldConstructionAbilities;
	
	UPROPERTY()
	FSquadSpawnLocation M_SquadSpawnLocation;

	/** Handles reinforcement activation and spawning missing squad units. */
	UPROPERTY()
	TObjectPtr<USquadReinforcementComponent> SquadReinforcement;

	UPROPERTY()
	TObjectPtr<UGrenadeComponent> M_GrenadeComponent;

	EGrenadeAbilityType M_ActiveGrenadeAbilityType = EGrenadeAbilityType::DefaultGerBundleGrenade;

	UPROPERTY()
	FSquadStartGameAction M_SquadStartGameAction;

	UPROPERTY()
	FSquadWeaponSwitch M_SquadWeaponSwitch;

	void InitSquadData_InitExperienceComponent();

	/** Async load the squad unit classes */
	virtual void LoadSquadUnitsAsync();

	/** Callback for when a squad unit class is loaded */
	virtual void OnSquadUnitClassLoaded(TSoftClassPtr<ASquadUnit> LoadedClass);

	virtual void OnAllSquadUnitsLoaded();
	
	void UnitInSquadDied_HandleGrenadeComp(ASquadUnit* UnitDied) const;
	/**
	 * @brief Part of init squad data, sets the fow vision for all squad units.
	 * @param NewVision The vision range to set.
	 */
	void SetSquadVisionRange(const float NewVision);

	UPROPERTY()
	TArray<ASquadUnit*> M_TSquadUnits;

	// Contains the data for the commands that this unit can execute using the ICommands interface.
	// Needs to be accessible by derived classes to get data like current command.
	UPROPERTY()
	UCommandData* M_UnitCommandData;

	bool GetIsValidSquadUnit(const ASquadUnit* Unit) const;
	bool GetIsValidSquadReinforcementComponent() const;
	void UpdateReinforcementAvailability();
	bool GetIsValidPlayerController();

	void BeginPlay_SetupPlayerController();
	void PostInitializeComponents_SetupGrenadeComponent();
	void PostInitializeComponent_SetupFieldConstructionAbilities();
	UFieldConstructionAbilityComponent* GetFieldConstructionAbility(const EFieldConstructionType ConstructionType) const;

	int M_UnitsCompletedCommand = 0;

	/** Generates paths for each squad unit based on the move location. */
	virtual ESquadPathFindingError GeneratePathsForSquadUnits(const FVector& MoveToLocation);


	/**
	 * @brief First step of squad path generation. Sets up the navigation system and checks if it is valid.
	 * @param OutWorld World pointer checked.
	 * @param OutNavSystem Nav system reference checked.
	 * @param OutAIController Squad unit AI controller checked.
	 * @return Whether all the checks passed.
	 */
	ESquadPathFindingError GeneratePaths_SetupNav(UWorld*& OutWorld, UNavigationSystemV1*& OutNavSystem,
	                                              AAIController*& OutAIController);

	/**
	 * @brief Second step of squad path generation. Creates a move request and query for the squad.
	 * @param MoveToLocation Final location to move to.
	 * @param NavSystem The navigation system to use.
	 * @param AIController Squad unit's AI controller.
	 * @param OutPFQuery The pathfinding query to fill.
	 * @return Whether building the pathfinding query was successful.
	 */
	ESquadPathFindingError GeneratePaths_GenerateQuery(const FVector& MoveToLocation, UNavigationSystemV1* NavSystem,
	                                                   const AAIController* AIController,
	                                                   FPathFindingQuery& OutPFQuery);

	/**
	 * @brief The third step of squad path generation. Finds the path for the squad and checks if it is valid.
	 * @param NavSystem The navigation system to use.
	 * @param PFQuery The pathfinding query to use.
	 * @param OutNavPath 
	 * @return The path for the squad will return null if invalid path.
	 */
	ESquadPathFindingError GeneratePaths_ExeQuery(UNavigationSystemV1* NavSystem, const FPathFindingQuery& PFQuery,
	                                                FNavPathSharedPtr& OutNavPath) const;

	/**
	 * @brief The final step of squad path generation. Assigns the paths to the squad units.
	 * @param MoveToLocation The location to move to.
	 * @param SquadPath By reference the path to assign to the squad units.
	 */
	ESquadPathFindingError GeneratePaths_Assign(const FVector& MoveToLocation, const FNavPathSharedPtr& SquadPath);

	FVector GetFinalPathPointOffset(const FVector& UnitOffset) const;

	TArray<FVector> M_SqPath_Offsets;

	/** The paths for each squad unit. */
	TMap<ASquadUnit*, FNavPathSharedPtr> M_SquadUnitPaths;

	// Contains the reference to the target item and whether we have already started picking up this item.
	UPROPERTY()
	FTargetPickupItemState M_TargetPickupItemState;

	UPROPERTY()
	FTargetScavengeState M_TargetScavengeState;

	UPROPERTY()
	FRepairState M_RepairState;

	UPROPERTY()
	FCaptureState M_CaptureState;

	/**
	 * @param TargetActor The item to get the distance to.
	 * @return The shortest distance to the item from the squad units.
	 * @pre Assumes that the provided TargetItem is valid checked.
	 */
	float GetDistanceToActor(TObjectPtr<AActor> TargetActor);

	// Directly call the start pick up function for the target item instead of waiting for unit movement.
	// Will also call done executing command for this pickup command.
	void OnItemCloseEnoughInstantPickup();

	void CheckCloseEnoughToItemPickupAnyway();

	bool GetIsValidWeaponPickup(AWeaponPickup* TargetWeaponItem) const;

	/**
	 * @brief Finds the eligible squad unit with the lowest weapon value for a pickup.
	 * @return Unit to receive the pickup; nullptr when none are eligible.
	 */
	TObjectPtr<ASquadUnit> FindSquadUnitWithLowestWeaponValueForPickup();

	/**
	 * @brief Checks if the provided squad unit can pick up a weapon.
	 * @param SquadUnit Squad unit evaluated for eligibility.
	 * @return True when the unit can receive a weapon pickup.
	 */
	bool CanSquadUnitPickupWeapon(ASquadUnit* SquadUnit) const;

	/**
	 * @brief Fetches the current weapon value of a squad unit using Global_GetWeaponValue.
	 * @param SquadUnit Squad unit to evaluate.
	 * @return Weapon value for comparison; Max int when the value cannot be determined.
	 */
	int32 GetWeaponValueForSquadUnit(const ASquadUnit* SquadUnit) const;

	bool IsPickupWeaponCommandActive(const AWeaponPickup* TargetWeaponItem) const;

	void PlayPickupSound(const ASquadUnit* SquadUnit) const;

	/**
	 * @brief Move the squad to the provided location, attempts to path find once using the squad otherwise units use
	 * self path finding.
	 * 
	 * @param MoveToLocation The location to move to.
	 * @param AbilityID For what ability this move to command is.
	 * @post The active ability ID is set on each squad unit and a call back is bound on when the move is completed.
	 */
	virtual void GeneralMoveToForAbility(const FVector& MoveToLocation, EAbilityID AbilityID);

	FVector FindNavigablePointNear(const FVector& Location, float Radius) const;

	void EnsureSquadUnitsValid();

	/**
	* @brief Initializes the spawn location generator.
	* @param GridOriginLocation The origin point around which the grid is centered.
	*/
	void StartGeneratingSpawnLocations(const FVector& GridOriginLocation);

	FVector GetSpawnLocationForSquad()const;

	/**
	 * @brief Generates the next spawn location in the grid.
	 * @return The next grid point location.
	 * @note Do not call, use StartGeneratingSpawnLocations followed by FindIdealSpawnLocation for each unit!
	 */
	FVector FindIdealSpawnLocation_GetNextGridPoint();

	FVector FindIdealSpawnLocation();

	// Index used for generating grid spawn locations
	int32 M_SpawnIndex;

	// Origin location for the grid
	FVector M_GridOriginLocation;

	// Size of the grid (distance between points)
	float M_GridSize;

	// Array to store the offsets for grid points
	TArray<FVector> M_Offsets;

	FVector ProjectLocationOnNavMesh(const FVector& Location, const float Radius,
	                                 const bool bProjectInZDirection) const;

	FTimerHandle M_SpawningTimer;


	/**
	 * @brief Updates the controller's position to the average position of all squad units.
	 *
	 * Calculates the average location of all valid squad units and sets the controller's position to this average.
	 * This function is intended to be called repeatedly at a fixed interval.
	 * @note Called every 0.33 seconds by a timer set in BeginPlay().
	 */
	void UpdateControllerPositionToAverage();

	// Vision range of the squad propagated to the squad units.
	float M_SquadVisionRange = 0.0f;

	// How many units are left to spawn, if the squad is fully loaded and a timer handle
	// to delay the initialization of the squad data if not all of the squad units are loaded in yet.
	FSquadLoadingStatus M_SquadLoadingStatus;

	void Debug_Scavenging(const FString& DebugMessage) const;
	void Debug_ItemPickup(const FString& DebugMessage) const;

	// Sets the data loaded from the game state for the squad subtype.
	void InitSquadData();
	void InitSquadData_SetValues(
		const float MaxWalkSpeed,
		const float MaxAcceleration,
		const float MaxHealth,
		const float VisionRadius,
		const ESquadSubtype SquadSubtype, const FResistanceAndDamageReductionData& ResistanceData);

	// Called after squad is loaded. Assuming that the subtype is set.
	void InitSquadData_SetAbilities(const TArray<FUnitAbilityEntry>& Abilities);

	void OnRepairUnitNotValidForRepairs();
	void OnRepairTargetFullyRepaired(AActor* RepairTarget);

	// ----------------- Weapon Icon -----------------
	void UpdateSquadWeaponIcon();
	void SetWeaponIcon(const EWeaponName HighestValuedWeapon);

};
