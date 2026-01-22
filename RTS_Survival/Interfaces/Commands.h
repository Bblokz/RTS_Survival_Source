// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "UObject/Interface.h"
#include "Commands.generated.h"

class UActionUIManager;
class ACPPResourceMaster;
class AItemsMaster;
class UHarvester;
class UResourceComponent;
enum class EModeAbilityType : uint8;
// Forward declaration.
class RTS_SURVIVAL_API ICommands;


UENUM(BlueprintType)
enum class ECommandQueueError : uint8
{
	NoError,
	QueueFull,
	QueueInactive,
	QueueHasPatrol,
	AbilityNotAllowed,
	CommandDataInvalid,
	AbilityOnCooldown,
};

/**
 * Contains all possible data for any command in the queue.
 */
USTRUCT()
struct FQueueCommand
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EAbilityID CommandType;

	UPROPERTY()
	FVector TargetLocation;

	UPROPERTY()
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY()
	FRotator TargetRotator;

	UPROPERTY()
	int32 CustomType;

	static constexpr int32 DefaultCustomType = 0;

	EBehaviourAbilityType GetBehaviourAbilitySubtype() const
	{
		return static_cast<EBehaviourAbilityType>(CustomType);
	}

	EModeAbilityType GetModeAbilitySubtype() const
	{
		return static_cast<EModeAbilityType>(CustomType);
	}

	EFieldConstructionType GetFieldConstructionSubtype() const
	{
		return static_cast<EFieldConstructionType>(CustomType);
	}

	EGrenadeAbilityType GetGrenadeAbilitySubtype() const
	{
		return static_cast<EGrenadeAbilityType>(CustomType);
	}

	EAimAbilityType GetAimAbilitySubtype() const
	{
		return static_cast<EAimAbilityType>(CustomType);
	}

	FQueueCommand()
		: CommandType(EAbilityID::IdNoAbility)
		  , TargetLocation(FVector::ZeroVector)
		  , TargetActor(nullptr)
		  , TargetRotator(FRotator::ZeroRotator)
		  , CustomType(DefaultCustomType)
	{
	}
};

/**
* @brief: Commands are added using AddAbilityToCommands(EAbilityID) if m_bStopAddingCommands is false
* then the ability will be added to this array.
*
* If this is the first ability added:w
* ExecuteNexCommand(executeCurrenCommand = false) is used which increments the index and
* executes the command on that index.
* 
* In blueprint after a Execute[CommandName] was completed DoneExecutingCommand is called.
* this function calls, ExecuteNextCommand(executeCurrentCommand = false) to execute the next ability
*
* @note: ---------ON Disabling Command ACTIVITY--------- 
* The command queue can be set to no longer accept new commands by calling SetCommandQueueEnabled(false).
* This means that all the public functions used to add a specific command to the queue will return false.
* This can be undone by calling SetCommandQueueEnabled(true) which will allow the unit to accept new commands again.
* This also clears the command queue!
* 
* @note: ---------ON Patrol in queue Command ---------
* @note On non-shift commands issued; if patrol is found in the queue then we disregard the command and do not add it.
* 
* @note: ---------ON INDICES INITIALIZATION---------
* @note Indices start at -1 after a reset.
*
* @note: ---------ON Abilities and ActionButtons---------
* @note If a direct ActionButton is pressed and shift is held, then the ability is added to the list like regular
* abilities. If no shift is held then Commands is reset and the ability added, making it the first ability to be
* executed. 
*/
UCLASS()
class RTS_SURVIVAL_API UCommandData : public UObject
{
	GENERATED_BODY()

	friend class ICommands;

public:
	UCommandData(const FObjectInitializer& ObjectInitializer);

	virtual void BeginDestroy() override;

	const TArray<FUnitAbilityEntry>& GetAbilities() const { return M_Abilities; }

	TArray<EAbilityID> GetAbilityIds(const bool bExcludeNoAbility = false) const;

	void SetAbilities(const TArray<FUnitAbilityEntry>& Abilities);

	bool SwapAbility(const EAbilityID OldAbility, const EAbilityID NewAbility);
	bool SwapAbility(const EAbilityID OldAbility, const FUnitAbilityEntry& NewAbility);

	/**
	 *
	 * @param NewAbility Ability to add.
	 * @param AtIndex Optional: provide a specific index to add the ability to.
	 * @return Whether we could add the ability to an empty slot in the array.
	 */
	bool AddAbility(const EAbilityID NewAbility, const int32 AtIndex);
	bool AddAbility(const FUnitAbilityEntry& NewAbility, const int32 AtIndex);
	/** @return Whether the ability could be successfully removed */
	bool RemoveAbility(const EAbilityID AbilityToRemove);

	/**
	 * @brief: Set up the CommandData with the owner of this CommandData.
	 * @param Owner: The owner of this CommandData struct.
	 * @note call this in the owner's constructor.
	 */
	void InitCommandData(ICommands* Owner);

	/** @return The currently active commandItem for this unit.
	 * @note returned item has EAbilityID::IdNoAbility if no command is active.
	 */
	EAbilityID GetCurrentActiveCommand() const;

	EAbilityID GetCurrentlyActiveCommandType() const;

	bool GetIsQueueFull() const;

	void UpdateActionUI();

	bool StartCooldownOnAbility(const EAbilityID AbilityID, const int32 CustomType);

private:
	// The manager that updates the ability UI for this unit.
	// If set the unit is primary selected.
	TWeakObjectPtr<UActionUIManager> M_ActionUIManager;

	const FQueueCommand* GetCurrentQueuedCommand() const;

	inline bool GetIsPrimarySelected() const { return M_ActionUIManager.IsValid(); };

	bool GetIsQueuedCommandStillAllowed(const FQueueCommand& QueuedAbility);
	bool GetIsQueuedCommandAbilityIdStillOnUnit(EAbilityID AbilityId) const;
	bool GetDoesQueuedCommandRequireSubtypeEntry(EAbilityID AbilityId) const;
	FUnitAbilityEntry* GetAbilityEntryForQueuedCommandSubtype(const FQueueCommand& QueuedCommand);
	FString GetQueuedCommandSubtypeSuffix(const FQueueCommand& QueuedCommand) const;
	bool GetIsAbilityEntryOnCooldown(EAbilityID AbilityId, const FUnitAbilityEntry* AbilityEntry,
	                                              const FString& SubtypeSuffix) const;

	void OnCommandInQueueCancelled(const FQueueCommand& CancelledCommand);
	/**
	 * Debug function that prints the queue to the screen or logs.
	 */
	void PrintCommands() const;

	/**
	 * @param CommandToCheck The EAbilityID to search for in the queue.
	 * @return Whether the queue has that command anywhere.
	 */
	bool GetHasCommandInQueue(EAbilityID CommandToCheck) const;

	/**
	 * Adds the given command data to the queue. 
	 * If this is the first command, we immediately execute it.
	 */
	ECommandQueueError AddAbilityToTCommands(
		EAbilityID Ability,
		const FVector& Location = FVector::ZeroVector,
		AActor* TargetActor = nullptr,
		const FRotator& Rotation = FRotator::ZeroRotator,
		const int32 CustomType = FQueueCommand::DefaultCustomType
	);

	void StartCooldownForCommand(const FQueueCommand& Command);

	/**
	 * Execute either the next command (if bExecuteCurrentCommand=false) 
	 * or re-execute the current command (if bExecuteCurrentCommand=true).
	 */
	void ExecuteCommand(bool bExecuteCurrentCommand);

	/**
	 * Clears the command queue and resets indexes.
	 */
	void ClearCommands();

	/**
	 * @return If the queue is active and we optionally check for no Patrol commands
	 */
	ECommandQueueError IsQueueActiveAndNoPatrol() const;

	/**
	 * Whether the queue is enabled (accepting new commands).
	 */
	bool bM_IsCommandQueueEnabled;


	/** If true, the unit is spawning and hasn't received any commands yet. */
	bool bM_IsSpawning;

	/**
	 * The maximum number of commands we can store is 16.
	 */
	static const int32 MAX_COMMANDS = 16;

	/** The fixed-size array of commands. */
	UPROPERTY()
	FQueueCommand M_TCommands[MAX_COMMANDS];

	// The Abilities this unit has; initialised with data from game state.
	UPROPERTY()
	TArray<FUnitAbilityEntry> M_Abilities;

	int32 GetAbilityIndexById(EAbilityID AbilityId) const;

	/** The number of valid commands in M_TCommands. */
	int32 NumCommands;

	/** The index of the currently active command in M_TCommands. 
	    -1 means "no active command." */
	int32 CurrentIndex;

	/**
	 * The "Owner" that implements ICommands and holds this UCommandData.
	 */
	ICommands* M_Owner;

	FTimerHandle M_AbilityCooldownTimerHandle;

	FUnitAbilityEntry* GetAbilityEntry(const EAbilityID AbilityId);
	// Also requires the custom type of the ability entry to match.
	FUnitAbilityEntry* GetAbilityEntryOfCustomType(const EAbilityID, int32 CustomType);
	const FUnitAbilityEntry* GetAbilityEntry(const EAbilityID AbilityId) const;
	bool HasAbilityOnCooldown() const;
	void StartAbilityCooldownTimer();
	void StopAbilityCooldownTimer();
	void AbilityCoolDownTick();


	struct FFinalRotation
	{
		FRotator Rotation;
		bool bIsSet;
		// Guard to avoid double idle broadcast when we inject a rotate command.
		bool bFinishedCommandQueueWithFinalRotation = false;

		// policy for reverse-arrival handling (default keeps reverse ignores final rotation behaviour.)
		bool bForceFinalRotationRegardlessOfReverse = false;
	};


	// The final rotation for the unit to take after all commands are finished, the valid flag has to be set for it to take
	// action.
	FFinalRotation M_FinalRotation;

	/** @return NoError if the queue is active, has no patrol commands, is active and is not full! */
	ECommandQueueError GetCanAddAnythingNewToQueue() const;


	/** @brief If this command is a movement command we save the final rotation and set the final rotation flag to set
	 * only call this for commands added to the END of the queue! */
	void SetRotationFlagForFinalMovementCommand(const EAbilityID NewFinalCommand, const FRotator& Rotation);


	void ExecuteBehaviourAbility(const EBehaviourAbilityType BehaviourAbility, const bool bByPassQueue) const;
	void ExecuteModeAbility(const EModeAbilityType ModeAbility) const;
	void ExecuteDisableModeAbility(const EModeAbilityType ModeAbility) const;

	/**
     * @brief Some queue commands are not represented as Action UI abilities.
     * These commands must not be rejected just because they are missing from M_Abilities.
     */
	bool IsAbilityRequiredOnCommandCard(const EAbilityID CommandType) const;

	void TerminateFieldConstructionCommand();
};


// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType, NotBlueprintable)
class UCommands : public UInterface
{
	GENERATED_BODY()
};

DECLARE_MULTICAST_DELEGATE(FOnUnitIdleAndNoNewCommands);


/**
 * Contains a framework for all commands that can be executed by a unit.
 * @note Setup:
 * @note Overwrites in base cpp class: Need to overwrite:
 * @note 0) GetCommandData() in derived classes and call InitCommandData()
 * @note 1) DoneExecutingCommand Call super.
 */
class RTS_SURVIVAL_API ICommands
{
	GENERATED_BODY()

	// Execute commands need to be accessible by the prepare command functions in the CommandData struct.
	friend UCommandData;
	// To call DoneExecutingCommand.
	friend class RTS_SURVIVAL_API UTask_VehicleMoveTo;

public:
	// Delegate called when the unit is idle and has no new commands in the queue.
	FOnUnitIdleAndNoNewCommands OnUnitIdleAndNoNewCommandsDelegate;

	void SetPrimarySelected(UActionUIManager* ActionUIManager);

	int32 GetConstructionAbilityCount();

	virtual bool GetIsSquadUnit() { return false; }

	virtual void SetRTSOverlapEvasionEnabled(const bool bEnabled);
	virtual bool GetIsUnitInCombat()const;


	/** @brief Sets a simple flag to indicate that this unit has not recieved any player commands yet.
	 * Will be reset as soon as any order is issued (other than the spawn order)
	 */
	void SetIsSpawning(const bool bIsSpawning);

	// -----------------------------------------------------------------------
	// ================== Ability Utilities =========================
	// -----------------------------------------------------------------------
	/**
	 * @brief Initialized with the abilities as found in the GameState for the unit type that owns this CommandData.
	 * Because the unit subtype is usually set in derived bp, this is done with a timer on the frame after beginplay to
	 * ensure the right data is used.
	 * @param Abilities The abilities to use for this unit.
	 */
	void InitAbilityArray(const TArray<FUnitAbilityEntry>& Abilities);

	void InitAbilityArray(const TArray<EAbilityID>& Abilities);

	TArray<FUnitAbilityEntry> GetUnitAbilityEntries();

	TArray<EAbilityID> GetUnitAbilities();
	/**
	 * @brief Allows adding or removing abilities at runtime for the unit.
	 * @param Abilities The new ability array for the unit.
	 */
	void SetUnitAbilitiesRunTime(const TArray<FUnitAbilityEntry>& Abilities);

	void SetUnitAbilitiesRunTime(const TArray<EAbilityID>& Abilities);

	/**
	 * 
	 * @param NewAbility Ability to add. 
	 * @param AtIndex Optional: provide a specific index to add the ability to.
	 * @return Whether we could add the ability to an empty slot in the array. 
	 */
	bool AddAbility(const EAbilityID NewAbility, const int32 AtIndex = INDEX_NONE);
	bool AddAbility(const FUnitAbilityEntry& NewAbility, const int32 AtIndex = INDEX_NONE);
	/** @return Whether the ability could be successfully removed */
	bool RemoveAbility(const EAbilityID AbilityToRemove);

	/**
	 * @brief: Swaps the new ability in the ability array for the old ability if found.
	 * @param OldAbility The ability to swap out.
	 * @param NewAbility The ability to swap in. 
	 * @return Whether the swap was successful.
	 */
	bool SwapAbility(const EAbilityID OldAbility, const EAbilityID NewAbility);
	bool SwapAbility(const EAbilityID OldAbility, const FUnitAbilityEntry& NewAbility);


	bool HasAbility(const EAbilityID AbilityToCheck);
	bool StartCooldownOnAbility(const EAbilityID AbilityID, const int32 CustomType);

	// -----------------------------------------------------------------------
	// ================== End Ability Utilities =========================
	// -----------------------------------------------------------------------

	virtual bool GetIsScavenger() { return false; }
	virtual bool GetIsRepairUnit() { return false; }
	virtual bool GetCanCaptureUnits() { return false; }
	virtual float GetUnitRepairRadius() { return 100.f; }

	/** @return Will return the harvester component if this unit implements one.*/
	virtual UHarvester* GetIsHarvester();

	/** @return Whether the IsSpawning flag is set; this means the unit has not yet recieved any commands from the player.*/
	bool GetIsSpawning();


	// Omits the queue and immediately sets all resource conversion components associated with the unit
	// to enabled or disabled.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual void NoQueue_SetResourceConversionEnabled(const bool bEnabled);
	/**
	 * @brief Moves the unit to the provided location, adds the move command to the command queue.
	 * or executes it immediately depending on the provided reset.
	 * @param bSetUnitToIdle: Whether the unit should clear all previous commands making this ability the first
	 * in the list to be executed.
	 * @param FinishedMovementRotation
	 * @param bForceFinalRotationRegardlessOfReverse
	 * @return Whether the command could be added; command queue has not been stopped
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError MoveToLocation(
		const FVector& Location,
		const bool bSetUnitToIdle, const FRotator& FinishedMovementRotation,
		const bool bForceFinalRotationRegardlessOfReverse = false);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError Reinforce(const bool bSetUnitToIdle);

	void SetForceFinalRotationRegardlessOfReverse(bool ForceUseFinalRotation);

	// On Arrow override this is set to true: KEEP final rotation Regardless of reversing.
	// Legacy behavior (when set to false): ignore final rotation on reverse.
	bool GetForceFinalRotationRegardlessOfReverse();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError StopCommand();

	/**
	 * @brief The unit Attacks the provided actor, adds the attack command to the command queue.
	 * or executes it immediately depending on the provided reset.
	 * @param bSetUnitToIdle: Whether the unit should clear all previous commands making this ability the first
	 * in the queue to be executed.
	 * @return Whether the command could be added; command queue has not been stopped
	 */

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError AttackActor(
		AActor* Target,
		const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError CaptureActor(AActor* CaptureTarget, const bool bSetUnitToIdle);
	/**
	 * @brief Activates a behavior ability for the unit.
	 * 
	 * This function allows the unit to execute a specific behavior ability, either immediately or by adding it to the 
	 * command queue. Behavior abilities are predefined actions or states that the unit can perform, such as sprinting 
	 * 
	 * @param BehaviourAbility The type of behavior ability to activate. This is specified using the EBehaviourAbilityType enum.
	 * @param bSetUnitToIdle If true, the unit will instantly activate the behaviour; ignoring the queue.
	 *                       If false, the ability will be added to the end of the command queue and executed in sequence.
	 *                       
	 *@note The bSetUnitToIdle here allows for immediate execution of the behaviour without interrupting the current command queue.  
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ActivateBehaviourAbility(const EBehaviourAbilityType BehaviourAbility,
	                                                    const bool bSetUnitToIdle);
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError
	FieldConstruction(const EFieldConstructionType FieldConstruction, const bool bSetUnitToIdle,
	                  const FVector& ConstructionLocation, const FRotator& ConstructionRotation, AActor* StaticPreview);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")

	virtual ECommandQueueError ActivateModeAbility(const EModeAbilityType ModeAbilityType, const bool bSetUnitToIdle);

	virtual ECommandQueueError DisableModeAbility(const EModeAbilityType ModeAbilityType, const bool bSetUnitToIdle);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError AttackGround(
		const FVector& Location,
		const bool bSetUnitToIdle);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError PatrolToLocation(
		const FVector& Location,
		const bool bSetUnitToIdle);


	/**
	 * @brief Moves the unit in reverse to the provided location, adds the move command to the command queue.
	 * or executes it immediately depending on the provided reset.
	 * @param bSetUnitToIdle: Whether the unit should clear all previous commands making this ability the first
	 * in the list to be executed.
	 * @return Whether the command could be added; command queue has not been stopped
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ReverseUnitToLocation(
		const FVector& Location,
		const bool bSetUnitToIdle);


	/**
	 * @brief Turns the unit in the direction of the specified rotator
	 * @param WorldRotation The rotator to turn towards.
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first
	 * in the list to be executed.
	 * @return Whether the command could be added; command queue has not been stopped
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError RotateTowards(
		const FRotator& WorldRotation,
		const bool bSetUnitToIdle);

	/**
	 * @brief Orders the unit to create a building at the given location, adds the command to the command queue.
	 * or executes it immediately depending on the provided reset.
	 * @param Location: The location to place the building.
	 * @param WorldRotation: The Rotation to place the building with.
	 * @param bSetUnitToIdle: Whether the unit should clear all previous commands making this ability the first
	 * in the list to be executed.
	 * @return Whether the command could be added; command queue has not been stopped
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError CreateBuildingAtLocation(
		const FVector Location,
		const FRotator WorldRotation,
		const bool bSetUnitToIdle);

	/**
	 * @brief Orders the unit to harvest resources at the given location, adds the command to the command queue.
	 * @param TargetResource The actor with a resource component to harvest. 
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first
	 * @return Whether the command could be added; command queue has not been stopped
	 */

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError HarvestResource(
		AActor* TargetResource,
		const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ReturnCargo(
		const bool bSetUnitToIdle);


	/**
	 * @brief Orders the unit to pick up the item at the given location of provided item, adds the command to the command queue.
	 * @param TargetItem The item to pick up.
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first to be
	 * executed.
	 * @return Whether the command could be added; command queue has not been stopped
	 */

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError PickupItem(
		AItemsMaster* TargetItem,
		const bool bSetUnitToIdle);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError DigIn(const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError BreakCover(const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError SwitchWeapons(const bool bSetUnitToIdle);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError FireRockets(const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError CancelFireRockets(const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ThrowGrenade(const FVector& Location, const bool bSetUnitToIdle, const EGrenadeAbilityType GrenadeAbilityType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError CancelThrowingGrenade(const bool bSetUnitToIdle,
	                                                 const EGrenadeAbilityType GrenadeAbilityType);

	/**
	 * @brief Queues the aim ability with the provided target location and subtype.
	 * @param Location World location to aim at.
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first.
	 * @param AimAbilityType Subtype to execute.
	 * @return Whether the command could be added; command queue has not been stopped.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError AimAbility(const FVector& Location, const bool bSetUnitToIdle,
	                                      const EAimAbilityType AimAbilityType);

	/**
	 * @brief Queues the cancel aim ability used while moving to range.
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first.
	 * @param AimAbilityType Subtype to cancel.
	 * @return Whether the command could be added; command queue has not been stopped.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError CancelAimAbility(const bool bSetUnitToIdle, const EAimAbilityType AimAbilityType);
	/**
	 * @brief Determines whether the provided command is in the command queue.
	 * @param CommandToCheck The command to check for.
	 * @return Whether the command is anywhere in the queue. 
	 */
	bool GetHasCommandInQueue(const EAbilityID CommandToCheck);

	/**
	 * Converts the building back into a vehicle.
	 * @param bSetUnitToIdle Whether the unit should clear all previous commands making this ability the first
	 * @return Whether the command could be added; command queue has not been stopped
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ConvertToVehicle(const bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ScavengeObject(AActor* TargetObject, bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError RepairActor(AActor* TargetObject, bool bSetUnitToIdle);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ReturnToBase(const bool bSetUnitToIdle);

	/** @return the current command of the unit. */
	EAbilityID GetActiveCommandID();

	bool GetIsUnitIdle();

	/**
	 * @brief Terminates the current command and clears the command queue which sets the unit to idle.
	 * @post Unit is ready to execute a (new) command.
	 */
	void SetUnitToIdle();

	/** @brief Executes the next command in Commands, or sets unit to idle state.
	 * @param AbilityFinished: The ability that has finished executing.
	 * @note needs to be virtual to be able to use in Blueprints.
	 */
	UFUNCTION(BlueprintCallable, Category = "Commands", meta = (BlueprintProtected = "true"))
	virtual void DoneExecutingCommand(EAbilityID AbilityFinished);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError EnterCargo(
		AActor* CarrierActor,
		const bool bSetUnitToIdle);

	// Enqueue: exit from current carrier/transport.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Commands")
	virtual ECommandQueueError ExitCargo(const bool bSetUnitToIdle);

	/**
	 * @brief helper to obtain owner location.
	 * @return The location of the actor implementing this interface.
	 */
	virtual FVector GetOwnerLocation() const =0;

	virtual AActor* GetOwnerActor() =0;

	/**
	 * @brief Helper to obtain the name of the owner.
	 * @return The name of the actor implementing this interface.
	 */
	virtual FString GetOwnerName() const =0;

protected:
	// Called when the last command was terminated and the unit has no new commands in the queue.
	virtual void OnUnitIdleAndNoNewCommands();
	/**
	 * @brief Move the unit to the specified location.
	 * @param MoveToLocation The location to move to.
	 */
	virtual void ExecuteMoveCommand(const FVector MoveToLocation);

	/** @brief Stops all logic used for move commands..*/
	virtual void TerminateMoveCommand();

	virtual void ExecuteReinforceCommand(AActor* ReinforcementTarget);
	virtual void TerminateReinforceCommand();

	virtual void ExecuteStopCommand();
	/**
	 * @brief Attack the specified target.
	 * @param TargetActor The actor to attack.
	 */
	virtual void ExecuteAttackCommand(AActor* TargetActor);

	/** @brief Run When the unit finished their attack command.*/
	virtual void TerminateAttackCommand();

	virtual void ExecuteAttackGroundCommand(const FVector GroundLocation);

	/** @brief Run When the unit finished their attack command.*/
	virtual void TerminateAttackGroundCommand();

	/**
	 * @brief Move the unit to the specified location.
	 * @param PatrolToLocation The location to move to.
	 */
	virtual void ExecutePatrolCommand(const FVector PatrolToLocation);

	virtual void TerminatePatrolCommand();

	/**
	 * @brief Force-reverse moves the unit to the specified location.
	 * @param ReverseToLocation The location to reverse move to.
	 */
	virtual void ExecuteReverseCommand(const FVector ReverseToLocation);

	/** @brief Run when the unit finished their turn towards command.*/
	virtual void TerminateReverseCommand();

	/**
	 * @brief Rotates the unit towards the specified rotator.
	 * @param RotateToRotator The rotator to rotate towards.
	 * @param IsQueueCommand Whether this command needs to call DoneExecutingCommand or is not called from queue
	 * but separately.
	 */
	virtual void ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand);

	/** @brief Run when the unit finished their move command.*/
	virtual void TerminateRotateTowardsCommand();

	/**
	 * @brief Instruct the unit to build a structure at the provided location with the provided world rotation.
	 * @param BuildingLocation: The location of the builing.
	 * @param BuildingRotation: The world rotation of the building.
	 */
	virtual void ExecuteCreateBuildingCommand(const FVector BuildingLocation, const FRotator BuildingRotation);

	/** @brief Stops all logic used for move commands..*/
	virtual void TerminateCreateBuildingCommand();

	/** @brief Instruct the unit to convert back to a vehicle. */
	virtual void ExecuteConvertToVehicleCommand();

	/** @brief Stops all logic used for TerminateConvertToVehicleCommand.*/
	virtual void TerminateConvertToVehicleCommand();

	virtual void ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource);

	virtual void TerminateHarvestResourceCommand();

	virtual void ExecuteReturnCargoCommand();
	virtual void TerminateReturnCargoCommand();

	virtual void ExecutePickupItemCommand(AItemsMaster* TargetItem);

	virtual void TerminatePickupItemCommand();

	virtual void ExecuteSwitchWeaponsCommand();
	virtual void TerminateSwitchWeaponsCommand();

	virtual void ExecuteScavengeObject(AActor* TargetObject);
	virtual void TerminateScavengeObject();

	virtual void ExecuteDigIn();
	virtual void TerminateDigIn();

	virtual void ExecuteBreakCover();
	virtual void TerminateBreakCover();

	virtual void ExecuteFireRockets();
	virtual void TerminateFireRockets();

	virtual void ExecuteCancelFireRockets();
	virtual void TerminateCancelFireRockets();


	/**
	 * @brief Executes the grenade throw ability for this squad.
	 * @param TargetLocation World target to throw the grenade towards.
	 */
	virtual void ExecuteThrowGrenadeCommand(const FVector TargetLocation, const EGrenadeAbilityType GrenadeAbilityType);
	virtual void TerminateThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType);
	virtual void ExecuteCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType);
	virtual void TerminateCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType);

	virtual void ExecuteAimAbilityCommand(const FVector TargetLocation, const EAimAbilityType AimAbilityType);
	virtual void TerminateAimAbilityCommand(const EAimAbilityType AimAbilityType);
	virtual void ExecuteCancelAimAbilityCommand(const EAimAbilityType AimAbilityType);
	virtual void TerminateCancelAimAbilityCommand(const EAimAbilityType AimAbilityType);

	virtual void ExecuteRepairCommand(AActor* TargetActor);
	virtual void TerminateRepairCommand();

	virtual void ExecuteReturnToBase();
	virtual void TerminateReturnToBase();

	virtual void ExecuteEnterCargoCommand(AActor* CarrierActor);
	virtual void TerminateEnterCargoCommand();

	virtual void ExecuteExitCargoCommand();
	virtual void TerminateExitCargoCommand();

	virtual void ExecuteCaptureCommand(AActor* CaptureTarget);
	virtual void TerminateCaptureCommand();

	virtual void ExecuteFieldConstructionCommand(const EFieldConstructionType FieldConstruction,
	                                             const FVector& ConstructionLocation,
	                                             const FRotator& ConstructionRotation, AActor* StaticPreviewActor);
	virtual void TerminateFieldConstructionCommand(EFieldConstructionType FieldConstructionType,
	                                               AActor* StaticPreviewActor);

	virtual void NoQueue_ExecuteSetResourceConversionEnabled(const bool bEnabled);
	/**
	 * @brief Allows for custom unit-specific logic when the unit is reset for a new command.
	 * Logic common to all units like stopping behaviour trees and movement is handled by the CommandData struct.
	 */
	virtual void SetUnitToIdleSpecificLogic() =0;

	/**
	 * @return The command data used by this unit. Like MoveToLocations and TargetActors.
	 */
	virtual UCommandData* GetIsValidCommandData() =0;

	// Util to stop behaviour trees on any IcommandUnit.
	virtual void StopBehaviourTree() =0;

	// Util to stop movement on any ICommand unit.
	virtual void StopMovement() =0;

	/**
	 * @brief Sets whether this queue is able to accept new commands.
	 * @note If set to true, then the queue is cleared of any 
	 */
	void SetCommandQueueEnabled(const bool bSetEnabled);

	/**
	 * @brief Will rotate towards the last saved movement rotation direction in the command data if the set flag is
	 * true, this means that the last command was a movement command of which the rotation direction was saved.
	 */
	void RotateTowardsFinalMovementRotation();

	/** @brief Used to disable the final movement rotation flag by hand which can be done if a unit ended in reverse,
	 * then we do not call the rotate to but need to reset it so it will not be executed after any future commands. */
	void ResetRotateTowardsFinalMovementRotation();

private:
	// Called on DoneExecutingCommand.
	void TerminateCommand(EAbilityID AbilityToKill);

	ECommandQueueError GetIsAbilityOnCommandCardAndNotOnCooldown(const EAbilityID AbilityToCheck);
};
