// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FactionMenuCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionPopup.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitBrowser/W_FactionUnitBrowser.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitBrowser/W_FactionUnitTab.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitBrowser/W_FactionUnitTile.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/FactionSystem/FactionSelection/TeamColorPicker/W_FactionTeamColorPicker.h"
#include "RTS_Survival/FactionSystem/FactionSelection/TeamColorPicker/W_FactionTeamColorSwatch.h"
#include "RTS_Survival/FactionSystem/TeamColor/FRTS_TeamColor.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFactionMenuCapture, Log, All);

namespace FactionMenuCapturePrivate
{
	const TCHAR* const CommandLineSwitch = TEXT("FactionMenuCapture");
	const TCHAR* const TeamColorsSwitch = TEXT("FactionMenuCaptureTeamColors");
	// Red, blue, yellow, then the camo default again to show the preview goes back to its camo.
	const int32 TeamColorSwatchesToClick[] = {1, 2, 4, 0};
	// Shaders compile on the first frames of a fresh run; the first picture waits for them.
	constexpr float FirstStepDelaySeconds = 12.f;
	// Long enough for the preview unit a click spawns and for the screenshot of the previous step to be written.
	constexpr float StepSettleSeconds = 3.f;
	constexpr float ScreenshotWriteSeconds = 1.f;
	// More than the browser can show (ALL plus eight unit classes); missing tabs are skipped.
	constexpr int32 MaxTabsToTry = 9;
	// A tile past the first, so the capture shows a selection that is not the default one.
	constexpr int32 TileToClick = 3;
	const ERTSFaction Factions[] = {ERTSFaction::GerBreakthroughDoctrine, ERTSFaction::GerStrikeDivision};

	template <typename WidgetType>
	WidgetType* FindLiveWidget(const UWorld* World)
	{
		for (TObjectIterator<WidgetType> WidgetIt; WidgetIt; ++WidgetIt)
		{
			if (WidgetIt->GetWorld() == World && WidgetIt->IsInViewport())
			{
				return *WidgetIt;
			}
		}
		return nullptr;
	}
}

bool UFactionMenuCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), FactionMenuCapturePrivate::CommandLineSwitch);
}

TStatId UFactionMenuCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFactionMenuCaptureSubsystem, STATGROUP_Tickables);
}

void UFactionMenuCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	M_OriginalTeamColor = FRTS_TeamColor::GetPlayerTeamColor(&InWorld);
	if (FParse::Param(FCommandLine::Get(), FactionMenuCapturePrivate::TeamColorsSwitch))
	{
		BuildTeamColorSteps();
	}
	else
	{
		BuildSteps();
	}
	bM_IsActive = true;
	M_NextStepRealSeconds = InWorld.GetRealTimeSeconds() + FactionMenuCapturePrivate::FirstStepDelaySeconds;
	UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE started steps=%d"), M_Steps.Num());
}

void UFactionMenuCaptureSubsystem::BuildSteps()
{
	using namespace FactionMenuCapturePrivate;
	for (const ERTSFaction Faction : Factions)
	{
		M_Steps.Add({Faction, INDEX_NONE, INDEX_NONE});
		for (int32 TabIndex = 0; TabIndex < MaxTabsToTry; ++TabIndex)
		{
			M_Steps.Add({Faction, TabIndex, INDEX_NONE});
		}
		M_Steps.Add({Faction, 0, TileToClick});
	}
}

void UFactionMenuCaptureSubsystem::BuildTeamColorSteps()
{
	using namespace FactionMenuCapturePrivate;
	M_Steps.Add({ERTSFaction::GerBreakthroughDoctrine, INDEX_NONE, INDEX_NONE, INDEX_NONE});
	for (const int32 SwatchIndex : TeamColorSwatchesToClick)
	{
		M_Steps.Add({ERTSFaction::GerBreakthroughDoctrine, INDEX_NONE, INDEX_NONE, SwatchIndex});
	}
}

void UFactionMenuCaptureSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not bM_IsActive || not IsValid(World))
	{
		return;
	}
	if (UW_FactionPopup* Popup = FactionMenuCapturePrivate::FindLiveWidget<UW_FactionPopup>(World))
	{
		Popup->OnPopupAccepted.Broadcast();
		return;
	}
	if (not FindMenu() || World->GetRealTimeSeconds() < M_NextStepRealSeconds)
	{
		return;
	}
	if (bM_IsWaitingForScreenshot)
	{
		bM_IsWaitingForScreenshot = false;
		ApplyNextStep(*World);
		return;
	}
	// The step applied one settle period ago is now on screen: record it, then wait for the file before the next click.
	if (M_Steps.IsValidIndex(M_StepIndex))
	{
		const FString StepName = GetStepName(M_Steps[M_StepIndex]);
		LogShownUnits(StepName);
		LogPreviewTeamColor(StepName);
		Capture(StepName);
		bM_IsWaitingForScreenshot = true;
		M_NextStepRealSeconds = World->GetRealTimeSeconds() + FactionMenuCapturePrivate::ScreenshotWriteSeconds;
		return;
	}
	ApplyNextStep(*World);
}

void UFactionMenuCaptureSubsystem::ApplyNextStep(const UWorld& World)
{
	M_NextStepRealSeconds = World.GetRealTimeSeconds() + FactionMenuCapturePrivate::StepSettleSeconds;
	while (true)
	{
		++M_StepIndex;
		if (not M_Steps.IsValidIndex(M_StepIndex))
		{
			// One more settle period so the last screenshot is written before the exit.
			if (M_StepIndex > M_Steps.Num())
			{
				Finish();
			}
			return;
		}
		if (ApplyStep(M_Steps[M_StepIndex]))
		{
			return;
		}
	}
}

bool UFactionMenuCaptureSubsystem::FindMenu()
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	M_Menu = FactionMenuCapturePrivate::FindLiveWidget<UW_FactionSelectionMenu>(GetWorld());
	return M_Menu.IsValid();
}

UW_FactionUnitBrowser* UFactionMenuCaptureSubsystem::GetBrowser() const
{
	return M_Menu.IsValid() ? M_Menu->GetUnitBrowser() : nullptr;
}

bool UFactionMenuCaptureSubsystem::ApplyStep(const FCaptureStep& Step)
{
	if (Step.TeamColorSwatch != INDEX_NONE)
	{
		return ApplyTeamColorStep(Step);
	}
	if (Step.TabIndex == INDEX_NONE)
	{
		M_Menu->ShowFaction(Step.Faction);
		return true;
	}
	const UW_FactionUnitBrowser* Browser = GetBrowser();
	if (not IsValid(Browser))
	{
		UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE no_unit_browser"));
		return false;
	}
	UW_FactionUnitTab* Tab = Browser->GetTab(Step.TabIndex);
	if (not IsValid(Tab))
	{
		return false;
	}
	Tab->SimulateClick();
	if (Step.TileIndex == INDEX_NONE)
	{
		return true;
	}
	UW_FactionUnitTile* Tile = Browser->GetTile(Step.TileIndex);
	if (not IsValid(Tile))
	{
		return false;
	}
	Tile->SimulateClick();
	return true;
}

bool UFactionMenuCaptureSubsystem::ApplyTeamColorStep(const FCaptureStep& Step)
{
	const UW_FactionTeamColorPicker* Picker = M_Menu->GetTeamColorPicker();
	UW_FactionTeamColorSwatch* Swatch = IsValid(Picker) ? Picker->GetSwatch(Step.TeamColorSwatch) : nullptr;
	if (not IsValid(Swatch))
	{
		UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE no_team_color_swatch %d"),
		       Step.TeamColorSwatch);
		return false;
	}
	bM_HasChangedTeamColor = true;
	Swatch->SimulateClick();
	return true;
}

void UFactionMenuCaptureSubsystem::LogPreviewTeamColor(const FString& StepName) const
{
	constexpr int32 TeamColorComponents = 4;
	for (TActorIterator<ATankMaster> TankIterator(GetWorld()); TankIterator; ++TankIterator)
	{
		const UPrimitiveComponent* Mesh = TankIterator->FindComponentByClass<UPrimitiveComponent>();
		const TArray<float>& Data = IsValid(Mesh) ? Mesh->GetCustomPrimitiveData().Data : TArray<float>();
		FString TeamColorText = TEXT("unset");
		if (Data.Num() >= FRTS_TeamColor::TeamColorPrimitiveDataIndex + TeamColorComponents)
		{
			const int32 First = FRTS_TeamColor::TeamColorPrimitiveDataIndex;
			TeamColorText = FString::Printf(TEXT("%.2f,%.2f,%.2f,%.2f"), Data[First], Data[First + 1], Data[First + 2],
			                                Data[First + 3]);
		}
		UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE %s preview=%s team_color=%s"),
		       *StepName, *TankIterator->GetName(), *TeamColorText);
	}
}

void UFactionMenuCaptureSubsystem::RestoreTeamColor()
{
	if (not bM_HasChangedTeamColor || not M_Menu.IsValid())
	{
		return;
	}
	M_Menu->HandleTeamColorSelected(M_OriginalTeamColor);
}

void UFactionMenuCaptureSubsystem::LogShownUnits(const FString& StepName) const
{
	const UW_FactionUnitBrowser* Browser = GetBrowser();
	if (not IsValid(Browser))
	{
		UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE %s no_unit_browser"), *StepName);
		return;
	}
	TArray<FString> TabLabels;
	for (int32 TabIndex = 0; TabIndex < Browser->GetTabCount(); ++TabIndex)
	{
		TabLabels.Add(Browser->GetTabLabel(TabIndex).ToString());
	}
	TArray<FString> UnitNames;
	for (const FTrainingOption& Unit : Browser->GetShownUnits())
	{
		UnitNames.Add(Unit.GetTrainingName());
	}
	UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE %s tabs=%s shown=%d units=%s"), *StepName,
	       *FString::Join(TabLabels, TEXT(",")), UnitNames.Num(), *FString::Join(UnitNames, TEXT(",")));
}

void UFactionMenuCaptureSubsystem::Capture(const FString& StepName) const
{
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("FactionMenu") / (StepName + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
}

FString UFactionMenuCaptureSubsystem::GetStepName(const FCaptureStep& Step) const
{
	FString FactionName = UEnum::GetValueAsString(Step.Faction);
	FactionName.RemoveFromStart(TEXT("ERTSFaction::"));
	if (Step.TeamColorSwatch != INDEX_NONE)
	{
		return FactionName + FString::Printf(TEXT("_TeamColor%d"), Step.TeamColorSwatch);
	}
	if (Step.TabIndex == INDEX_NONE)
	{
		return FactionName + TEXT("_Default");
	}
	if (Step.TileIndex != INDEX_NONE)
	{
		return FactionName + FString::Printf(TEXT("_Tile%d"), Step.TileIndex);
	}
	const UW_FactionUnitBrowser* Browser = GetBrowser();
	const FString TabLabel = IsValid(Browser) ? Browser->GetTabLabel(Step.TabIndex).ToString() : TEXT("Missing");
	return FactionName + FString::Printf(TEXT("_Tab%d_"), Step.TabIndex) + TabLabel;
}

void UFactionMenuCaptureSubsystem::Finish()
{
	bM_IsActive = false;
	RestoreTeamColor();
	UE_LOG(LogRTSFactionMenuCapture, Display, TEXT("RTS_FACTION_MENU_CAPTURE done"));
	RTSFunctionLibrary::PopModalDialogSuppression();
	FPlatformMisc::RequestExit(false, TEXT("FactionMenuCapture"));
}

#else

bool UFactionMenuCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UFactionMenuCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UFactionMenuCaptureSubsystem::Tick(const float DeltaTime)
{
}

TStatId UFactionMenuCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFactionMenuCaptureSubsystem, STATGROUP_Tickables);
}

#endif
