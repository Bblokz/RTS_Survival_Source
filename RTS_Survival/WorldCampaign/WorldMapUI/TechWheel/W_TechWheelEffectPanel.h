// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "W_TechWheelEffectPanel.generated.h"

class UDataTable;
class UScrollBox;
class UTechWheelEffectProbes;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWrapBox;
struct FTechWheelWeaponChange;
struct FTrainingOption;
struct FWorldCampaignUILook;
enum class EUnitVariantStatTrend : uint8;

/**
 * @brief The part of the tech wheel's detail panel that says what a technology does in a mission: the units it
 * applies to, and for each weapon it changes the changed stats as the pictograms and pips of the unit variant
 * screen, with the pips gained in green and the pips lost in red. Created and fed by UW_WorldTechWheel.
 * @note SetupPanel: call once after CreateWidget, before ShowTechnology.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TechWheelEffectPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupPanel(const FWorldCampaignUILook& Look);

	/** Shows the technology's units at once and its weapon changes when the probes have read them. */
	void ShowTechnology(const ETechnology Technology);

	/** Empties the panel and stops probes that are still settling. */
	void ClearPanel();

protected:
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void LoadSharedAssets();
	void AddUnitChip(const FTrainingOption& Unit);
	void HandleProbesRead(const ETechnology Technology);
	void AddWeaponBlock(const FTechWheelWeaponChange& Change);

	/**
	 * @brief One changed stat as a pictogram and ten pips: the level both share in the accent colour, pips the
	 * technology adds in green, pips it takes away in red.
	 * @param StatIcon The stat's pictogram.
	 * @param BeforeLevel 1-10 without the technology.
	 * @param AfterLevel 1-10 with it.
	 * @param Trend Better or worse; moves AfterLevel at least one pip from BeforeLevel so every change is visible.
	 * @return The bar.
	 */
	UWidget* BuildChangeBar(UTexture2D* StatIcon, const int32 BeforeLevel, const int32 AfterLevel,
	                        const EUnitVariantStatTrend Trend);
	void AddChangedStatBars(UVerticalBox& Rows, const FTechWheelWeaponChange& Change);
	UTextBlock* AddSectionLabel(const FText& Label);

	bool GetIsValidColumn() const;
	bool GetIsValidProbes() const;

	// Owned by the campaign HUD, which outlives this panel.
	const FWorldCampaignUILook* M_Look = nullptr;

	// The variant screen's weapon icon and stat pictograms, in the campaign's fonts and colours.
	UPROPERTY()
	FUnitVariantSelectionStyle M_WeaponStyle;

	UPROPERTY()
	TObjectPtr<UTechWheelEffectProbes> M_Probes;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_WeaponCardTable;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_DamageStatIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_PenetrationStatIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_ReloadStatIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_MagazineStatIcon;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_Column;

	UPROPERTY()
	TObjectPtr<UWrapBox> M_UnitsBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_WeaponsLabel;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_WeaponsBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_StatusText;

	ETechnology M_ShownTechnology = ETechnology::Tech_NONE;
};
