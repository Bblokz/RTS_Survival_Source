// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "FactionMenuCaptureSubsystem.generated.h"

class UW_FactionSelectionMenu;
class UW_FactionUnitBrowser;
enum class ERTSFaction : uint8;

/**
 * @brief Offscreen check of the faction selection menu on RTS_FactionSelection, started with -FactionMenuCapture.
 * For each German faction it clicks every unit browser tab and one unit tile (through their buttons, as a player
 * would), logs which units each tab shows and screenshots every state to Saved/FactionMenu/. Logs
 * RTS_FACTION_MENU_CAPTURE. With -FactionMenuCaptureTeamColors it instead clicks team colour swatches and logs the
 * colour the hangar preview vehicle received; the player's colour is restored afterwards.
 */
UCLASS()
class RTS_SURVIVAL_API UFactionMenuCaptureSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FCaptureStep
	{
		ERTSFaction Faction;
		// INDEX_NONE for the faction's default view, else the browser tab to click.
		int32 TabIndex = INDEX_NONE;
		// INDEX_NONE to click no tile, else the tile to click after the tab.
		int32 TileIndex = INDEX_NONE;
		// INDEX_NONE to click no team colour, else the swatch to click (0 is the camo default).
		int32 TeamColorSwatch = INDEX_NONE;
	};

	TArray<FCaptureStep> M_Steps;
	int32 M_StepIndex = INDEX_NONE;
	bool bM_IsActive = false;
	// The screenshot of a step is written frames after the request; the next click waits for it.
	bool bM_IsWaitingForScreenshot = false;
	float M_NextStepRealSeconds = 0.f;
	// The team colour the player had before the run; the swatch steps save to the profile, so it is put back.
	FLinearColor M_OriginalTeamColor = FLinearColor::Transparent;
	bool bM_HasChangedTeamColor = false;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_Menu;

	void BuildSteps();
	void BuildTeamColorSteps();
	bool ApplyTeamColorStep(const FCaptureStep& Step);
	// Logs the custom primitive data team colour of the player's vehicles in the hangar.
	void LogPreviewTeamColor(const FString& StepName) const;
	void RestoreTeamColor();
	bool FindMenu();
	UW_FactionUnitBrowser* GetBrowser() const;
	/** @return False when the step's tab or tile does not exist for this faction, so it is skipped. */
	bool ApplyStep(const FCaptureStep& Step);
	void LogShownUnits(const FString& StepName) const;
	void Capture(const FString& StepName) const;
	FString GetStepName(const FCaptureStep& Step) const;
	void ApplyNextStep(const UWorld& World);
	void Finish();
};
