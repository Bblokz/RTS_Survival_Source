// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MissionObjectivePlateSettings.generated.h"

class UFont;
class UMaterialInterface;

/**
 * @brief Project settings for the mission panel's objective plates (design A1-c, Source/Docs/UI/MissionObjectivePlates.md).
 * UW_Mission reads them when it builds its plate in C++; W_BP_Mission and the mission assets need no setup.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Mission Objective Plates"))
class RTS_SURVIVAL_API UMissionObjectivePlateSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMissionObjectivePlateSettings();

	/** Off: the mission widgets keep W_BP_Mission's own designer layout (the old backplates). */
	UPROPERTY(Config, EditAnywhere, Category="General")
	bool bUseObjectivePlates = true;

	/** M_UISkinSlice instance of the gunmetal plate behind every objective (the HUD sidebar frame). */
	UPROPERTY(Config, EditAnywhere, Category="Plate")
	TSoftObjectPtr<UMaterialInterface> PlateMaterial;

	/** The BrushSize parameter of PlateMaterial; the brush must use it as its image size. */
	UPROPERTY(Config, EditAnywhere, Category="Plate")
	FVector2D PlateBrushSize = FVector2D(128.0, 128.0);

	/** M_UI_ObjectiveShield instances: chrome bezel and enamel, no icon. */
	UPROPERTY(Config, EditAnywhere, Category="Shield")
	TSoftObjectPtr<UMaterialInterface> MajorShieldMaterial;

	UPROPERTY(Config, EditAnywhere, Category="Shield")
	TSoftObjectPtr<UMaterialInterface> BonusShieldMaterial;

	/** M_UI_ObjectiveGlassIcon instances: the coloured glass star / diamond drawn as its own image on the shield. */
	UPROPERTY(Config, EditAnywhere, Category="Shield")
	TSoftObjectPtr<UMaterialInterface> MajorIconMaterial;

	UPROPERTY(Config, EditAnywhere, Category="Shield")
	TSoftObjectPtr<UMaterialInterface> BonusIconMaterial;

	/** M_UI_ObjectiveKnob: the knurled expand / collapse button (scalars PointUp, Hover, Press). */
	UPROPERTY(Config, EditAnywhere, Category="Buttons")
	TSoftObjectPtr<UMaterialInterface> KnobMaterial;

	/** M_UISkinSlice instances of the OK / Next button. */
	UPROPERTY(Config, EditAnywhere, Category="Buttons")
	TSoftObjectPtr<UMaterialInterface> ConfirmButtonNormal;

	UPROPERTY(Config, EditAnywhere, Category="Buttons")
	TSoftObjectPtr<UMaterialInterface> ConfirmButtonHovered;

	UPROPERTY(Config, EditAnywhere, Category="Buttons")
	TSoftObjectPtr<UMaterialInterface> ConfirmButtonPressed;

	/** The BrushSize parameter of the confirm button instances. */
	UPROPERTY(Config, EditAnywhere, Category="Buttons")
	FVector2D ConfirmButtonBrushSize = FVector2D(368.0, 138.0);

	/** Font of the MAJOR / BONUS labels, untagged titles and the OK / Next button (RR_mainFont is only Faster One). */
	UPROPERTY(Config, EditAnywhere, Category="Text")
	TSoftObjectPtr<UFont> LabelFont;

	/** Typeface of LabelFont used by the plate. */
	UPROPERTY(Config, EditAnywhere, Category="Text")
	FName LabelTypeface = TEXT("Bold");

	/** Untagged mission titles use LabelFont at this size; tagged titles keep their DT_RichText row. */
	UPROPERTY(Config, EditAnywhere, Category="Text", meta=(ClampMin="8"))
	int32 TitleFontSize = 18;

	UPROPERTY(Config, EditAnywhere, Category="Text", meta=(ClampMin="8"))
	int32 ExpandedTitleFontSize = 20;

	UPROPERTY(Config, EditAnywhere, Category="Text")
	FLinearColor TitleColor;

	UPROPERTY(Config, EditAnywhere, Category="Text")
	FLinearColor MajorLabelColor;

	UPROPERTY(Config, EditAnywhere, Category="Text")
	FLinearColor BonusLabelColor;

	UPROPERTY(Config, EditAnywhere, Category="Text")
	FLinearColor ButtonTextColor;

	/** Width of an expanded plate in UI units; the description wraps inside it. */
	UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="200.0"))
	float ExpandedPlateWidth = 540.f;

	/** Narrowest title-only plate in UI units, so short titles still read as plates. */
	UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="100.0"))
	float MinTitlePlateWidth = 300.f;

	/** How far a bonus objective is indented under the major ones, in UI units. */
	UPROPERTY(Config, EditAnywhere, Category="Layout", meta=(ClampMin="0.0"))
	float BonusIndent = 24.f;
};
