// Copyright (C) Bas Blokzijl - All rights reserved.


#include "ApplyBehaviourAbilityComponent.h"

#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


FApplyBehaviourAbilitySettings::FApplyBehaviourAbilitySettings()
	: BehaviourAbility(EBehaviourAbilityType::DefaultSprint),
	  PreferredAbilityIndex(INDEX_NONE),
	  bReplaceAbilityInPreferredSlot(false),
	  Cooldown(5),
	  BehaviourApplied(nullptr)

{
	
}

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

	if (not GetIsValidBehaviourApplied())
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
	RefreshOwnerReferences();
}


void UApplyBehaviourAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	if (not GetShouldSetupAbilityOnBeginPlay())
	{
		return;
	}

	SetupBehaviourAbilityFromCurrentSettings();
}

bool UApplyBehaviourAbilityComponent::GetShouldSetupAbilityOnBeginPlay() const
{
	return true;
}

void UApplyBehaviourAbilityComponent::SetupBehaviourAbilityFromCurrentSettings()
{
	RefreshOwnerReferences();
	// Error checks.
	(void)GetIsValidOwnerCommandsInterface();
	(void)GetIsValidOwnerBehaviourComponent();
	CheckSettings();
	AddAbility();
}

void UApplyBehaviourAbilityComponent::RefreshOwnerReferences()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		M_OwnerBehaviourComponent.Reset();
		M_OwnerCommandsInterface.SetInterface(nullptr);
		M_OwnerCommandsInterface.SetObject(nullptr);
		return;
	}

	ICommands* CommandsInterface = Cast<ICommands>(Owner);
	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(CommandsInterface != nullptr ? Owner : nullptr);
	M_OwnerBehaviourComponent = Owner->FindComponentByClass<UBehaviourComp>();
}

bool UApplyBehaviourAbilityComponent::GetIsValidOwnerBehaviourComponent() const
{
	if (M_OwnerBehaviourComponent.IsValid())
	{
		return true;
	}

	const_cast<UApplyBehaviourAbilityComponent*>(this)->RefreshOwnerReferences();
	if (M_OwnerBehaviourComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerBehaviourComponent",
		"GetIsValidOwnerBehaviourComponent",
		this);
	return false;
}

bool UApplyBehaviourAbilityComponent::GetIsValidOwnerCommandsInterface() const
{
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	const_cast<UApplyBehaviourAbilityComponent*>(this)->RefreshOwnerReferences();
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerCommandsInterface",
		"GetIsValidOwnerCommandsInterface",
		this);
	return false;
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

bool UApplyBehaviourAbilityComponent::GetIsValidBehaviourApplied() const
{
	if (IsValid(BehaviourAbilitySettings.BehaviourApplied))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"BehaviourAbilitySettings.BehaviourApplied",
		"GetIsValidBehaviourApplied",
		this);
	return false;
}

void UApplyBehaviourAbilityComponent::CheckSettings() const
{
	(void)GetIsValidBehaviourApplied();
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
	if (BehaviourAbilitySettings.bReplaceAbilityInPreferredSlot)
	{
		AddAbilityToCommands_FreePreferredSlot();
	}
	M_OwnerCommandsInterface->AddAbility(NewAbility, BehaviourAbilitySettings.PreferredAbilityIndex);
}

void UApplyBehaviourAbilityComponent::AddAbilityToCommands_FreePreferredSlot() const
{
	const int32 PreferredIndex = BehaviourAbilitySettings.PreferredAbilityIndex;
	const TArray<FUnitAbilityEntry> OwnerAbilities = M_OwnerCommandsInterface->GetUnitAbilityEntries();
	if (not OwnerAbilities.IsValidIndex(PreferredIndex))
	{
		RTSFunctionLibrary::ReportError("bReplaceAbilityInPreferredSlot needs a valid PreferredAbilityIndex; got "
			+ FString::FromInt(PreferredIndex) + " on " + GetDebugName());
		return;
	}
	const FUnitAbilityEntry& OccupyingAbility = OwnerAbilities[PreferredIndex];
	if (OccupyingAbility.AbilityId == EAbilityID::IdNoAbility)
	{
		return;
	}
	if (not M_OwnerCommandsInterface->RemoveAbility(OccupyingAbility.AbilityId, OccupyingAbility.CustomType))
	{
		RTSFunctionLibrary::ReportError("Could not replace the ability in slot " + FString::FromInt(PreferredIndex)
			+ " on " + GetDebugName());
	}
}

void UApplyBehaviourAbilityComponent::AddAbility()
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
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UApplyBehaviourAbilityComponent> WeakThis(this);
		auto SetupAbility = [WeakThis]()-> void
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->AddAbilityToCommands();
		};
		// Prevent race condition with tank master beginplay initialisation.
		FTimerDelegate Del;
		Del.BindLambda(SetupAbility);
		World->GetTimerManager().SetTimerForNextTick(Del);
	}
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
