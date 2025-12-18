// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ModeAbilityComponent.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"


UModeAbilityComponent::UModeAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UModeAbilityComponent::ActivateMode()
{
	if (bM_IsModeActive)
	{
		NotifyDoneExecuting(EAbilityID::IdActivateMode);
		return;
	}

	if (not GetIsValidOwnerBehaviourComponent())
	{
		NotifyDoneExecuting(EAbilityID::IdActivateMode);
		return;
	}

	for (const TSubclassOf<UBehaviour>& BehaviourClass : ModeAbilitySettings.BehavioursToApply)
	{
		if (not BehaviourClass)
		{
			continue;
		}

		M_OwnerBehaviourComponent->AddBehaviour(BehaviourClass);
	}

	bM_IsModeActive = true;

	ClearAutoDeactivateTimer();
	if (ModeAbilitySettings.bAutoDeactivates && ModeAbilitySettings.ActiveTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UModeAbilityComponent> WeakThis(this);
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindLambda([WeakThis]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				WeakThis->HandleAutoDeactivate();
			});
			World->GetTimerManager().SetTimer(M_AutoDeactivateTimerHandle, TimerDelegate, ModeAbilitySettings.ActiveTime,
			                                false);
		}
	}

	SwapModeAbilityOnOwner(EAbilityID::IdActivateMode, EAbilityID::IdDisableMode);

	NotifyDoneExecuting(EAbilityID::IdActivateMode);
}

void UModeAbilityComponent::DeactivateMode()
{
	DeactivateModeInternal(true);
}

bool UModeAbilityComponent::GetIsModeActive() const
{
	return bM_IsModeActive;
}

EModeAbilityType UModeAbilityComponent::GetModeAbilityType() const
{
	return ModeAbilitySettings.ModeType;
}

void UModeAbilityComponent::PostInitProperties()
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

	M_OwnerBehaviourComponent = Cast<UBehaviourComp>(
		GetOwner()->GetComponentByClass(UBehaviourComp::StaticClass())
	);
}

void UModeAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	(void)GetIsValidOwnerCommandsInterface();
	(void)GetIsValidOwnerBehaviourComponent();
	BeginPlay_CheckSettings();
	BeginPlay_AddAbility();
}

bool UModeAbilityComponent::GetIsValidOwnerCommandsInterface() const
{
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_OwnerCommandsInterface",
		"GetIsValidOwnerCommandsInterface",
		this
	);
	return false;
}

bool UModeAbilityComponent::GetIsValidOwnerBehaviourComponent() const
{
	if (M_OwnerBehaviourComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_OwnerBehaviourComponent",
		"GetIsValidOwnerBehaviourComponent",
		GetOwner()
	);
	return false;
}

void UModeAbilityComponent::BeginPlay_CheckSettings() const
{
	if (ModeAbilitySettings.BehavioursToApply.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("No behaviours configured for mode ability component: " + GetDebugName());
	}

	if (ModeAbilitySettings.bAutoDeactivates && ModeAbilitySettings.ActiveTime <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			"Mode ability component set to auto deactivate without valid active time: " + GetDebugName());
	}
}

void UModeAbilityComponent::BeginPlay_AddAbility()
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

	AddAbilityToCommands();
}

void UModeAbilityComponent::AddAbilityToSquad(ASquadController* Squad)
{
	TWeakObjectPtr<UModeAbilityComponent> WeakThis(this);
	auto ApplyLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AddAbilityToCommands();
	};
	Squad->SquadDataCallbacks.CallbackOnSquadDataLoaded(ApplyLambda, WeakThis);
}

void UModeAbilityComponent::AddAbilityToCommands()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdActivateMode;
	NewAbility.CustomType = static_cast<int32>(ModeAbilitySettings.ModeType);
	M_OwnerCommandsInterface->AddAbility(NewAbility, ModeAbilitySettings.PreferredAbilityIndex);
}

void UModeAbilityComponent::ClearAutoDeactivateTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AutoDeactivateTimerHandle);
	}
}

void UModeAbilityComponent::HandleAutoDeactivate()
{
	DeactivateModeInternal(false);
}

void UModeAbilityComponent::SwapModeAbilityOnOwner(const EAbilityID OldAbility, const EAbilityID NewAbility) const
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry ModeAbilityEntry;
	ModeAbilityEntry.AbilityId = NewAbility;
	ModeAbilityEntry.CustomType = static_cast<int32>(ModeAbilitySettings.ModeType);

	if (not M_OwnerCommandsInterface->SwapAbility(OldAbility, ModeAbilityEntry))
	{
		const FString FromAbility = Global_GetAbilityIDAsString(OldAbility);
		const FString ToAbility = Global_GetAbilityIDAsString(NewAbility);
		RTSFunctionLibrary::ReportError(
			"Could not swap mode ability from " + FromAbility + " to " + ToAbility + ": " + GetDebugName());
	}
}

void UModeAbilityComponent::DeactivateModeInternal(const bool bShouldNotifyCommand)
{
	if (not bM_IsModeActive)
	{
		if (bShouldNotifyCommand)
		{
			NotifyDoneExecuting(EAbilityID::IdDisableMode);
		}
		return;
	}

	ClearAutoDeactivateTimer();

	if (not GetIsValidOwnerBehaviourComponent())
	{
		bM_IsModeActive = false;
		SwapModeAbilityOnOwner(EAbilityID::IdDisableMode, EAbilityID::IdActivateMode);
		if (bShouldNotifyCommand)
		{
			NotifyDoneExecuting(EAbilityID::IdDisableMode);
		}
		return;
	}

	for (const TSubclassOf<UBehaviour>& BehaviourClass : ModeAbilitySettings.BehavioursToApply)
	{
		if (not BehaviourClass)
		{
			continue;
		}

		M_OwnerBehaviourComponent->RemoveBehaviour(BehaviourClass);
	}

	bM_IsModeActive = false;
	SwapModeAbilityOnOwner(EAbilityID::IdDisableMode, EAbilityID::IdActivateMode);

	if (bShouldNotifyCommand)
	{
		NotifyDoneExecuting(EAbilityID::IdDisableMode);
	}
}

FString UModeAbilityComponent::GetDebugName() const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		return Owner->GetName() + " with component: " + GetName();
	}
	return "Null Owner";
}

void UModeAbilityComponent::NotifyDoneExecuting(const EAbilityID AbilityId) const
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	if (M_OwnerCommandsInterface->GetActiveCommandID() != AbilityId)
	{
		return;
	}

	M_OwnerCommandsInterface->DoneExecutingCommand(AbilityId);
}
