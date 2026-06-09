// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ResearchTechnologyAbilityComp.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UResearchTechnologyAbilityComp::UResearchTechnologyAbilityComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UResearchTechnologyAbilityComp::BeginPlay()
{
	Super::BeginPlay();

	RefreshOwnerReferences();
	BeginPlay_InitRuntimeSettings();
	BeginPlay_InitValidateSettings();
	BeginPlay_InitAddAbility();
}

void UResearchTechnologyAbilityComp::TechResearchComplete(const ETechnology CompletedTechnology)
{
	if (CompletedTechnology != M_CurrentTechnology)
	{
		return;
	}

	PlayCompletedAnnouncerVoiceLine();

	if (M_NextFollowUpTechnologyIndex >= ResearchTechnologyAbilitySettings.FollowUpTechnologies.Num())
	{
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->RemoveAbility(
				EAbilityID::IdResearchTechnology,
				static_cast<int32>(M_CurrentTechnology));
		}
		return;
	}

	SwapToNextTechnology(CompletedTechnology);
}

void UResearchTechnologyAbilityComp::RefreshOwnerReferences()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		M_OwnerCommandsInterface.SetInterface(nullptr);
		M_OwnerCommandsInterface.SetObject(nullptr);
		return;
	}

	ICommands* CommandsInterface = Cast<ICommands>(Owner);
	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(CommandsInterface != nullptr ? Owner : nullptr);
}

void UResearchTechnologyAbilityComp::BeginPlay_InitRuntimeSettings()
{
	M_CurrentTechnology = ResearchTechnologyAbilitySettings.Technology;
	M_CurrentRequiredTechnologies = ResearchTechnologyAbilitySettings.RequiredTechnologies;
	M_NextFollowUpTechnologyIndex = 0;
}

void UResearchTechnologyAbilityComp::BeginPlay_InitValidateSettings() const
{
	if (ResearchTechnologyAbilitySettings.Technology == ETechnology::Tech_NONE)
	{
		RTSFunctionLibrary::ReportError("Research technology ability has Tech_NONE configured.");
	}
}

void UResearchTechnologyAbilityComp::BeginPlay_InitAddAbility()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	ASquadController* SquadController = Cast<ASquadController>(Owner);
	if (IsValid(SquadController))
	{
		AddAbilityToSquad(SquadController);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UResearchTechnologyAbilityComp> WeakThis(this);
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->AddAbilityToCommands();
		});
		World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
	}
}

void UResearchTechnologyAbilityComp::AddAbilityToSquad(ASquadController* Squad)
{
	TWeakObjectPtr<UResearchTechnologyAbilityComp> WeakThis(this);
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

void UResearchTechnologyAbilityComp::AddAbilityToCommands()
{
	if (M_CurrentTechnology == ETechnology::Tech_NONE)
	{
		return;
	}

	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	M_OwnerCommandsInterface->AddAbility(
		CreateCurrentAbilityEntry(),
		ResearchTechnologyAbilitySettings.PreferredAbilityIndex);
}

void UResearchTechnologyAbilityComp::PlayCompletedAnnouncerVoiceLine()
{
	if (ResearchTechnologyAbilitySettings.CompletedAnnouncerVoiceLines.VoiceLines.IsEmpty())
	{
		return;
	}

	USoundBase* CompletedAnnouncerVoiceLine =
		ResearchTechnologyAbilitySettings.CompletedAnnouncerVoiceLines.GetVoiceLine();
	if (not IsValid(CompletedAnnouncerVoiceLine))
	{
		return;
	}

	ACPPController* PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return;
	}

	PlayerController->PlayCustomAnnouncerVoiceLine(CompletedAnnouncerVoiceLine, true);
}

void UResearchTechnologyAbilityComp::SwapToNextTechnology(const ETechnology CompletedTechnology)
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	while (M_NextFollowUpTechnologyIndex < ResearchTechnologyAbilitySettings.FollowUpTechnologies.Num())
	{
		const ETechnology NextTechnology =
			ResearchTechnologyAbilitySettings.FollowUpTechnologies[M_NextFollowUpTechnologyIndex];
		M_NextFollowUpTechnologyIndex++;

		if (NextTechnology == ETechnology::Tech_NONE)
		{
			continue;
		}

		M_CurrentTechnology = NextTechnology;
		M_CurrentRequiredTechnologies.Reset();
		M_CurrentRequiredTechnologies.Add(CompletedTechnology);

		M_OwnerCommandsInterface->SwapAbility(
			EAbilityID::IdResearchTechnology,
			static_cast<int32>(CompletedTechnology),
			CreateCurrentAbilityEntry());
		return;
	}

	M_OwnerCommandsInterface->RemoveAbility(
		EAbilityID::IdResearchTechnology,
		static_cast<int32>(CompletedTechnology));
}

FUnitAbilityEntry UResearchTechnologyAbilityComp::CreateCurrentAbilityEntry() const
{
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdResearchTechnology;
	NewAbility.CustomType = static_cast<int32>(M_CurrentTechnology);
	NewAbility.Costs = ResearchTechnologyAbilitySettings.Costs;
	return NewAbility;
}

bool UResearchTechnologyAbilityComp::GetIsValidOwnerCommandsInterface() const
{
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	const_cast<UResearchTechnologyAbilityComp*>(this)->RefreshOwnerReferences();
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
