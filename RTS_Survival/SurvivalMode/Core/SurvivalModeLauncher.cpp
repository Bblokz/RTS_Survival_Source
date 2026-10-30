// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalModeLauncher.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalModeLauncherConstants
{
	const TCHAR* FactionSelectionMap = TEXT("/Game/RTS_Survival/FactionSelection/RTS_FactionSelection");
	const TCHAR* FirstSurvivalMap = TEXT("/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01.SM_Desert_01");
}

void USurvivalModeLauncher::LaunchSurvivalMode(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> SurvivalMap)
{
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(WorldContextObject);
	if (not IsValid(GameInstance))
	{
		RTSFunctionLibrary::ReportError(TEXT("LaunchSurvivalMode: no RTS game instance."));
		return;
	}
	if (SurvivalMap.IsNull())
	{
		SurvivalMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(SurvivalModeLauncherConstants::FirstSurvivalMap));
	}
	GameInstance->SetPreGameLaunchRequest(ERTSPreGameSetupFlow::FactionAndDifficulty, SurvivalMap);
	GameInstance->SetAllowUnitVariantSelection(true);
	UGameplayStatics::OpenLevel(WorldContextObject, FName(SurvivalModeLauncherConstants::FactionSelectionMap));
}

TArray<TSoftObjectPtr<UWorld>> USurvivalModeLauncher::GetSurvivalMaps()
{
	return {TSoftObjectPtr<UWorld>(FSoftObjectPath(SurvivalModeLauncherConstants::FirstSurvivalMap))};
}
