// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ResearchTechnologyAbilityComp.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
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
	BeginPlay_SetActiveTechAbilityForFaction();
	BeginPlay_InitRuntimeSettings();
	BeginPlay_InitValidateSettings();
	BeginPlay_RegisterAtPlayerTechManager();
	BeginPlay_InitAddAbility();
}

void UResearchTechnologyAbilityComp::TechResearchComplete(const ETechnology CompletedTechnology)
{
	AccountForResearchedTechnology(CompletedTechnology);
}

void UResearchTechnologyAbilityComp::AccountForResearchedTechnology(const ETechnology ResearchedTechnology)
{
	if (ResearchedTechnology == ETechnology::Tech_NONE)
	{
		return;
	}

	const int32 ResearchedTechnologyIndex = FindOrderedTechnologyIndex(ResearchedTechnology);
	if (ResearchedTechnologyIndex == INDEX_NONE)
	{
		return;
	}

	if (ResearchedTechnologyIndex == M_CurrentTechnologyIndex)
	{
		SwapToNextTechnology(ResearchedTechnology);
		return;
	}

	RemoveResearchedTechnologyFromQueuedEntries(ResearchedTechnology, ResearchedTechnologyIndex);
}

void UResearchTechnologyAbilityComp::RefreshOwnerReferences()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		M_OwnerCommandsInterface.SetInterface(nullptr);
		M_OwnerCommandsInterface.SetObject(nullptr);
		M_PlayerTechManager = nullptr;
		return;
	}

	ICommands* CommandsInterface = Cast<ICommands>(Owner);
	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(CommandsInterface != nullptr ? Owner : nullptr);

	M_PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this);
}

void UResearchTechnologyAbilityComp::BeginPlay_SetActiveTechAbilityForFaction()
{
	ERTSFaction PlayerFaction  = FRTS_Statics::GetPlayerFaction(this);
	if (PlayerFaction == ERTSFaction::NotInitialised)
	{
		const FString SafeOwnerName = IsValid(GetOwner())? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("No Initialized faction for player cannot determine faction research abilities, for: " + SafeOwnerName);
		return ;
	}
	if (TechAbilitySettingsPerFaction.Contains(PlayerFaction))
	{
		M_FactionChosenTechAbilitySettings = TechAbilitySettingsPerFaction[PlayerFaction];
	}
	else
	{
		const FString SafeOwnerName = IsValid(GetOwner())? GetOwner()->GetName() : "NULL";
		RTSFunctionLibrary::ReportError("There was not faction research setup for the faction: " + UEnum::GetValueAsString(PlayerFaction) +
			"\n For owner: " + SafeOwnerName);
	}
}


void UResearchTechnologyAbilityComp::BeginPlay_InitRuntimeSettings()
{
	M_OrderedTechnologyEntries.Reset();
	BuildOrderedTechnologyEntries(M_FactionChosenTechAbilitySettings.TechnologyData, M_OrderedTechnologyEntries);

	M_CurrentTechnologyIndex = M_OrderedTechnologyEntries.IsEmpty() ? INDEX_NONE : 0;
	M_CurrentTechnologyData = M_CurrentTechnologyIndex == INDEX_NONE
		? FResearchTechnologyAbilityTechnologyRuntimeData()
		: M_OrderedTechnologyEntries[M_CurrentTechnologyIndex];
}

void UResearchTechnologyAbilityComp::BeginPlay_InitValidateSettings() const
{
	if (not IsValid(M_FactionChosenTechAbilitySettings.TechnologyData))
	{
		RTSFunctionLibrary::ReportError("Research technology ability has no technology data configured.");
		return;
	}

	if (M_FactionChosenTechAbilitySettings.TechnologyData->Technology == ETechnology::Tech_NONE)
	{
		RTSFunctionLibrary::ReportError("Research technology ability has Tech_NONE configured.");
	}
}

void UResearchTechnologyAbilityComp::BeginPlay_RegisterAtPlayerTechManager()
{
	if (M_CurrentTechnologyData.Technology == ETechnology::Tech_NONE)
	{
		return;
	}

	if (not GetIsValidPlayerTechManager())
	{
		return;
	}

	M_PlayerTechManager->RegisterResearchTechnologyAbilityComp(this);
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

	bM_HasAddedAbilityToCommands = M_OwnerCommandsInterface->AddAbility(
		CreateCurrentAbilityEntry(),
		M_FactionChosenTechAbilitySettings.PreferredAbilityIndex);
}

bool UResearchTechnologyAbilityComp::TryPlayCompletedAnnouncerVoiceLine(const ETechnology CompletedTechnology)
{
	if (CompletedTechnology != M_CurrentTechnologyData.Technology)
	{
		return false;
	}

	if (M_CurrentTechnologyData.CompletedAnnouncerVoiceLines.VoiceLines.IsEmpty())
	{
		return false;
	}

	USoundBase* CompletedAnnouncerVoiceLine =
		M_CurrentTechnologyData.CompletedAnnouncerVoiceLines.GetVoiceLine();
	if (not IsValid(CompletedAnnouncerVoiceLine))
	{
		return false;
	}

	ACPPController* PlayerController = FRTS_Statics::GetRTSController(this);
	if (not IsValid(PlayerController))
	{
		return false;
	}

	PlayerController->PlayCustomAnnouncerVoiceLine(CompletedAnnouncerVoiceLine, true);
	return true;
}

void UResearchTechnologyAbilityComp::SwapToNextTechnology(const ETechnology CompletedTechnology)
{
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
		UpdateCommandCardForResearchedCurrentTechnology(CompletedTechnology);
		return;
	}

	M_CurrentTechnologyIndex = INDEX_NONE;
	M_CurrentTechnologyData = FResearchTechnologyAbilityTechnologyRuntimeData();
	UpdateCommandCardForResearchedCurrentTechnology(CompletedTechnology);
}

void UResearchTechnologyAbilityComp::RemoveResearchedTechnologyFromQueuedEntries(
	const ETechnology ResearchedTechnology,
	const int32 ResearchedTechnologyIndex)
{
	if (ResearchedTechnologyIndex < 0 || ResearchedTechnologyIndex >= M_OrderedTechnologyEntries.Num())
	{
		return;
	}

	M_OrderedTechnologyEntries.RemoveAt(ResearchedTechnologyIndex);
	if (ResearchedTechnologyIndex < M_CurrentTechnologyIndex)
	{
		--M_CurrentTechnologyIndex;
	}

	if (M_CurrentTechnologyData.Technology == ResearchedTechnology)
	{
		M_CurrentTechnologyIndex = FindOrderedTechnologyIndex(ResearchedTechnology);
	}
}

int32 UResearchTechnologyAbilityComp::FindOrderedTechnologyIndex(const ETechnology Technology) const
{
	return M_OrderedTechnologyEntries.IndexOfByPredicate(
		[Technology](const FResearchTechnologyAbilityTechnologyRuntimeData& TechnologyData)
		{
			return TechnologyData.Technology == Technology;
		});
}

void UResearchTechnologyAbilityComp::UpdateCommandCardForResearchedCurrentTechnology(
	const ETechnology CompletedTechnology)
{
	if (not bM_HasAddedAbilityToCommands)
	{
		return;
	}

	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	if (M_CurrentTechnologyData.Technology == ETechnology::Tech_NONE)
	{
		bM_HasAddedAbilityToCommands = not M_OwnerCommandsInterface->RemoveAbility(
			EAbilityID::IdResearchTechnology,
			static_cast<int32>(CompletedTechnology));
		return;
	}

	bM_HasAddedAbilityToCommands = M_OwnerCommandsInterface->SwapAbility(
		EAbilityID::IdResearchTechnology,
		static_cast<int32>(CompletedTechnology),
		CreateCurrentAbilityEntry());
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

bool UResearchTechnologyAbilityComp::GetIsValidPlayerTechManager() const
{
	if (IsValid(M_PlayerTechManager))
	{
		return true;
	}

	const_cast<UResearchTechnologyAbilityComp*>(this)->RefreshOwnerReferences();
	if (IsValid(M_PlayerTechManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PlayerTechManager",
		"GetIsValidPlayerTechManager",
		this);
	return false;
}
