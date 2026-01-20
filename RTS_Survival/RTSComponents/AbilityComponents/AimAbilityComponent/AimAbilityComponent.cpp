// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AimAbilityComponent.h"

#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehWeaponOverwriteVFX.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace AimAbilityConstants
{
	constexpr float MoveCheckIntervalSeconds = 0.2f;
	constexpr float SquadBehaviourBufferSeconds = 0.2f;
	constexpr float PercentDenominator = 100.0f;
	constexpr int32 RangePercentMin = 1;
	constexpr int32 RangePercentMax = 99;
}

FAimAbilitySettings::FAimAbilitySettings()
	: AimAbilityType(EAimAbilityType::DefaultBrummbarFire)
	, PreferredAbilityIndex(INDEX_NONE)
	, Cooldown(5)
	, BehaviourDuration(0.0f)
	, BehaviourApplied(nullptr)
	, AimAssistType(EPlayerAimAbilityTypes::None)
	, RangePercentage(10)
{
}

UAimAbilityComponent::UAimAbilityComponent()
	: M_AimAbilitySettings()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAimAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (not GetOwner())
	{
		return;
	}
	ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
	if (CommandsInterface)
	{
		M_OwnerCommandsInterface.SetInterface(CommandsInterface);
		M_OwnerCommandsInterface.SetObject(GetOwner());
	}
}

void UAimAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	(void)GetIsValidOwnerCommandsInterface();
	BeginPlay_CreateBehaviourInstance();
	BeginPlay_ValidateSettings();
	BeginPlay_AddAbility();
}

bool UAimAbilityComponent::CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const
{
	return false;
}

void UAimAbilityComponent::SetWeaponsDisabled()
{
}

void UAimAbilityComponent::SetWeaponsAutoEngage(const bool bUseLastTarget)
{
}

void UAimAbilityComponent::FireWeaponsAtLocation(const FVector& TargetLocation)
{
}

void UAimAbilityComponent::RequestMoveToLocation(const FVector& MoveToLocation)
{
}

void UAimAbilityComponent::StopMovementForAbility()
{
}

void UAimAbilityComponent::ExecuteAimAbility(const FVector& TargetLocation)
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	if (not GetIsValidBehaviourInstance())
	{
		NotifyDoneExecuting();
		return;
	}

	TArray<UWeaponState*> WeaponStates;
	float MaxRange = 0.0f;
	if (not CollectWeaponStates(WeaponStates, MaxRange))
	{
		NotifyDoneExecuting();
		return;
	}

	if (not GetIsInRange(TargetLocation, MaxRange))
	{
		StartMoveToRange(TargetLocation, MaxRange);
		return;
	}

	HandleAbilityInRange(TargetLocation, WeaponStates);
}

void UAimAbilityComponent::TerminateAimAbility()
{
	ClearAbilityTimers();
	if (M_AbilityExecutionState.M_State == EAimAbilityState::InAbility)
	{
		HandleBehaviourDurationFinished(false);
		return;
	}
	if (M_AbilityExecutionState.M_State != EAimAbilityState::MovingToRange)
	{
		return;
	}

	StopMovementForAbility();
	SwapAbilityToAim();
	M_AbilityExecutionState.M_State = EAimAbilityState::None;
}

void UAimAbilityComponent::ExecuteCancelAimAbility()
{
	ClearAbilityTimers();
	if (M_AbilityExecutionState.M_State != EAimAbilityState::MovingToRange)
	{
		return;
	}
	StopMovementForAbility();
	SwapAbilityToAim();
	M_AbilityExecutionState.M_State = EAimAbilityState::None;
}

EAimAbilityType UAimAbilityComponent::GetAimAbilityType() const
{
	return M_AimAbilitySettings.AimAbilityType;
}

EPlayerAimAbilityTypes UAimAbilityComponent::GetAimAssistType() const
{
	return M_AimAbilitySettings.AimAssistType;
}

float UAimAbilityComponent::GetAimAbilityRange() const
{
	TArray<UWeaponState*> WeaponStates;
	float MaxRange = 0.0f;
	if (not CollectWeaponStates(WeaponStates, MaxRange))
	{
		return 0.0f;
	}
	return MaxRange;
}

float UAimAbilityComponent::GetAimAbilityRadius() const
{
	return M_AimAbilitySettings.Radius;
}

void UAimAbilityComponent::BeginAbilityDurationTimer()
{
	StartBehaviourTimer(M_AimAbilitySettings.BehaviourDuration + AimAbilityConstants::SquadBehaviourBufferSeconds);
}

void UAimAbilityComponent::ClearDerivedTimers()
{
}

bool UAimAbilityComponent::GetIsValidOwnerCommandsInterface() const
{
	if (not IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_OwnerCommandsInterface",
		                                                      "UAimAbilityComponent::GetIsValidOwnerCommandsInterface",
		                                                      this);
		return false;
	}

	return true;
}

bool UAimAbilityComponent::GetIsValidBehaviourInstance() const
{
	if (not IsValid(M_WeaponOverwriteBehaviour))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_WeaponOverwriteBehaviour",
		                                                      "UAimAbilityComponent::GetIsValidBehaviourInstance",
		                                                      this);
		return false;
	}

	return true;
}

void UAimAbilityComponent::StartBehaviourTimer(const float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		RTSFunctionLibrary::ReportError("Aim ability behaviour duration is not valid.");
		HandleBehaviourDurationFinished(true);
		return;
	}
	UWorld* World = GetWorld();
	if (not World)
	{
		HandleBehaviourDurationFinished(true);
		return;
	}

	FTimerDelegate BehaviourTimerDelegate;
	TWeakObjectPtr<UAimAbilityComponent> WeakThis(this);
	BehaviourTimerDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->HandleBehaviourDurationFinished(true);
	});
	World->GetTimerManager().SetTimer(M_AbilityExecutionState.M_BehaviourDurationTimerHandle, BehaviourTimerDelegate, DurationSeconds, false);
}

void UAimAbilityComponent::HandleBehaviourDurationFinished(const bool bNotifyCommand)
{
	ClearAbilityTimers();
	if (not GetIsValidBehaviourInstance())
	{
		return;
	}

	for (const TWeakObjectPtr<UWeaponState>& WeaponState : M_AbilityExecutionState.M_WeaponStates)
	{
		if (not WeaponState.IsValid())
		{
			continue;
		}
		M_WeaponOverwriteBehaviour->RemovebehaviourFromWeaponStandalone(WeaponState.Get());
	}

	ClearCachedAbilityWeaponStates();
	SetWeaponsAutoEngage(false);
	M_AbilityExecutionState.M_State = EAimAbilityState::None;
	SwapAbilityToAim();
	StartCooldownOnAimAbility();

	if (bNotifyCommand)
	{
		NotifyDoneExecuting();
	}
}

void UAimAbilityComponent::CacheAbilityWeaponStates(const TArray<UWeaponState*>& WeaponStates)
{
	M_AbilityExecutionState.M_WeaponStates.Reset();
	M_AbilityExecutionState.M_WeaponStates.Reserve(WeaponStates.Num());
	for (UWeaponState* WeaponState : WeaponStates)
	{
		if (not IsValid(WeaponState))
		{
			continue;
		}
		M_AbilityExecutionState.M_WeaponStates.Add(WeaponState);
	}
}

void UAimAbilityComponent::ClearCachedAbilityWeaponStates()
{
	M_AbilityExecutionState.M_WeaponStates.Reset();
}

void UAimAbilityComponent::BeginPlay_AddAbility()
{
	if (not GetOwner())
	{
		return;
	}
	ASquadController* SquadController = Cast<ASquadController>(GetOwner());
	if (SquadController)
	{
		AddAbilityToSquad(SquadController);
		return;
	}
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate Del;
	TWeakObjectPtr<UAimAbilityComponent> WeakThis(this);
	Del.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AddAbilityToCommands();
	});
	World->GetTimerManager().SetTimerForNextTick(Del);
}

void UAimAbilityComponent::AddAbilityToCommands()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	FUnitAbilityEntry AimAbilityEntry;
	AimAbilityEntry.AbilityId = EAbilityID::IdAimAbility;
	AimAbilityEntry.CustomType = static_cast<int32>(M_AimAbilitySettings.AimAbilityType);
	AimAbilityEntry.CooldownDuration = M_AimAbilitySettings.Cooldown;
	M_OwnerCommandsInterface->AddAbility(AimAbilityEntry, M_AimAbilitySettings.PreferredAbilityIndex);
}

void UAimAbilityComponent::AddAbilityToSquad(ASquadController* SquadController)
{
	TWeakObjectPtr<UAimAbilityComponent> WeakThis(this);
	auto ApplyLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AddAbilityToCommands();
	};
	SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(ApplyLambda, WeakThis);
}

void UAimAbilityComponent::BeginPlay_CreateBehaviourInstance()
{
	if (not IsValid(M_AimAbilitySettings.BehaviourApplied))
	{
		RTSFunctionLibrary::ReportError("Aim ability behaviour class is not set.");
		return;
	}
	M_WeaponOverwriteBehaviour = NewObject<UBehWeaponOverwriteVFX>(this, M_AimAbilitySettings.BehaviourApplied);
}

void UAimAbilityComponent::BeginPlay_ValidateSettings() const
{
	if (M_AimAbilitySettings.Cooldown <= 0)
	{
		RTSFunctionLibrary::ReportError("Aim ability cooldown is not valid.");
	}
	if (M_AimAbilitySettings.BehaviourDuration <= 0.0f)
	{
		RTSFunctionLibrary::ReportError("Aim ability behaviour duration is not valid.");
	}
	if (M_AimAbilitySettings.RangePercentage < AimAbilityConstants::RangePercentMin
		|| M_AimAbilitySettings.RangePercentage > AimAbilityConstants::RangePercentMax)
	{
		RTSFunctionLibrary::ReportError("Aim ability range percentage must be between 1 and 99.");
	}
}

bool UAimAbilityComponent::GetIsInRange(const FVector& TargetLocation, const float MaxRange) const
{
	if (not IsValid(GetOwner()))
	{
		return false;
	}
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const float DistanceSquared = FVector::DistSquared(OwnerLocation, TargetLocation);
	return DistanceSquared <= FMath::Square(MaxRange);
}

float UAimAbilityComponent::GetDesiredRangeWithBuffer(const float MaxRange) const
{
	const int32 ClampedRangePercentage = FMath::Clamp(
		M_AimAbilitySettings.RangePercentage,
		AimAbilityConstants::RangePercentMin,
		AimAbilityConstants::RangePercentMax);
	const float RangeMultiplier = 1.0f - (static_cast<float>(ClampedRangePercentage) / AimAbilityConstants::PercentDenominator);
	return MaxRange * RangeMultiplier;
}

FVector UAimAbilityComponent::GetMoveToRangeLocation(const FVector& TargetLocation, const float DesiredRange) const
{
	if (not IsValid(GetOwner()))
	{
		return TargetLocation;
	}
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector OffsetToTarget = TargetLocation - OwnerLocation;
	const float DistanceToTarget = OffsetToTarget.Size();
	if (DistanceToTarget <= DesiredRange)
	{
		return OwnerLocation;
	}
	const FVector DirectionToTarget = OffsetToTarget / DistanceToTarget;
	return TargetLocation - (DirectionToTarget * DesiredRange);
}

void UAimAbilityComponent::HandleAbilityInRange(const FVector& TargetLocation,
                                                const TArray<UWeaponState*>& WeaponStates)
{
	if (not GetIsValidBehaviourInstance())
	{
		NotifyDoneExecuting();
		return;
	}

	M_AbilityExecutionState.M_TargetLocation = TargetLocation;
	if (M_AbilityExecutionState.M_State == EAimAbilityState::MovingToRange)
	{
		SwapAbilityToAim();
	}

	M_AbilityExecutionState.M_State = EAimAbilityState::InAbility;
	CacheAbilityWeaponStates(WeaponStates);
	SetWeaponsDisabled();
	for (UWeaponState* WeaponState : WeaponStates)
	{
		if (not IsValid(WeaponState))
		{
			continue;
		}
		M_WeaponOverwriteBehaviour->ApplybehaviourToWeaponStandalone(WeaponState);
	}
	FireWeaponsAtLocation(TargetLocation);
	BeginAbilityDurationTimer();
}

void UAimAbilityComponent::StartMoveToRange(const FVector& TargetLocation, const float MaxRange)
{
	if (M_AbilityExecutionState.M_State == EAimAbilityState::MovingToRange)
	{
		M_AbilityExecutionState.M_TargetLocation = TargetLocation;
		return;
	}

	M_AbilityExecutionState.M_TargetLocation = TargetLocation;
	M_AbilityExecutionState.M_State = EAimAbilityState::MovingToRange;
	SwapAbilityToCancel();

	const float DesiredRange = GetDesiredRangeWithBuffer(MaxRange);
	const FVector MoveToLocation = GetMoveToRangeLocation(TargetLocation, DesiredRange);
	RequestMoveToLocation(MoveToLocation);

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate MoveCheckDelegate;
	TWeakObjectPtr<UAimAbilityComponent> WeakThis(this);
	MoveCheckDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnMoveCheckTimer();
	});
	World->GetTimerManager().SetTimer(
		M_AbilityExecutionState.M_MoveToRangeTimerHandle,
		MoveCheckDelegate,
		AimAbilityConstants::MoveCheckIntervalSeconds,
		true);
}

void UAimAbilityComponent::OnMoveCheckTimer()
{
	if (M_AbilityExecutionState.M_State != EAimAbilityState::MovingToRange)
	{
		return;
	}

	TArray<UWeaponState*> WeaponStates;
	float MaxRange = 0.0f;
	if (not CollectWeaponStates(WeaponStates, MaxRange))
	{
		return;
	}

	if (not GetIsInRange(M_AbilityExecutionState.M_TargetLocation, MaxRange))
	{
		return;
	}

	StopMovementForAbility();
	ClearAbilityTimers();
	HandleAbilityInRange(M_AbilityExecutionState.M_TargetLocation, WeaponStates);
}

void UAimAbilityComponent::ClearAbilityTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AbilityExecutionState.M_MoveToRangeTimerHandle);
		World->GetTimerManager().ClearTimer(M_AbilityExecutionState.M_BehaviourDurationTimerHandle);
	}
	ClearDerivedTimers();
}

void UAimAbilityComponent::SwapAbilityToCancel()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	FUnitAbilityEntry CancelEntry;
	CancelEntry.AbilityId = EAbilityID::IdCancelAimAbility;
	CancelEntry.CustomType = static_cast<int32>(M_AimAbilitySettings.AimAbilityType);
	M_OwnerCommandsInterface->SwapAbility(EAbilityID::IdAimAbility, CancelEntry);
}

void UAimAbilityComponent::SwapAbilityToAim()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	FUnitAbilityEntry AimEntry;
	AimEntry.AbilityId = EAbilityID::IdAimAbility;
	AimEntry.CustomType = static_cast<int32>(M_AimAbilitySettings.AimAbilityType);
	AimEntry.CooldownDuration = M_AimAbilitySettings.Cooldown;
	M_OwnerCommandsInterface->SwapAbility(EAbilityID::IdCancelAimAbility, AimEntry);
}

void UAimAbilityComponent::NotifyDoneExecuting() const
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdAimAbility);
}

void UAimAbilityComponent::StartCooldownOnAimAbility() const
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	M_OwnerCommandsInterface->StartCooldownOnAbility(EAbilityID::IdAimAbility,
	                                                static_cast<int32>(M_AimAbilitySettings.AimAbilityType));
}
