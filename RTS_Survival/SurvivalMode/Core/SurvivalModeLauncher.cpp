// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalModeLauncher.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/SurvivalMode/UI/W_SurvivalMapPicker.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalModeLauncherConstants
{
	const TCHAR* FactionSelectionMap = TEXT("/Game/RTS_Survival/FactionSelection/RTS_FactionSelection");
	const TCHAR* FirstSurvivalMap = TEXT("/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01.SM_Desert_01");
	const TCHAR* KesselSurvivalMap = TEXT("/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01.SM_Kessel_01");
	constexpr int32 MapPickerZOrder = 50;
}

void USurvivalModeLauncher::LaunchSurvivalMode(const UObject* WorldContextObject, TSoftObjectPtr<UWorld> SurvivalMap)
{
	if (not SurvivalMap.IsNull())
	{
		LaunchSurvivalOnMap(WorldContextObject, SurvivalMap);
		return;
	}
#if RTS_WITH_MAP_TESTS
	// The menu test presses the SURVIVAL button and has no mouse for the picker: -SurvivalMenuMapIndex=N picks the map.
	if (FParse::Param(FCommandLine::Get(), TEXT("RTSSurvivalMenuFlow")))
	{
		int32 MapIndex = 0;
		FParse::Value(FCommandLine::Get(), TEXT("SurvivalMenuMapIndex="), MapIndex);
		const TArray<TSoftObjectPtr<UWorld>> Maps = GetSurvivalMaps();
		LaunchSurvivalOnMap(WorldContextObject, Maps[FMath::Clamp(MapIndex, 0, Maps.Num() - 1)]);
		return;
	}
#endif
	ShowMapPicker(WorldContextObject);
}

void USurvivalModeLauncher::LaunchSurvivalOnMap(const UObject* WorldContextObject, const TSoftObjectPtr<UWorld>& SurvivalMap)
{
	URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(WorldContextObject);
	if (not IsValid(GameInstance))
	{
		RTSFunctionLibrary::ReportError(TEXT("LaunchSurvivalMode: no RTS game instance."));
		return;
	}
	GameInstance->SetPreGameLaunchRequest(ERTSPreGameSetupFlow::FactionAndDifficulty, SurvivalMap);
	GameInstance->SetAllowUnitVariantSelection(true);
	UGameplayStatics::OpenLevel(WorldContextObject, FName(SurvivalModeLauncherConstants::FactionSelectionMap));
}

void USurvivalModeLauncher::ShowMapPicker(const UObject* WorldContextObject)
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	UW_SurvivalMapPicker* Picker = IsValid(PlayerController)
		                               ? CreateWidget<UW_SurvivalMapPicker>(PlayerController, UW_SurvivalMapPicker::StaticClass())
		                               : nullptr;
	if (not IsValid(Picker))
	{
		RTSFunctionLibrary::ReportError(TEXT("LaunchSurvivalMode: could not create the survival map picker."));
		return;
	}
	Picker->AddToViewport(SurvivalModeLauncherConstants::MapPickerZOrder);
	Picker->InitMapPicker(GetSurvivalMapChoices());
}

TArray<TSoftObjectPtr<UWorld>> USurvivalModeLauncher::GetSurvivalMaps()
{
	TArray<TSoftObjectPtr<UWorld>> Maps;
	for (const FSurvivalMapChoice& Choice : GetSurvivalMapChoices())
	{
		Maps.Add(Choice.Map);
	}
	return Maps;
}

TArray<FSurvivalMapChoice> USurvivalModeLauncher::GetSurvivalMapChoices()
{
	FSurvivalMapChoice Desert;
	Desert.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(SurvivalModeLauncherConstants::FirstSurvivalMap));
	Desert.Title = NSLOCTEXT("SurvivalMaps", "DesertTitle", "THE DESERT");
	Desert.Description = NSLOCTEXT("SurvivalMaps", "DesertText", "Pick your start. Soviet bases hold the other benches.");
	FSurvivalMapChoice Kessel;
	Kessel.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(SurvivalModeLauncherConstants::KesselSurvivalMap));
	Kessel.Title = NSLOCTEXT("SurvivalMaps", "KesselTitle", "THE KESSEL");
	Kessel.Description = NSLOCTEXT("SurvivalMaps", "KesselText", "You hold the plateau in the middle. The waves come from every side.");
	return {Desert, Kessel};
}
