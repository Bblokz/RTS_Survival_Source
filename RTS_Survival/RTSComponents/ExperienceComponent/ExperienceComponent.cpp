#include "ExperienceComponent.h"

#include "ExperienceInterface/ExperienceInterface.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/UnitData/AircraftData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


URTSExperienceComp::URTSExperienceComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSExperienceComp::ReceiveExperience(const float Amount)
{
	Debug_Experience("Level pre receive exp: " + FString::FromInt(M_UnitExperience.M_CurrentLevel));
	M_UnitExperience.M_CumulativeExperience += Amount * M_UnitExperience.M_ExperienceMultiplier;

	while (M_UnitExperience.M_CurrentLevel + 1 < M_UnitExperience.Levels.Num() &&
		M_UnitExperience.M_CumulativeExperience >= M_UnitExperience.Levels[M_UnitExperience.M_CurrentLevel + 1].
		CumulativeExperienceNeeded)
	{
		M_UnitExperience.M_CurrentLevel++;
		OnLevelUpOwner();
	}
	Debug_Experience(
		"Amount exp received: " + FString::SanitizeFloat(Amount * M_UnitExperience.M_ExperienceMultiplier) +
		"\n Level post receive exp: " + FString::FromInt(M_UnitExperience.M_CurrentLevel));

	// Only updates the UI if this component is selected.
	UpdateExperienceUI();
}


void URTSExperienceComp::InitExperienceComponent(const TScriptInterface<IExperienceInterface>& Owner,
                                                 const FTrainingOption& UnitTypeAndSubtype,
                                                 const int8 OwningPlayer)
{
	M_Owner = Owner;
	M_OwningPlayer = OwningPlayer;
	(void)GetIsValidOwner();
	if (OwningPlayer != 1 && OwningPlayer != 2)
	{
		RTSFunctionLibrary::ReportError("Owning player is not valid! "
			"\n component: " + GetName()
			+ "\n Provided player: " + FString::FromInt(OwningPlayer));
		return;
	}
	SetupExperienceLevels(UnitTypeAndSubtype, OwningPlayer);
	SetupVeterancyIconSet(UnitTypeAndSubtype);
}

TFunction<void(float)> URTSExperienceComp::GetExperienceCallback()
{
	auto WeakThis = TWeakObjectPtr<URTSExperienceComp>(this);
	return [WeakThis](const float Amount)-> void
	{
		if (const auto This = WeakThis.Get())
		{
			This->ReceiveExperience(Amount);
		}
	};
}

void URTSExperienceComp::SetExperienceBarSelected(const bool bSetSelected, TObjectPtr<UActionUIManager> ActionUIManager)
{
	if (bSetSelected)
	{
		if (IsValid(ActionUIManager))
		{
			M_ActionUIManager = ActionUIManager;
			return;
		}
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, TEXT("ActionUIManager"),
		                                                  TEXT("URTSExperienceComp::SetExperienceBarSelected"));
		return;
	}
	// Reset the action UI manager.
	M_ActionUIManager = nullptr;
}

void URTSExperienceComp::UpdateExperienceUI() const
{
	// Only set if this component is selected.
	if (not IsValid(M_ActionUIManager))
	{
		return;
	}

	float ExpPercentage = 0.f;
	const int32 CumulativeExp = M_UnitExperience.M_CumulativeExperience;
	int32 ExpNeededForNextLevel = 0.f;

	// Check if there is a next level.
	if (M_UnitExperience.M_CurrentLevel + 1 < M_UnitExperience.Levels.Num())
	{
		if (M_UnitExperience.M_CurrentLevel >= 0)
		{
			ExpPercentage = GetPercentageProgressToNextLevel(ExpNeededForNextLevel, CumulativeExp);
		}
		else
		{
			ExpPercentage = static_cast<float>(CumulativeExp) / static_cast<float>(M_UnitExperience.Levels[0].
				CumulativeExperienceNeeded);
			ExpNeededForNextLevel = M_UnitExperience.Levels[0].CumulativeExperienceNeeded;
		}
	}
	else
	{
		ExpPercentage = GetPercentageProgressAtMaxLevel(ExpNeededForNextLevel, CumulativeExp);
	}
	M_ActionUIManager->UpdateExperienceBar(ExpPercentage, CumulativeExp, ExpNeededForNextLevel,
	                                       M_UnitExperience.M_CurrentLevel, M_UnitExperience.Levels.Num(), M_UnitExperience.M_VeterancyIconSet);
	Debug_Experience("update ui with :"
		"\n ExpPercentage: " + FString::SanitizeFloat(ExpPercentage) +
		"\n CumulativeExp: " + FString::FromInt(CumulativeExp) +
		"\n ExpNeededForNextLevel: " + FString::FromInt(ExpNeededForNextLevel) +
		"\n CurrentLevel: " + FString::FromInt(M_UnitExperience.M_CurrentLevel));
}


bool URTSExperienceComp::GetIsValidOwner() const
{
	if (IsValid(M_Owner.GetObject()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("the experience owner of the experience component is not valid!"
		"\n component: " + GetName());
	return false;
}

void URTSExperienceComp::OnLevelUpOwner() const
{
	if (GetIsValidOwner())
	{
		M_Owner->OnUnitLevelUp();
	}
}

float URTSExperienceComp::GetPercentageProgressToNextLevel(int32& OutExpNeededNextLevel,
                                                           const int32 InCumulativeExp) const
{
	const int32 CurrentLevelCumulative = M_UnitExperience.Levels[M_UnitExperience.M_CurrentLevel].
		CumulativeExperienceNeeded;
	const float NextLevelCumulative = M_UnitExperience.Levels[M_UnitExperience.M_CurrentLevel + 1].
		CumulativeExperienceNeeded;
	const int32 Range = NextLevelCumulative - CurrentLevelCumulative;
	const int32 Progress = InCumulativeExp - CurrentLevelCumulative;
	OutExpNeededNextLevel = NextLevelCumulative;
	return (Range > 0.f) ? (static_cast<float>(Progress) / static_cast<float>(Range)) : 0.f;
}

float URTSExperienceComp::GetPercentageProgressAtMaxLevel(int32& OutExpNeededNextLevel, int32 InCumulativeExp) const
{
	OutExpNeededNextLevel = InCumulativeExp;
	return 1.f;
}

void URTSExperienceComp::SetupExperienceLevels(const FTrainingOption& UnitTypeAndSubtype, const uint8 OwningPlayer)
{
	const auto CppGameState = FRTS_Statics::GetGameState(this);
	if (not CppGameState)
	{
		RTSFunctionLibrary::ReportError(
			"Cannot setup the experience levels on component: " + GetName() + " because the game state is null!");
		return;
	}
	switch (UnitTypeAndSubtype.UnitType)
	{
	case EAllUnitType::UNType_None:
		OnUnitTypeError("NONE");
		break;
	case EAllUnitType::UNType_Squad:
		SetupSquadExp(CppGameState, static_cast<ESquadSubtype>(UnitTypeAndSubtype.SubtypeValue), OwningPlayer);
		break;
	case EAllUnitType::UNType_Harvester:
		OnUnitTypeError("HARVESTER");
		break;
	case EAllUnitType::UNType_Tank:
		SetupTankExp(CppGameState, static_cast<ETankSubtype>(UnitTypeAndSubtype.SubtypeValue), OwningPlayer);
		break;
	case EAllUnitType::UNType_Nomadic:
		OnUnitTypeError("NOMADIC");
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		OnUnitTypeError("BUILDING_EXPANSION");
		break;
	case EAllUnitType::UNType_Aircraft:
		SetupAircraftExp(CppGameState, static_cast<EAircraftSubtype>(UnitTypeAndSubtype.SubtypeValue), OwningPlayer);
		break;
	}
}

void URTSExperienceComp::SetupVeterancyIconSet(const FTrainingOption& UnitTypeAndSubtype)
{
	switch (UnitTypeAndSubtype.UnitType) {
	case EAllUnitType::UNType_None:
		OnInvalidUnitForVeterancy("Unitype_None");
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerVehicles;
		break;
	case EAllUnitType::UNType_Squad:
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerInfantry;
		break;
	case EAllUnitType::UNType_Harvester:
		OnInvalidUnitForVeterancy("Harvester");
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerVehicles;
		break;
	case EAllUnitType::UNType_Tank:
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerVehicles;
		break;
	case EAllUnitType::UNType_Nomadic:
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerVehicles;
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		OnInvalidUnitForVeterancy("BuildingExpansion");
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerVehicles;
		break;
	case EAllUnitType::UNType_Aircraft:
		M_UnitExperience.M_VeterancyIconSet = EVeterancyIconSet::EVI_GerAircraft;
		break;
	}
}

void URTSExperienceComp::SetupTankExp(const ACPPGameState* GameState, const ETankSubtype TankSubType,
                                      const uint8 OwningPlayer)
{
	const auto TankData = GameState->GetTankDataOfPlayer(OwningPlayer, TankSubType);
	M_UnitExperience.Levels = TankData.ExperienceLevels;
	M_UnitExperience.M_ExperienceMultiplier = TankData.ExperienceMultiplier;
	M_UnitExperience.M_CumulativeExperience = TankData.ExperienceMultiplier;
	M_ExperienceWorth = TankData.ExperienceWorth;
}

void URTSExperienceComp::SetupSquadExp(const ACPPGameState* GameState, const ESquadSubtype SquadSubType,
                                       const uint8 OwningPlayer)
{
	const auto SquadData = GameState->GetSquadDataOfPlayer(OwningPlayer, SquadSubType);
	M_UnitExperience.Levels = SquadData.ExperienceLevels;
	M_UnitExperience.M_ExperienceMultiplier = SquadData.ExperienceMultiplier;
	M_UnitExperience.M_CumulativeExperience = SquadData.ExperienceMultiplier;
	M_ExperienceWorth = SquadData.ExperienceWorth;
}

void URTSExperienceComp::SetupAircraftExp(
	const ACPPGameState* GameState,
	const EAircraftSubtype AircraftTypeSubtype,
	const uint8 OwningPlayer)
{
	const auto AircraftData = GameState->GetAircraftDataOfPlayer(AircraftTypeSubtype, OwningPlayer);
	M_UnitExperience.Levels = AircraftData.ExperienceLevels;
	M_UnitExperience.M_ExperienceMultiplier = AircraftData.ExperienceMultiplier;
	M_UnitExperience.M_CumulativeExperience = AircraftData.ExperienceMultiplier;
	M_ExperienceWorth = AircraftData.ExperienceWorth;
}

void URTSExperienceComp::OnUnitTypeError(const FString& TypeAsString) const
{
	RTSFunctionLibrary::PrintString("Unit type is not valid for experience component! "
		"\n component: " + GetName()
		+ "\n Provided type: " + TypeAsString +
		"\n Ensure that at URTSExperienceComp::SetupExperienceLevels"
		"the proper type is supported");
}

void URTSExperienceComp::OnInvalidUnitForVeterancy(const FString& TypeAsString) const
{
	RTSFunctionLibrary::ReportError("Cannot init the veterancy icon set for the unit type: " + TypeAsString +
		"\n Ensure that the unit type is supported for veterancy icon sets.");
}

void URTSExperienceComp::Debug_Experience(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GExperienceSystem_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Purple);
	}
}
