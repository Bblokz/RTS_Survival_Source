// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Resources/Harvester/HarvesterInterface/HarvesterInterface.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoOwner/CargoOwner.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldOwner/ShieldOwner.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachWeaponAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapAbilityTypes.h"
#include "RTS_Survival/RTSComponents/TowMechanic/TowAbilityTypes/TowAbilityTypes.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBatches.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/CrewRepairAbilityTypes.h"
#include "TankMaster.generated.h"

enum class ERTSAggroBehaviour : uint8;
class UHullWeaponComponent;
class UTankEnergyComponent;
class URTSOptimizer;
class UDigInComponent;
class USpatialVoiceLinePlayer;
class ACPPResourceMaster;
class UArmor;
class UArmorCalculation;
class UVehicleModuleSubsystem;
struct FHealthHealingReceipt;
class URTSNavCollision;
class UBehaviour;
class UBehaviourComp;
// Forward Declaration; reduce compile time; guard against cyclic dependencies.
class RTS_SURVIVAL_API AAITankMaster;
class RTS_SURVIVAL_API ACPPTurretsMaster;
class UTurretSwapComp;
class UVehicleFireFeedbackComponent;
class UVehicleTowComponent;
class UTowedActorComponent;
class UShieldComponent;
class UCargoSquad;
class UCargo;
class ATeamWeaponController;
class ATeamWeapon;
class UWeaponState;

enum class ETurretRangePursuitStatus : uint8
{
	None,
	Active,
	FailureBackoff
};

enum class ETurretRangeMovementEndMode : uint8
{
	FullStop,
	MovementHandoff
};

USTRUCT()
struct FTankTurretRangePursuitState
{
	GENERATED_BODY()

	ETurretRangePursuitStatus Status = ETurretRangePursuitStatus::None;
	uint64 RequestSerial = 0;

	UPROPERTY()
	TWeakObjectPtr<UObject> OwningWeapon;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	FVector TargetLocation = FVector::ZeroVector;
	double LastRequestTimeSeconds = 0.0;
	double RetryNotBeforeTimeSeconds = 0.0;
};

enum class ECrewRepairStatus : uint8
{
	Inactive,
	Repairing,
	// Cleanup in progress; re-entrant stop requests are ignored.
	Stopping
};

enum class ECrewRepairStopReason : uint8
{
	PlayerDisabled,
	AllModulesRecovered,
	// The command queue terminated the command and owns queue progression.
	QueueTermination,
	StartFailed,
	// Death or EndPlay: clear everything without enabling weapons or advancing commands.
	OwnerDestroyed
};

/** @brief Session of the tank's CrewRepair action; one repeating timer per active tank, no per-module timers. */
USTRUCT()
struct FCrewRepairState
{
	GENERATED_BODY()

	ECrewRepairStatus Status = ECrewRepairStatus::Inactive;

	// Red module being repaired; kept until it is repaired, removed or repaired externally.
	int32 CurrentModuleId = INDEX_NONE;

	double ModuleWorkStartGameTime = 0.0;
	float RequiredModuleSeconds = 0.f;

	FTimerHandle TimerHandle;

	// Incremented on every start and stop so stale timer callbacks are ignored.
	uint32 SessionGeneration = 0;

	// Execution serial of the queued Enable command owned by this session; 0 when none.
	uint64 ActiveCommandToken = 0;

	UPROPERTY()
	FAbilitySuppressionHandle AbilitySuppressionHandle;

	bool bHoldsWeaponAndMovementLock = false;
};

/** @brief One behaviour's mobility restriction; the tank composes all sources by minimum. */
USTRUCT()
struct FTankMobilityRestriction
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UObject> Source;

	float TravelSpeedMultiplier = 1.f;
	float TurnRateMultiplier = 1.f;
	float AccelerationMultiplier = 1.f;
};

/** @brief Cached module counts plus the ordinary-healing finishing progress of a module tank. */
USTRUCT()
struct FVehicleModuleRepairState
{
	GENERATED_BODY()

	int32 DestroyedModuleCount = 0;
	int32 NonHealthyModuleCount = 0;

	// Armor damage revision the finishing work belongs to; any new module damage resets the work.
	uint32 ModuleDamageRevision = 0;

	float FullServiceAccumulatedWork = 0.f;
};

/** @brief The turret gun that owns the tank's single Weapon module when the tank installed that module itself. */
USTRUCT()
struct FTankAutoWeaponModuleBinding
{
	GENERATED_BODY()

	// INDEX_NONE until the tank installs the Weapon module; a designer-installed Weapon module is never moved.
	int32 ModuleId = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<ACPPTurretsMaster> Turret;

	UPROPERTY()
	TWeakObjectPtr<UWeaponState> Weapon;

	float WeaponCalibre = 0.f;
};

/**
 * @brief Turrets install their own vehicle modules the tick after BeginPlay, once their weapons exist.
 * The tank batches them in mount order so module slot IDs match between spawns and saves.
 */
USTRUCT()
struct FTankTurretModuleRegistrationState
{
	GENERATED_BODY()

	// Turrets that began play but whose weapon arrays are not yet guaranteed complete.
	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> PendingTurrets;

	// Turrets with complete weapons; installed together once no turret is pending.
	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> ReadyTurrets;

	// Turrets with an armored mount; the candidates for the tank's single Weapon module.
	UPROPERTY()
	TArray<TWeakObjectPtr<ACPPTurretsMaster>> RegisteredTurrets;

	UPROPERTY()
	FTankAutoWeaponModuleBinding AutoWeaponModule;

	// Load received before the turret modules existed; applied afterwards so the module counts match.
	TOptional<FVehicleModuleSaveData> DeferredSaveData;
};

USTRUCT()
struct FTankStartGameAction
{
	GENERATED_BODY()

	FTankStartGameAction();

	void InitStartGameAction(const EAbilityID InAbilityID,
	                         AActor* InTargetActor,
	                         const FVector& InTargetLocation,
	                         ATankMaster* InTankMaster, const FRotator& InEndRotation);

	void OnBeginPlay();
	void OnTankDestroyed(UWorld* World);

private:
	void TimerIteration();
	void StartTimerForNextFrame();
	void ClearActionTimer();
	bool GetIsValidTankMaster() const;
	bool ExecuteStartAbility() const;

	EAbilityID StartGameAction;

	UPROPERTY()
	FTimerHandle ActionTimer;

	UPROPERTY()
	AActor* TargetActor = nullptr;

	UPROPERTY()
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator EndRotation = FRotator::ZeroRotator;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster = nullptr;

	bool bM_BeginPlayCalled;
};

/**
 * @brief Tank with logic to add turrets and engage targets.
 * @note ***********************************************************************************************
 * @note Set in GrandChild Blueprints:
 * @note 1) SetupTurret() to add a new turret to this vehicle.
 * @note ***********************************************************************************************
 * @note In child blueprints like bp_TrackedTankMaster and bp_ChaosTankMaster.
 * @note InitTankMaster is used to get a reference to the controller.
 * @note ***********************************************************************************************
 * @note In Grandchild blueprints / The specific tank blueprints.
 * @note TRACKED TANKS: call InitTrackedTank to setup rotation speed and force multipliers.
 * @note CHAOS TANKS:  call InitChaosTank to setup the gear up and down logic and the rotation speed.
 * @note ***********************************************************************************************
 * Uses a final overwrite on ExecuteCommandMove as both tracked and wheeled vehicle use the same VehicleAIInterface.
 * todo ResetUnitSpecificLogic with turrets.
 */
UCLASS()
class RTS_SURVIVAL_API ATankMaster : public ASelectablePawnMaster, public ITurretOwner,
                                     public IHarvesterInterface, public IRTSNavAgentInterface, public ICargoOwner,
                                     public IAimOffsetProvider, public IShieldOwner
{
	GENERATED_BODY()

	// To not bloat public interface with functions that are not relevant to more than one other class.
	friend class ACPPTurretsMaster;
	friend class AAITankMaster;
	// For movement orders on the AI controller directly.
	friend class UTankAimAbilityComponent;
	friend class UTurretSwapComp;
	friend struct FMissionTowTeamWeaponSpawnState;
	friend class RTS_SURVIVAL_API UBehVehicleStunned;
	friend class UShieldComponent;

public:
	ATankMaster(const FObjectInitializer& ObjectInitializer);

	virtual UShieldComponent* GetShield() const override { return M_ShieldComponent.Get(); }

	// ---- Vehicle modules ----

	/**
	 * @brief Runs once per committed native module batch: counts, behaviours, CrewRepair, card, icons,
	 * announcements and Blueprint events. Called by this tank's UArmorCalculation only.
	 */
	void OnModuleStateBatchCommitted(const FModuleChangeBatch& Batch);

	/** @brief Re-synchronizes a module's behaviour context after its mesh binding changed. */
	void OnModuleBindingChanged(int32 SlotIndex);
	/** @brief Rebinds active mounted effects when a turret or hull weapon is installed. */
	void RefreshMountedModuleBehaviours();

	/**
	 * @brief Receipt of one accepted ordinary healing transaction; picks the single module repair
	 * milestone of this heal (full service, red -> yellow recovery or nothing).
	 */
	void OnHealthHealingApplied(const FHealthHealingReceipt& Receipt);

	/** @return True when hull health and every installed module are at maximum. */
	UFUNCTION(BlueprintPure, Category = "Vehicle Modules")
	bool GetIsVehicleFullyRepaired() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle Modules")
	UArmorCalculation* GetVehicleModuleArmor() const { return M_ModuleArmor.Get(); }

	/** @return Total crew seconds still needed for every red module, including the current target. */
	UFUNCTION(BlueprintPure, Category = "Vehicle Modules")
	float GetRemainingCrewRepairSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Vehicle Modules")
	int32 GetCrewRepairTargetModuleId() const { return M_CrewRepairState.CurrentModuleId; }

	/**
	 * @brief Lets tank Blueprints react once to committed module state transitions.
	 * @param ModuleType Type of the instance whose state changed.
	 * @param NewState Committed Healthy, Damaged or Destroyed state.
	 * @param RemainingModuleHp Absolute remaining module HP, clamped to [0, MaxHp].
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle Modules")
	void OnVehicleModuleStateChanged(
		EVehicleModuleTypes ModuleType,
		EVehicleModuleState NewState,
		float RemainingModuleHp);

	/** @brief Module health fractions, coverage, finishing work and roll serial for tactical persistence. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Modules")
	FVehicleModuleSaveData ExportVehicleModuleSaveData() const;

	/**
	 * @brief Applies saved module state after module finalization; never resumes crew work or replays events.
	 * @param SaveData Data exported from a tank with the same module setup.
	 * @return True if the data matched this tank and was applied, or was queued until the turrets installed
	 * their modules (a queued load that then mismatches reports an error).
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle Modules")
	bool ImportVehicleModuleSaveData(const FVehicleModuleSaveData& SaveData);

	virtual bool GetIsCrewRepairActive() const override;

	// ---- Mounted weapon lock shared by CrewRepair, stuns and module behaviours ----

	/** @brief Disables every turret and hull weapon until all lock sources released their lock. */
	void AcquireMountedWeaponLock(UObject* Source);

	/**
	 * @brief Releases one lock source; weapons auto-engage again only when no other source holds a lock.
	 * @param Source The object that acquired the lock.
	 * @param bUseLastTargetOnRestore Forwarded to auto-engage when this was the last lock.
	 */
	void ReleaseMountedWeaponLock(UObject* Source, bool bUseLastTargetOnRestore);

	bool GetHasMountedWeaponLock() const;

	// ---- Mobility restrictions applied by behaviours (e.g. damaged running gear or engine) ----

	/**
	 * @brief Registers or replaces one source's mobility restriction; sources combine by minimum, not product.
	 * @param Source Restricting object, e.g. a vehicle module behaviour.
	 * @param TravelSpeedMultiplier Allowed fraction of travel speed; 0 removes powered travel.
	 * @param TurnRateMultiplier Allowed fraction of path-following turning; 0 removes powered turning.
	 * @param AccelerationMultiplier Allowed fraction of tracked drive acceleration.
	 */
	void SetMobilityRestriction(UObject* Source, float TravelSpeedMultiplier, float TurnRateMultiplier,
	                            float AccelerationMultiplier = 1.f);

	/** @brief Removes only this source's restriction; the remaining sources are recomposed. */
	void ClearMobilityRestriction(const UObject* Source);
	
	void SetAudioCompsDisabled(const bool bDisable);

	virtual void PropagateNewAggroStance(const ERTSAggroBehaviour NewStance) final;
	virtual void PropagateNewTargetPreference(const ETargetPreference TargetPreference) final;
	ETargetPreference GetTargetPreference();
	void SetTargetPreferenceForAllWeapons(const ETargetPreference NewPreference);

	// virtual because TargetAcquistion component on tracked tank
	virtual void SetAggroStance(const ERTSAggroBehaviour NewStance);
	// virtual because TargetAcquistion component on tracked tank
	virtual ERTSAggroBehaviour GetEngagementStance() const;

	// Controller is set with OnPosses on AITankMaster.
	void SetAIController(AAITankMaster* NewController) { AITankController = NewController; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="TankMaster")
	inline AAITankMaster* GetAIController() const { return AITankController; };

	UFUNCTION(BlueprintCallable)
	virtual USkeletalMeshComponent* GetTankMesh() const;

	float GetTurnRate() const { return TurnRate; }


	/** @param NewTurnRate The new turn rate used on this vehicle and the track physics movement. */
	virtual void UpgradeTurnRate(const float NewTurnRate);


	inline float GetTankCornerOffset() const { return M_CornerOffset; }

	/**
	 * Propagates the amount of health after reaching a certain threshold.
	 * @param PercentageLeft Enum level of percentage health that is left.
	 * @param bIsHealing
	 */
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override;

	inline TArray<ACPPTurretsMaster*> GetTurrets() const { return Turrets; };
	inline TArray<UHullWeaponComponent*> GetHullWeapons() const { return HullWeapons; }
	float GetVehicleHighestWeaponRange() const;

	virtual ERTSNavAgents GetRTSNavAgentType() const override { return NavAgentType; }

	// -----Start overwrite cargo interface-----
	virtual void OnSquadRegistered(ASquadController* SquadController) override;
	virtual void OnCargoEmpty() override;
	// -----End overwrite cargo interface-----

	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;
	virtual void ExecuteDetachTowCommand() override;
	virtual void OnActorBeingTowed(AActor* TowingVehicle, UVehicleTowComponent* TowComp) override;

	// For mission programming.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ChangeAbilityCooldown(const EAbilityID AbilityId, const float NewCooldown, const int32 Subtype);

	/** @brief Restores turret and hull weapon auto-engage after temporary aircraft transport disabling. */
	void EnableWeaponsAfterAircraftDrop();
	
	UFUNCTION(BlueprintCallable, Category="Turrets")
	void SetTurretsDisabled();
	

protected:
	virtual void Tick(float DeltaSeconds) override;

	virtual void BeginPlay() override;
	virtual void CheckForUpgrades();

	virtual void BeginDestroy() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnitDies(const ERTSDeathType DeathType) override;
	virtual bool GetShouldIgnoreCommandCompletion(EAbilityID AbilityFinished) override;
	void CheckIfUpsideDown();

	virtual void PostInitializeComponents() override;

	/**
	 * @brief Set up the action this tank should perform at the start of the game.
	 * @param TargetActor Optional target actor for actor-driven abilities.
	 * @param TargetLocation Location to use for location-driven abilities.
	 * @param EndRotation
	 * @param StartGameAbility Ability to execute on the next frame after begin play.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetTankStartGameAction(AActor* TargetActor, const FVector TargetLocation, const FRotator EndRotation,
	                            const EAbilityID StartGameAbility);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="AimOffset")
	TArray<FVector> AimOffsetPoints = {
		FVector(0, 0, 100), FVector(-50, 0, 100),
		FVector(0, 25, 100), FVector(0, -25, 100)
	};

	// Defines which version of ANavData (Different instances of RecastNavMesh) this pawn can use.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ERTSNavAgents NavAgentType = ERTSNavAgents::LightTank;
	bool bM_AttackGroundActive;
	FVector M_AttackGroundLocation;

	UFUNCTION(BlueprintCallable, Category="Turrets")
	void SetTurretsToAutoEngage(const bool bUseLastTarget);


	// The turrets mounted on this tank.
	UPROPERTY()
	TArray<ACPPTurretsMaster*> Turrets;

	// The Hull Weapons mounted on this tank.
	UPROPERTY()
	TArray<UHullWeaponComponent*> HullWeapons;

	/**
	 * @brief Adds the provided turret to the array keeping track of all turrets on this tank.
	 * Turret and Weapon vehicle modules are not set up here; turrets install those themselves after BeginPlay.
	 */
	UFUNCTION(BlueprintCallable)
	inline void SetupTurret(ACPPTurretsMaster* NewTurret)
	{
		Turrets.Add(NewTurret);
		if (NewTurret)
		{
			NewTurret->OnSetupTurret(this);
			// Newly spawned or swapped turrets inherit an active weapon lock.
			if (GetHasMountedWeaponLock())
			{
				NewTurret->DisableTurret();
			}
			RefreshMountedModuleBehaviours();
		}
	}

	void RemoveTurret(ACPPTurretsMaster* TurretToRemove);

	/** @brief Adds the provided HullWeapon to the array keeping track of all HullWeapons on this tank. */
	UFUNCTION(BlueprintCallable)
	void SetupHullWeapon(UHullWeaponComponent* NewHullWeapon);


	/**
	 * @brief Sets up the collision for the mesh attached to the tracks.
	 * Will only interact with traces for visibility.
	 * @param MeshToSetup The mesh to setup the collision for.
	 * @param bIsHarvester
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupCollisionForMeshAttachedToTracks(UMeshComponent* MeshToSetup, const bool bIsHarvester = false) const;

	// Used to affect navmesh when stationary.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Components")
	URTSNavCollision* RTSNavCollision;


	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="Reference")
	UBehaviourComp* BehaviourComponent;

	UPROPERTY(EditDefaultsOnly, Category="Behaviour")
	TArray<TSubclassOf<UBehaviour>> OnMovementBehaviour;

	bool GetIsValidBehaviourComponent() const;
	/**
	 * @brief the AIController of this tank
	 * contains behaviour tree logic, like tank movement
	 */
	UPROPERTY()
	AAITankMaster* AITankController;

	// Called when the last command was terminated and the unit has no new commands in the queue.
	virtual void OnUnitIdleAndNoNewCommands() override;

	/**
	 * @brief Implemented in derived blueprint to run a BT on the AI.
	 * @pre TargetLocation is set in the AIController.
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteCommandMoveBP(const bool Reverse);

	/** @brief Orders a movement command towards the TargetLocation */
	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteAttackMoveBP(const FVector TargetLocation);


	// COMMAND INTERFACE OVERWRITES ------------------------------------------------------------------------------------

	virtual void ExecuteStopCommand() override;
	virtual void OnCommandExecutionStarting(EAbilityID AbilityStarting) override;

	/** @copydoc ICommands::ExecuteAttackCommand
	 * Sets all Turrets to engage the specified target.
	 */
	virtual void ExecuteAttackCommand(AActor* Target) override;

	/** @copydoc ICommands::ExecuteMoveCommand
	 * Moves to the specified location.
	 * Sets all Turrets to engage freely.
	 */
	virtual void ExecuteMoveCommand(const FVector MoveToLocation) override;

	/** @copydoc ICommands::TerminateAttackCommand
	 * Sets all turrets to freely search for targets.
	 */
	virtual void TerminateAttackCommand() override;

	/** @copydoc ICommands::TerminateMoveCommand */
	virtual void TerminateMoveCommand() override;

	virtual void ExecuteReverseCommand(const FVector ReverseToLocation) override;
	virtual void TerminateReverseCommand() override;


	virtual void ExecuteAttackGroundCommand(const FVector GroundLocation) override;
	virtual void TerminateAttackGroundCommand() override;

	virtual void ExecuteAimAbilityCommand(const FVector TargetLocation, const EAimAbilityType AimAbilityType) override;
	virtual void TerminateAimAbilityCommand(const EAimAbilityType AimAbilityType) override;
	virtual void ExecuteAttachedWeaponAbilityCommand(const FVector TargetLocation,
	                                                 const EAttachWeaponAbilitySubType
	                                                 AttachedWeaponAbilityType) override;
	virtual void
	TerminateAttachedWeaponAbilityCommand(const EAttachWeaponAbilitySubType AttachedWeaponAbilityType) override;
	virtual void ExecuteTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType) override;
	virtual void TerminateTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType) override;
	virtual void ExecuteCancelAimAbilityCommand(const EAimAbilityType AimAbilityType) override;
	virtual void TerminateCancelAimAbilityCommand(const EAimAbilityType AimAbilityType) override;

	UFUNCTION(BlueprintImplementableEvent, Category="Commands")
	void BP_ExecuteTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType);

	UFUNCTION(BlueprintImplementableEvent, Category="Commands")
	void BP_TerminateTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType);

	virtual void SetUnitToIdleSpecificLogic() override;

	/** @copydoc ICommands::StopBehaviourTree()
	 * Is the same for all derived classes (they used derive AAITankMaster controllers.
	 */
	virtual void StopBehaviourTree() override final;

	/** @copydoc ICommands::TerminateAttackCommand */
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand) override;
	virtual void TerminateRotateTowardsCommand() override;

	virtual UHarvester* GetIsHarvester() override final;
	// Start the Behaviour tree in blueprints.
	virtual void ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource) override;
	virtual void TerminateHarvestResourceCommand() override;

	virtual void ExecuteReturnCargoCommand() override final;
	virtual void TerminateReturnCargoCommand() override final;
	virtual void ExecuteExitCargoCommand() override final;

	virtual void ExecuteTowActorCommand(AActor* TowTargetActor, const ETowedActorTarget TowSubtype) override;
	virtual void TerminateTowActorCommand() override;
	virtual void TerminateDetachTowCommand() override;
	virtual void ExecuteCrewRepairCommand(ECrewRepairAbilityType Subtype) override;
	virtual void TerminateCrewRepairCommand(ECrewRepairAbilityType Subtype) override;
	bool TryTowTeamWeaponInstant(class ATeamWeaponController* TeamWeaponController, class ATeamWeapon* TeamWeaponActor);

	// For the harvester that uses resources that are stored in the harvester component.
	// This function is used to adust the visuals of the harvester depending on the amount of resources stored.
	virtual void OnResourceStorageChanged(int32 PercentageResourcesLeft, const ERTSResourceType ResourceType) override;
	virtual void OnResourceStorageEmpty() override;

	UFUNCTION(BlueprintImplementableEvent, Category="Harvester")
	void OnResourceStorageChangedBP(int32 PercentageResourcesLeft, const ERTSResourceType ResourceType);

	// How fast this tank turns, used in derived blueprints for navigation logic.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	float TurnRate = 30;

	// Called when the tank completed a rotation command which was not issued from the command queue but somewhere else.
	virtual void OnFinishedStandaloneRotation();

	// Stops any current rotation logic being executed.
	void StopRotating();

	/**
	 * @brief ITurretOwner function: to get the player for the turret
	 * @return The number of the player that owns this tank.
	 * @note This is an ITurretOwner function.
	 */
	virtual int GetOwningPlayer() override final;

	/**
	 * @brief Moves the tank closer to the TurretTarget
	 * If the tank is not ordered directly by the player to move somewhere else.
	 * @param TargetLocation The location of the target the turret is aiming at.
	 * @param CallingTurret The turret that is out of range.
	 * @note This is an ITurretOwner function.
	 */
	virtual void OnTurretOutOfRange(
		const FVector TargetLocation,
		ACPPTurretsMaster* CallingTurret) override;

	/**
	 * @brief Stops the tank movement towards the previous target location
	 * if the current ability is either idle or attack.
	 * @param CallingTurret The turret that is in range.
     * @note This is an ITurretOwner function. 
	 */
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override ;
	virtual void OnHullWeaponOutOfRange(
		const FVector TargetLocation,
		UHullWeaponComponent* CallingHullWeapon) override;
	virtual void OnHullWeaponInRange(UHullWeaponComponent* CallingHullWeapon) override;
	
	virtual void OnCancelMovementToGetInRangeOfTurret();

	/**
	 * @brief Called when a turret killed the target given.
	 * Executes the next Command in the queue.
	 * @param CallingTurret The turret that destroyed its target.
	 * @param CallingHullWeapon
	 * @param DestroyedActor The actor that was destroyed. may be null.
	 * @param bWasDestroyedByOwnWeapons
	 * @note This is an ITurretOwner function.
	 */
	virtual void OnMountedWeaponTargetDestroyed(
		ACPPTurretsMaster* CallingTurret,
		UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor,
		const bool bWasDestroyedByOwnWeapons) override final;

	virtual void OnFireWeapon(ACPPTurretsMaster* CallingTurret) override;
	virtual void OnProjectileHit(const bool bBounced) override;

	virtual void OnTankKilledAnyActor(AActor* KilledActor);

	/**
	 * Called by turret to be able to rotate the turret back to the base position.
	 * @return The world rotation of the tank.
	 * @note This is an ITurretOwner function.
	 */
	virtual FRotator GetOwnerRotation() const override;

	/**
	 * Set the offset used by the ai controller when path finding.
	 * @param Offset The offset the tank will use to offset from corners.
	 */
	void SetTankCornerOffset(const float Offset) { M_CornerOffset = Offset; }


	bool CheckTurretIsValid(ACPPTurretsMaster* TurretToCheck) const;

	/**
	 * @return Whether the AIcontroler is valid.
	 * @note Will attempt to repair the reference if invalid.
	 */
	bool GetIsValidAIController();

	/**
	 * @brief Does proper error reporting if the RTSNavCollision component is not valid.
	 * @return Whether the RTSNavCollision is valid.
	 */
	bool GetIsValidRTSNavCollision() const;

	bool bWasLastMovementReverse = false;


	bool GetIsValidHullWeapon(const UHullWeaponComponent* HullWeapon) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	USpatialVoiceLinePlayer* GetSpatialVoiceLinePlayer() const { return M_SpatialVoiceLinePlayer; }

	bool GetIsValidSpatialVoiceLinePlayer() const;

	virtual EAnnouncerVoiceLineType OverrideAnnouncerDeathVoiceLine(const EAnnouncerVoiceLineType OriginalLine) const;

	/**
	 * @brief Lets kinematic and physics-driven tanks decide completion from the motion they actually apply.
	 * @param RemainingYawDegrees Signed shortest yaw still required.
	 * @param DeltaSeconds Time available for this rotation update.
	 * @return True only when the vehicle has physically completed the requested rotation.
	 */
	virtual bool ApplyRotateTowardsStep(const float RemainingYawDegrees, const float DeltaSeconds);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FVehicleModuleTestAccess;
#endif

	// ---- Vehicle module integration ----
	void BeginPlay_InitVehicleModules();
	void BeginPlay_InitVehicleModuleBindings();
	bool GetIsValidModuleArmor() const;
	void RefreshModuleRepairCounts();
	void SyncModuleBehaviour(const FModuleStateChange& Change);
	void SyncModuleBehaviourForSlot(int32 SlotIndex);
	void OnModuleBehavioursApplied();
	void PublishModuleIconChanges(const FModuleChangeBatch& Batch) const;
	void PublishModuleBlueprintEvents(const FModuleChangeBatch& Batch);
	void AnnounceModuleTransitions(const FModuleChangeBatch& Batch);
	void OnTankMaxHealthChanged(float OldMaxHealth, float NewMaxHealth);
	void CleanupVehicleModuleBindings();

	// ---- Turret-driven module registration (called by ACPPTurretsMaster only) ----
	void AddPendingTurretModuleRegistration(ACPPTurretsMaster* Turret);
	void OnTurretReadyForModuleRegistration(ACPPTurretsMaster* Turret);
	void CancelTurretModuleRegistration(const ACPPTurretsMaster* Turret);
	bool GetHasUnprocessedTurretModuleRegistrations();
	// Non-const: the engine's AActor::IsUnitAlive is non-const.
	bool GetCanInstallTurretModules();
	void ProcessReadyTurretModuleRegistrations();
	TArray<ACPPTurretsMaster*> TakeReadyTurretsInMountOrder();
	void RegisterTurretModulesInMountOrder(const TArray<ACPPTurretsMaster*>& TurretsInMountOrder);
	bool RegisterModulesForTurret(ACPPTurretsMaster* Turret);
	void EnsureTurretModuleForMesh(UMeshComponent* TurretMesh);
	void AdoptDesignerWeaponModule(const ACPPTurretsMaster* Turret, UMeshComponent* TurretMesh);
	void UpdateAutoWeaponModule();
	bool FindLargestRegisteredTurretWeapon(FTankAutoWeaponModuleBinding& OutCandidate) const;
	bool GetIsAutoWeaponModuleBindingCurrent() const;
	void ApplyDeferredVehicleModuleSaveData();

	UPROPERTY()
	FTankTurretModuleRegistrationState M_TurretModuleRegistration;

	// ---- Ordinary healing milestones ----
	bool TryRecoverDestroyedModulesAfterHealing();
	// @return True when the finishing work is complete; performs no armor mutation.
	bool AccumulateFullModuleService(float Work);
	float GetCurrentFinishingWork() const;

	// ---- CrewRepair ----
	void InitializeCrewRepairAbilitySlot();
	void RefreshCrewRepairAbilityFromModuleState();
	FUnitAbilityEntry MakeCrewRepairAbilityEntry(ECrewRepairAbilityType Subtype) const;
	bool BeginCrewRepair();
	void AcquireCrewRepairRestrictions(UCommandData& CommandData);
	void StopVehicleForCrewRepair();
	bool StartCrewRepairTimer();
	void CrewRepairTick(uint32 SessionGeneration);
	bool SelectNextRedModuleForCrewRepair();
	void UpdateCrewRepairAfterModuleBatch();
	void FinishCrewRepair(ECrewRepairStopReason Reason);
	void RestoreCrewRepairState();

	UPROPERTY()
	TWeakObjectPtr<UArmorCalculation> M_ModuleArmor;

	// Shared module asset cache; weak because the game instance owns it.
	UPROPERTY()
	TWeakObjectPtr<UVehicleModuleSubsystem> M_VehicleModuleSubsystem;

	FVehicleModuleRepairState M_ModuleRepairState;

	FCrewRepairState M_CrewRepairState;

	// True once the armor component finalized module HP for this tank.
	bool bM_AreVehicleModulesInitialized = false;

	// Blueprint transition events waiting for deferred module behaviour changes to be committed.
	FModuleChangeBatch M_DeferredModuleBlueprintEvents;

	FDelegateHandle M_MaxHealthChangedHandle;
	FDelegateHandle M_ModuleBehavioursAppliedHandle;

	// Game time of the last module announcement; announcements never delay icon changes.
	double M_LastModuleAnnouncementTime = -1.0;

	// Sources that currently disable all mounted weapons (crew repair, stun, ...).
	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> M_MountedWeaponLockSources;

	// Behaviour mobility restrictions; recomposed on change so removing one never restores a stale base value.
	UPROPERTY()
	TArray<FTankMobilityRestriction> M_MobilityRestrictions;

	void RebuildVehicleMobility();

	void ClearShieldComponentCache(const UShieldComponent* ShieldComponent);

	// Whether the vehicle is currently Turning.
	bool bM_NeedToTurnTowardsTarget;

	// Whether the rotation command is from queue or standalone.
	bool bM_IsRotationAQueueCommand;

	// The direction the mesh needs to face.
	FRotator M_RotateToDirection;
	
	void BeginPlay_SetupCollisionVsBuildings();
	void BeginPlay_SetFactionFlagPrimitiveDataIndex();

	// the derived bp has already set the correct tank subtype before c++ begin play.
	void BeginPlay_SetupData();
	void BeginPlay_SetupData_Resistances(const FResistanceAndDamageReductionData& ResistanceData,
	                                     const float MaxHealth) const;


	void BeginPlay_DetermineMainWeapon();
	void PostInitializeComponents_SetupVehicleFireFeedbackOptimizationLink() const;

	bool GetIsValidVehicleTowComponent() const;
	bool GetIsValidTowedActorComponent() const;

	void ExecuteTowActorCommand_TowVehicle(AActor* TowTargetActor, UTowedActorComponent* TowedActorComponent);
	void ExecuteTowActorCommand_TowTeamWeapon(class ATeamWeapon* TeamWeaponActor,
	                                          class ATeamWeaponController* TeamWeaponController,
	                                          UTowedActorComponent* TowedActorComponent);
	bool TryTowTeamWeapon_Internal(class ATeamWeapon* TeamWeaponActor,
	                               class ATeamWeaponController* TeamWeaponController,
	                               UTowedActorComponent* TowedActorComponent,
	                               const bool bDoneExecutingCommandAfterTow);
	bool ExecuteTowActorCommand_GetShouldQueueMoveThenRetry(const AActor* TowTargetActor,
	                                                        const UVehicleTowComponent* VehicleTowComp) const;
	bool ExecuteDetachTowCommand_TryDetachSelfFromTow();
	void ExecuteDetachTowCommand_DetachTowedVehicle(AActor* TowedActor, UTowedActorComponent* TowedActorComponent);
	void ExecuteDetachTowCommand_DetachTowedTeamWeapon(class ATeamWeaponController* TeamWeaponController);
	void CleanupTowRelationshipsOnDeath();
	bool GetTowTargetData(AActor* TowTargetActor, const ETowedActorTarget TowSubtype,
	                      UTowedActorComponent*& OutTowedActorComponent,
	                      class ATeamWeapon*& OutTeamWeaponActor,
	                      class ATeamWeaponController*& OutTeamWeaponController) const;
	bool GetIsValidVehicleTowComponentNoReport() const;
	bool GetIsValidTowedActorComponentNoReport() const;

	// The actor targeted by this tank master.
	UPROPERTY()
	AActor* M_TargetActor;

	// Couples turret range notifications to one exact auxiliary controller request and target identity.
	FTankTurretRangePursuitState M_TurretRangePursuit;
	uint64 M_TurretRangePursuitSerialCounter = 0;
	FTimerHandle M_TurretRangeMovementInvariantTimerHandle;
	double M_TurretRangeOrphanStartTimeSeconds = -1.0;

	UPROPERTY()
	USpatialVoiceLinePlayer* M_SpatialVoiceLinePlayer;

	UPROPERTY()
	TObjectPtr<URTSOptimizer> M_OptimizationComponent;


	// Offset from corners used when path finding.
	UPROPERTY()
	float M_CornerOffset;

	/**
	 * Attempts to repair the AIController reference by checking for a controller, if there is non
	 * we will spawn a new controller for this pawn.
	 * @return Whether the AIController reference is valid. 
	 */
	bool RepairAIControllerReference();

	// Set at post init components, can be null.
	// Only implemented if derived blueprint has an attached harvester component.
	UPROPERTY()
	TObjectPtr<UHarvester> M_HarvesterComponent;

	// Optional component used by specific tank blueprints for hull fire feedback.
	UPROPERTY()
	TWeakObjectPtr<UVehicleFireFeedbackComponent> M_VehicleFireFeedbackComponent;

	UPROPERTY()
	TWeakObjectPtr<UVehicleTowComponent> M_VehicleTowComponent;

	UPROPERTY()
	TWeakObjectPtr<UTowedActorComponent> M_TowedActorComponent;

	// Optional shield is found once so weapon systems can use IShieldOwner without component searches.
	UPROPERTY()
	TObjectPtr<UShieldComponent> M_ShieldComponent = nullptr;

	// For adjusting the rotation.
	FTimerHandle TimerHandle_CheckIfUpsideDown;


	UPROPERTY()
	FTankStartGameAction M_TankStartGameAction;

	/** @return Whether the current active ability of the tank allows for the turret to take control */
	bool GetCanTurretTakeControl() const;
	bool GetDoesWeaponOwnCurrentPursuit(const UObject* CallingWeapon) const;
	/**
	 * @brief Keeps one mounted weapon responsible for range-closing so competing weapons cannot cancel each other.
	 * @param TargetLocation The current location the vehicle must approach.
	 * @param CallingWeapon The mounted weapon requesting range-closing ownership.
	 * @param TargetActor The actor being pursued, if the weapon has an actor target.
	 */
	void HandleMountedWeaponOutOfRange(
		const FVector& TargetLocation,
		UObject* CallingWeapon,
		AActor* TargetActor);
	void HandleMountedWeaponInRange(UObject* CallingWeapon);
	void StartTurretRangeMovementInvariantMonitoring();
	void StopTurretRangeMovementInvariantMonitoring();
	void CheckTurretRangeMovementInvariant();
	void ResetTurretRangeMovementOrphanGracePeriod();
	/**
	 * @brief Replaces only the request owned by the same weapon and binds its exact request identity.
	 * @param TargetLocation The current destination for the range-closing request.
	 * @param CallingWeapon The mounted weapon that owns the request.
	 * @param TargetActor The actor being pursued, if present.
	 * @param CurrentTimeSeconds The issue time used to throttle request replacement.
	 */
	void BeginTurretRangeMovementRequest(
		const FVector& TargetLocation,
		UObject* CallingWeapon,
		AActor* TargetActor,
		double CurrentTimeSeconds);
	/**
	 * @brief Accepts a terminal result only while the same pursuit generation still owns the request.
	 * @param TurretRangePursuitSerial The pursuit generation associated with the completed request.
	 * @param MovementResultCode The path-following terminal result.
	 */
	void OnTurretRangeMovementRequestFinished(
		uint64 TurretRangePursuitSerial,
		EPathFollowingResult::Type MovementResultCode);
	/**
	 * @brief Centralizes cleanup while preserving nav state during an intentional movement handoff.
	 * @param EndMode Whether cleanup stops locomotion or hands it to a queued movement command.
	 * @param NextStatus The bounded post-cleanup state, normally none or failure backoff.
	 */
	void EndTurretRangeMovement(
		ETurretRangeMovementEndMode EndMode,
		ETurretRangePursuitStatus NextStatus = ETurretRangePursuitStatus::None);

	// Plays a little after beginplay to set max vehicle speed on the vehicle movement component.
	FTimerHandle TimerHandle_SetupMaxSpeed;

	void SetMaxSpeedOnVehicleMovementComponent();

	void OnTurretKilledActor(ACPPTurretsMaster* CallingTurret, AActor* DestroyedActor);
	void OnHullWeaponKilledActor(UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor);

	void OnUnitDies_DisableWeapons();
	void OnUnitDies_CheckForCargo(const ERTSDeathType DeathType) const;
	void OnUnitDies_AnnouncerDeathVoiceLine() const;
	virtual void OnRotateTowardsFinished();

	void ApplyMovementBehaviours();
	void RemoveMovementBehaviours();
};
