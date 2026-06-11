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
	if (CompletedTechnology != M_CurrentTechnologyData.Technology)
	{
		return;
	}

	PlayCompletedAnnouncerVoiceLine();

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
	M_OrderedTechnologyEntries.Reset();
	BuildOrderedTechnologyEntries(ResearchTechnologyAbilitySettings.TechnologyData, M_OrderedTechnologyEntries);

	M_CurrentTechnologyIndex = M_OrderedTechnologyEntries.IsEmpty() ? INDEX_NONE : 0;
	M_CurrentTechnologyData = M_CurrentTechnologyIndex == INDEX_NONE
		? FResearchTechnologyAbilityTechnologyRuntimeData()
		: M_OrderedTechnologyEntries[M_CurrentTechnologyIndex];
}

void UResearchTechnologyAbilityComp::BeginPlay_InitValidateSettings() const
{
	if (not IsValid(ResearchTechnologyAbilitySettings.TechnologyData))
	{
		RTSFunctionLibrary::ReportError("Research technology ability has no technology data configured.");
		return;
	}

	if (ResearchTechnologyAbilitySettings.TechnologyData->Technology == ETechnology::Tech_NONE)
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

	BeginPlay_InitAddAbilityToCommandsNextTick();
}

void UResearchTechnologyAbilityComp::BeginPlay_InitAddAbilityToCommandsNextTick()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

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
	if (M_CurrentTechnologyData.Technology == ETechnology::Tech_NONE)
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
	if (M_CurrentTechnologyData.CompletedAnnouncerVoiceLines.VoiceLines.IsEmpty())
	{
		return;
	}

	USoundBase* CompletedAnnouncerVoiceLine =
		M_CurrentTechnologyData.CompletedAnnouncerVoiceLines.GetVoiceLine();
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

	for (int32 NextTechnologyIndex = M_CurrentTechnologyIndex + 1;
		 NextTechnologyIndex < M_OrderedTechnologyEntries.Num();
		 ++NextTechnologyIndex)
	{
		const FResearchTechnologyAbilityTechnologyRuntimeData& NextTechnologyData =
			M_OrderedTechnologyEntries[NextTechnologyIndex];
		if (NextTechnologyData.Technology == ETechnology::Tech_NONE)
		{
			continue;
		}

		M_CurrentTechnologyIndex = NextTechnologyIndex;
		M_CurrentTechnologyData = NextTechnologyData;
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

void UResearchTechnologyAbilityComp::BuildOrderedTechnologyEntries(
	const UResearchTechnologyAbilityTechnologyData* TechnologyData,
	TArray<FResearchTechnologyAbilityTechnologyRuntimeData>& OutTechnologyEntries) const
{
	if (not IsValid(TechnologyData))
	{
		return;
	}

	OutTechnologyEntries.Add(CreateRuntimeTechnologyData(*TechnologyData));

	for (const TObjectPtr<UResearchTechnologyAbilityTechnologyData>& FollowUpTechnologyData :
		 TechnologyData->FollowUpTechnologies)
	{
		BuildOrderedTechnologyEntries(FollowUpTechnologyData, OutTechnologyEntries);
	}
}

FResearchTechnologyAbilityTechnologyRuntimeData UResearchTechnologyAbilityComp::CreateRuntimeTechnologyData(
	const UResearchTechnologyAbilityTechnologyData& TechnologyData) const
{
	FResearchTechnologyAbilityTechnologyRuntimeData RuntimeTechnologyData;
	RuntimeTechnologyData.Technology = TechnologyData.Technology;
	RuntimeTechnologyData.RequiredTechnologies = TechnologyData.RequiredTechnologies;
	RuntimeTechnologyData.Costs = TechnologyData.Costs;
	RuntimeTechnologyData.CompletedAnnouncerVoiceLines = TechnologyData.CompletedAnnouncerVoiceLines;
	return RuntimeTechnologyData;
}

FUnitAbilityEntry UResearchTechnologyAbilityComp::CreateCurrentAbilityEntry() const
{
	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdResearchTechnology;
	NewAbility.CustomType = static_cast<int32>(M_CurrentTechnologyData.Technology);
	NewAbility.Costs = M_CurrentTechnologyData.Costs;
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
