// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "AimAbilityComponent.generated.h"

class ASquadController;
class ICommands;
class UBehWeaponOverwriteVFX;
class UWeaponState;

UENUM()
enum class EAimAbilityState : uint8
{
	None,
	MovingToRange,
	InAbility,
};

USTRUCT()
struct FAimAbilityExecutionState
{
	GENERATED_BODY()

	EAimAbilityState M_State = EAimAbilityState::None;

	FVector M_TargetLocation = FVector::ZeroVector;

	// Cached weapon states for removing the behaviour after the ability ends.
	UPROPERTY()
	TArray<TWeakObjectPtr<UWeaponState>> M_WeaponStates;

	FTimerHandle M_MoveToRangeTimerHandle;
	FTimerHandle M_BehaviourDurationTimerHandle;
};

USTRUCT(BlueprintType)
struct FAimAbilitySettings
{
	GENERATED_BODY()

	FAimAbilitySettings();

	// Determines what type of aim ability this is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAimAbilityType AimAbilityType = EAimAbilityType::DefaultBrummbarFire;

	// Attempts to add the ability to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	// How long the ability is on cooldown after use. (does not affect behaviour duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 5;

	// How long the behaviour remains applied after firing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BehaviourDuration = 0.0f;

	// Behaviour applied to weapon states while the aim ability is active.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UBehWeaponOverwriteVFX> BehaviourApplied;

	// Aim assist material type shown on the player aim ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerAimAbilityTypes AimAssistType = EPlayerAimAbilityTypes::None;

	// 1-99 buffer range percentage when moving closer before firing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 RangePercentage = 10;

	// the radius of the ability effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Radius = 300;
	
};

/**
 * @brief Shared aim ability logic that derived components extend for squads or tanks.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UAimAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAimAbilityComponent();

	void ExecuteAimAbility(const FVector& TargetLocation);
	void TerminateAimAbility();
	void ExecuteCancelAimAbility();

	EAimAbilityType GetAimAbilityType() const;
	EPlayerAimAbilityTypes GetAimAssistType() const;
	float GetAimAbilityRange() const;
	float GetAimAbilityRadius() const;

protected:
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAimAbilitySettings M_AimAbilitySettings;

	/**
	 * @brief Collects the weapons used by this ability and their max range for targeting.
	 * @param OutWeaponStates Weapon states used for the ability.
	 * @param OutMaxRange Maximum weapon range detected for targeting.
	 * @return True when the weapon list is valid for use.
	 */
	virtual bool CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const ;

	virtual void SetWeaponsDisabled() ;
	virtual void SetWeaponsAutoEngage(const bool bUseLastTarget) ;
	virtual void FireWeaponsAtLocation(const FVector& TargetLocation) ;
	virtual void RequestMoveToLocation(const FVector& MoveToLocation) ;
	virtual void StopMovementForAbility() ;
	virtual void BeginAbilityDurationTimer();
	virtual void ClearDerivedTimers();

	bool GetIsValidOwnerCommandsInterface() const;
	bool GetIsValidBehaviourInstance() const;

	void StartBehaviourTimer(const float DurationSeconds);
	void HandleBehaviourDurationFinished(const bool bNotifyCommand);
	void CacheAbilityWeaponStates(const TArray<UWeaponState*>& WeaponStates);
	void ClearCachedAbilityWeaponStates();

private:
	void BeginPlay_AddAbility();
	void AddAbilityToCommands();
	void AddAbilityToSquad(ASquadController* SquadController);
	void BeginPlay_CreateBehaviourInstance();
	void BeginPlay_ValidateSettings() const;

	bool GetIsInRange(const FVector& TargetLocation, const float MaxRange) const;
	float GetDesiredRangeWithBuffer(const float MaxRange) const;
	FVector GetMoveToRangeLocation(const FVector& TargetLocation, const float DesiredRange) const;
	void HandleAbilityInRange(const FVector& TargetLocation, const TArray<UWeaponState*>& WeaponStates);
	void StartMoveToRange(const FVector& TargetLocation, const float MaxRange);
	void OnMoveCheckTimer();
	void ClearAbilityTimers();

	void SwapAbilityToCancel();
	void SwapAbilityToAim();
	void NotifyDoneExecuting() const;
	void StartCooldownOnAimAbility() const;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	UPROPERTY()
	TObjectPtr<UBehWeaponOverwriteVFX> M_WeaponOverwriteBehaviour;

	FAimAbilityExecutionState M_AbilityExecutionState;
};
