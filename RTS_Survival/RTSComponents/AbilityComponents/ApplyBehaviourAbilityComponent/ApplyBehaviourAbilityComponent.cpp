// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ApplyBehaviourAbilityComponent.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UApplyBehaviourAbilityComponent::UApplyBehaviourAbilityComponent(): BehaviourAbilitySettings()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UApplyBehaviourAbilityComponent::ExecuteBehaviourAbility() const
{
	if (not GetIsValidOwnerBehaviourComponent())
	{
		return;
	}
	M_OwnerBehaviourComponent->AddBehaviour(BehaviourAbilitySettings.BehaviourApplied);
}

void UApplyBehaviourAbilityComponent::TerminateBehaviourAbility() const
{
}

EBehaviourAbilityType UApplyBehaviourAbilityComponent::GetBehaviourAbilityType() const
{
	return BehaviourAbilitySettings.BehaviourAbility;
}

void UApplyBehaviourAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (not GetOwner())
	{
		return;
	}
	// Attempt to cast the owner to the ICommands interface
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


void UApplyBehaviourAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	// Error checks.
	(void)GetIsValidOwnerCommandsInterface();
	(void)GetIsValidOwnerBehaviourComponent();
	BeginPlay_CheckSettings();
	BeginPlay_AddAbility();
}

bool UApplyBehaviourAbilityComponent::GetIsValidOwnerBehaviourComponent() const
{
	if (not M_OwnerBehaviourComponent.IsValid())
	{
		RTSFunctionLibrary::ReportError("Owner behaviour component is not valid at : " + GetDebugName());
		return false;
	}
	return true;
}

bool UApplyBehaviourAbilityComponent::GetIsValidOwnerCommandsInterface() const
{
	if (not IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		RTSFunctionLibrary::ReportError("Owner as icommands is not valid at: " + GetDebugName());
		return false;
	}
	return true;
}

FString UApplyBehaviourAbilityComponent::GetDebugName() const
{
	const AActor* Owner = GetOwner();
	if (IsValid(Owner))
	{
		return Owner->GetName() + " with component: " + GetName();
	}
	return "Null Owner";
}

void UApplyBehaviourAbilityComponent::BeginPlay_CheckSettings() const
{
	if (not IsValid(BehaviourAbilitySettings.BehaviourApplied))
	{
		RTSFunctionLibrary::ReportError("No valid behaviour class on behaviour ability component."
			+ GetDebugName());
	}
	if (BehaviourAbilitySettings.Cooldown <= 0)
	{
		RTSFunctionLibrary::ReportError("cooldown on behaviour Ability is not valid!" + GetDebugName());
	}
}

void UApplyBehaviourAbilityComponent::AddAbilityToCommands()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdApplyBehaviour;
	NewAbility.CooldownDuration = BehaviourAbilitySettings.Cooldown;
	NewAbility.CustomType = static_cast<int32>(BehaviourAbilitySettings.BehaviourAbility);
	M_OwnerCommandsInterface->AddAbility(NewAbility, BehaviourAbilitySettings.PreferredAbilityIndex);
}

void UApplyBehaviourAbilityComponent::BeginPlay_AddAbility()
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

void UApplyBehaviourAbilityComponent::AddAbilityToSquad(ASquadController* Squad)
{
	TWeakObjectPtr<UApplyBehaviourAbilityComponent> WeakThis(this);
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
