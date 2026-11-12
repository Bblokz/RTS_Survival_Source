// Copyright (C) Bas Blokzijl - All rights reserved.

// UMissionBase: base-building tutorial helpers (training restriction, nomadic and building expansion queries).

#include "MissionBase.h"

#include "EngineUtils.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/TrainingRestriction/MissionTrainingRestrictionSubsystem.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionBaseBuilding
{
	constexpr uint8 PlayerTeamId = 1;

	UMissionTrainingRestrictionSubsystem* GetRestriction(const UWorld* World)
	{
		if (not IsValid(World))
		{
			RTSFunctionLibrary::ReportError("Mission training restriction: the mission has no world.");
			return nullptr;
		}
		return World->GetSubsystem<UMissionTrainingRestrictionSubsystem>();
	}

	bool GetIsOwnedByPlayer(const URTSComponent* RTSComponent)
	{
		return IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == PlayerTeamId;
	}
}

void UMissionBase::RestrictPlayerTrainingTo(const TArray<FTrainingOption>& AllowedOptions)
{
	if (UMissionTrainingRestrictionSubsystem* Restriction = MissionBaseBuilding::GetRestriction(GetWorld()))
	{
		Restriction->RestrictTo(AllowedOptions);
	}
}

void UMissionBase::AllowPlayerTrainingOptions(const TArray<FTrainingOption>& OptionsToAllow)
{
	if (UMissionTrainingRestrictionSubsystem* Restriction = MissionBaseBuilding::GetRestriction(GetWorld()))
	{
		Restriction->AllowOptions(OptionsToAllow);
	}
}

void UMissionBase::ClearPlayerTrainingRestriction()
{
	if (UMissionTrainingRestrictionSubsystem* Restriction = MissionBaseBuilding::GetRestriction(GetWorld()))
	{
		Restriction->ClearRestriction();
	}
}

void UMissionBase::SetMissionObjectiveText(const FText& Title, const FText& Description)
{
	MissionWidgetState.MissionTitle = Title;
	MissionWidgetState.MissionDescription = Description;
	if (not GetHasMissionWidget())
	{
		return;
	}
	M_MissionWidget->RefreshMissionWidget(MissionWidgetState);
}

TArray<ANomadicVehicle*> UMissionBase::GetPlayerNomadicsOfType(const ENomadicSubtype NomadicSubtype) const
{
	TArray<ANomadicVehicle*> PlayerNomadics;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return PlayerNomadics;
	}
	for (TActorIterator<ANomadicVehicle> It(World); It; ++It)
	{
		ANomadicVehicle* Nomadic = *It;
		if (not IsValid(Nomadic))
		{
			continue;
		}
		const URTSComponent* RTSComponent = Nomadic->GetRTSComponent();
		if (not MissionBaseBuilding::GetIsOwnedByPlayer(RTSComponent)
			|| RTSComponent->GetSubtypeAsNomadicSubtype() != NomadicSubtype)
		{
			continue;
		}
		PlayerNomadics.Add(Nomadic);
	}
	return PlayerNomadics;
}

bool UMissionBase::GetIsNomadicDeployedAsBuilding(const ANomadicVehicle* NomadicVehicle) const
{
	return IsValid(NomadicVehicle) && NomadicVehicle->GetNomadicStatus() == ENomadStatus::Building;
}

int32 UMissionBase::GetPlayerBuiltBuildingExpansionCountOfTypes(
	const TArray<EBuildingExpansionType>& BuildingExpansionTypes) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || BuildingExpansionTypes.IsEmpty())
	{
		return 0;
	}
	const TSet<EBuildingExpansionType> RequestedTypes(BuildingExpansionTypes);
	int32 BuiltCount = 0;
	for (TActorIterator<ABuildingExpansion> It(World); It; ++It)
	{
		const ABuildingExpansion* BuildingExpansion = *It;
		if (not IsValid(BuildingExpansion) || BuildingExpansion->GetStatus() != EBuildingExpansionStatus::BXS_Built)
		{
			continue;
		}
		const URTSComponent* RTSComponent = BuildingExpansion->GetRTSComponent();
		if (MissionBaseBuilding::GetIsOwnedByPlayer(RTSComponent)
			&& RequestedTypes.Contains(RTSComponent->GetSubtypeAsBxpSubtype()))
		{
			++BuiltCount;
		}
	}
	return BuiltCount;
}
