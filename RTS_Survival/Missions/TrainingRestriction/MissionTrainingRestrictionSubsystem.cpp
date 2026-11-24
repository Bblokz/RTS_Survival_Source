// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionTrainingRestrictionSubsystem.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY(LogMissionTrainingRestriction);

namespace MissionTrainingRestriction
{
	constexpr uint8 PlayerTeamId = 1;
}

void UMissionTrainingRestrictionSubsystem::RestrictTo(const TArray<FTrainingOption>& AllowedOptions)
{
	bM_IsRestricted = true;
	M_AllowedOptions = TSet<FTrainingOption>(AllowedOptions);
	LogRestriction(TEXT("restricted"));
	RefreshPlayerTrainers();
}

void UMissionTrainingRestrictionSubsystem::AllowOptions(const TArray<FTrainingOption>& OptionsToAllow)
{
	if (not bM_IsRestricted)
	{
		return;
	}
	M_AllowedOptions.Append(OptionsToAllow);
	LogRestriction(TEXT("allowed more"));
	RefreshPlayerTrainers();
}

void UMissionTrainingRestrictionSubsystem::ClearRestriction()
{
	if (not bM_IsRestricted)
	{
		return;
	}
	bM_IsRestricted = false;
	M_AllowedOptions.Reset();
	LogRestriction(TEXT("cleared"));
	RefreshPlayerTrainers();
}

bool UMissionTrainingRestrictionSubsystem::GetIsOptionAllowed(const FTrainingOption& Option) const
{
	return not bM_IsRestricted || M_AllowedOptions.Contains(Option);
}

void UMissionTrainingRestrictionSubsystem::RefreshPlayerTrainers() const
{
	const UWorld* World = GetWorld();
	for (TObjectIterator<UTrainerComponent> It; It; ++It)
	{
		UTrainerComponent* Trainer = *It;
		if (not IsValid(Trainer) || Trainer->GetWorld() != World)
		{
			continue;
		}
		const AActor* Owner = Trainer->GetOwner();
		const URTSComponent* RTSComponent = IsValid(Owner) ? Owner->FindComponentByClass<URTSComponent>() : nullptr;
		if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != MissionTrainingRestriction::PlayerTeamId)
		{
			continue;
		}
		Trainer->CheckRequirementsAndRefreshUI();
	}
}

void UMissionTrainingRestrictionSubsystem::LogRestriction(const TCHAR* Reason) const
{
	FString Allowed;
	for (const FTrainingOption& Option : M_AllowedOptions)
	{
		Allowed += Option.GetTrainingName() + TEXT(" ");
	}
	UE_LOG(LogMissionTrainingRestriction, Display, TEXT("Mission training restriction %s: restricted=%d allowed=[%s]"),
	       Reason, bM_IsRestricted ? 1 : 0, *Allowed);
}
