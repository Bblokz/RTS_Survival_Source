// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Resources/Harvester/HarvesterInterface/HarvesterInterface.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoOwner/CargoOwner.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/Turret/TurretOwner/TurretOwner.h"
#include "TankMaster.generated.h"

class URTSOptimizer;
class UDigInComponent;
class USpatialVoiceLinePlayer;
class ACPPResourceMaster;
class UArmor;
class URTSNavCollision;
// Forward Declaration; reduce compile time; guard against cyclic dependencies.
class RTS_SURVIVAL_API AAITankMaster;
class RTS_SURVIVAL_API ACPPTurretsMaster;

USTRUCT()
struct FTankStartGameAction
{
        GENERATED_BODY()

        FTankStartGameAction();

        void InitStartGameAction(const EAbilityID InAbilityID,
                                 AActor* InTargetActor,
                                 const FVector& InTargetLocation,
                                 ATankMaster* InTankMaster);

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
                                     public IAimOffsetProvider
{
	GENERATED_BODY()

	// To not bloat public interface with functions that are not relevant to more than one other class.
	friend class ACPPTurretsMaster;
	friend class AAITankMaster;

public:
	ATankMaster(const FObjectInitializer& ObjectInitializer);

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

	inline TArray<UArmor*> GetTankArmor() const { return M_TankArmor; }

	virtual ERTSNavAgents GetRTSNavAgentType() const override { return NavAgentType; }

	// -----Start overwrite cargo interface-----
	virtual void OnSquadRegistered(ASquadController* SquadController) override;
	virtual void OnCargoEmpty() override;
	// -----End overwrite cargo interface-----

	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;
protected:
        virtual void Tick(float DeltaSeconds) override;

        virtual void BeginPlay() override;

        virtual void BeginDestroy() override;
        virtual void UnitDies(const ERTSDeathType DeathType) override;
        void CheckIfUpsideDown();

        virtual void PostInitializeComponents() override;

        /**
         * @brief Set up the action this tank should perform at the start of the game.
         * @param TargetActor Optional target actor for actor-driven abilities.
         * @param TargetLocation Location to use for location-driven abilities.
         * @param StartGameAbility Ability to execute on the next frame after begin play.
         */
        UFUNCTION(BlueprintCallable, NotBlueprintable)
        void SetTankStartGameAction(AActor* TargetActor, const FVector TargetLocation, const EAbilityID StartGameAbility);

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

	UFUNCTION(BlueprintCallable, Category="Turrets")
	void SetTurretsDisabled();

	// The turrets mounted on this tank.
	UPROPERTY()
	TArray<ACPPTurretsMaster*> Turrets;

	// The Hull Weapons mounted on this tank.
	UPROPERTY()
	TArray<UHullWeaponComponent*> HullWeapons;

	/** @brief Adds the provided turret to the array keeping track of all turrets on this tank. */
	UFUNCTION(BlueprintCallable)
	inline void SetupTurret(ACPPTurretsMaster* NewTurret)
	{
		Turrets.Add(NewTurret);
		if (NewTurret)
		{
			NewTurret->OnSetupTurret(this);
		}
	}

	/** @brief Adds the provided HullWeapon to the array keeping track of all HullWeapons on this tank. */
	UFUNCTION(BlueprintCallable)
	inline void SetupHullWeapon(UHullWeaponComponent* NewHullWeapon) { HullWeapons.Add(NewHullWeapon); }

	/**
	 * @brief Setup the armor of this tank by providing all the armor components.
	 * @param MyArmorSetup The array of armor components.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ChildBpSetupArmor(TArray<UArmor*> MyArmorSetup);

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

	bool GetIsValidBehaviourComponent()const;
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
	virtual void OnTurretInRange(ACPPTurretsMaster* CallingTurret) override;

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

private:
	// Whether the vehicle is currently Turning.
	bool bM_NeedToTurnTowardsTarget;

	// Whether the rotation command is from queue or standalone.
	bool bM_IsRotationAQueueCommand;

	// The direction the mesh needs to face.
	FRotator M_RotateToDirection;

	void BeginPlay_SetupCollisionVsBuildings();

	// the derived bp has already set the correct tank subtype before c++ begin play.
	void BeginPlay_SetupData();
	void BeginPlay_SetupData_Resistances(const FResistanceAndDamageReductionData& ResistanceData,
	                                     const float MaxHealth) const;


	void BeginPlay_DetermineMainWeapon();

	// The actor targeted by this tank master.
	UPROPERTY()
	AActor* M_TargetActor;

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

	// For adjusting the rotation.
	FTimerHandle TimerHandle_CheckIfUpsideDown;

        // Contains all the armor components that are setup in blueprints.
        UPROPERTY()
        TArray<UArmor*> M_TankArmor;

        UPROPERTY()
        FTankStartGameAction M_TankStartGameAction;

        /** @return Whether the current active ability of the tank allows for the turret to take control */
        bool GetCanTurretTakeControl() const;

        // Plays a little after beginplay to set max vehicle speed on the vehicle movement component.
        FTimerHandle TimerHandle_SetupMaxSpeed;

	void SetMaxSpeedOnVehicleMovementComponent();

	void OnTurretKilledActor(ACPPTurretsMaster* CallingTurret, AActor* DestroyedActor);
	void OnHullWeaponKilledActor(UHullWeaponComponent* CallingHullWeapon, AActor* DestroyedActor);

	void OnUnitDies_DisableWeapons();
	void OnUnitDies_CheckForCargo(const ERTSDeathType DeathType) const;
	void OnUnitDies_AnnouncerDeathVoiceLine()const;
};
