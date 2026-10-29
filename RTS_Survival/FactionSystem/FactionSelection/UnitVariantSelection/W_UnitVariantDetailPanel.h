// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnitVariantAbilityCards.h"
#include "UnitVariantSelectionStyle.h"

#include "W_UnitVariantDetailPanel.generated.h"

class UBorder;
class UDataTable;
class UTexture2D;
class UTextBlock;
class UVerticalBox;
class UWidget;
struct FUnitVariantProbeResult;
struct FUnitVariantStatRow;
struct FUnitVariantWeaponStats;
enum class EUnitVariantStatTrend : uint8;

/** How an ability or weapon of the shown variant compares with the Base variant of the same unit. */
UENUM()
enum class EUnitVariantChange : uint8
{
	// Base itself is shown, or the variant keeps it.
	Unchanged,
	Gained,
	Removed
};

/** Everything the panel shows for one variant; the pointers stay owned by the menu. */
struct FUnitVariantDetailContent
{
	FText VariantName;
	const TArray<FUnitVariantAbilityCard>* AbilityCards = nullptr;
	// Null when Base itself is shown, which tags nothing.
	const TArray<FUnitVariantAbilityCard>* BaseAbilityCards = nullptr;
	// Null while the variant's probe has not been read yet.
	const FUnitVariantProbeResult* Probe = nullptr;
	// Null when Base itself is shown or its probe has not been read yet.
	const FUnitVariantProbeResult* BaseProbe = nullptr;
};

/**
 * @brief Left of the variant cards on the unit variant selection screen: the special abilities, the weapons and the
 * stat changes of the hovered or equipped variant against Base. Created and placed by UW_UnitVariantSelectionMenu,
 * which resolves the data; builds its own widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UnitVariantDetailPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @param Style Brushes, fonts and colours shared by the whole screen.
	 * @param RichTextStyle DT_RichText, for the <Style> tags of the action bar's ability descriptions.
	 * @param WeaponCardTable WeaponUI_Data, the weapon bar's icon per weapon.
	 */
	void SetupPanel(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
	                const UDataTable* WeaponCardTable);

	void ShowVariant(const FUnitVariantDetailContent& Content);
	void HidePanel();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> M_Plate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantNameText;

	// Labels and lists of the three sections; rebuilt for every variant shown.
	UPROPERTY()
	TObjectPtr<UVerticalBox> M_SectionColumn;

	UPROPERTY()
	TObjectPtr<UDataTable> M_RichTextStyle;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_WeaponCardTable;

	UPROPERTY()
	FUnitVariantSelectionStyle M_Style;

	// The pictograms in front of the weapon stat bars, loaded from M_Style.
	UPROPERTY()
	TObjectPtr<UTexture2D> M_DamageStatIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_PenetrationStatIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> M_ReloadStatIcon;

	bool bM_NextRowFollowsLabel = false;

	void BuildWidgetTree();
	void AddSectionLabel(const FText& Label);
	void AddMutedLine(const FText& Text);
	void AddAbilitySection(const FUnitVariantDetailContent& Content);
	void AddWeaponSection(const FUnitVariantDetailContent& Content);
	void AddComparisonSection(const FUnitVariantDetailContent& Content);
	void AddAbilityRow(const FUnitVariantAbilityCard& AbilityCard, const EUnitVariantChange Change);
	/**
	 * @param Weapon The weapon to list.
	 * @param Change NEW / REMOVED tag against Base.
	 * @param BaseWeapon The Base weapon it is compared with; null keeps its pips neutral.
	 */
	void AddWeaponRow(const FUnitVariantWeaponStats& Weapon, const EUnitVariantChange Change,
	                  const FUnitVariantWeaponStats* BaseWeapon);
	void AddStatRow(const FUnitVariantStatRow& StatRow);

	/**
	 * @brief A weapon stat as a pictogram and UnitVariantStatComparison::MaxStatLevel pips, Level of them filled.
	 * @param StatIcon The stat's pictogram; the bar is drawn without it when it is missing.
	 * @param Level 1 to MaxStatLevel.
	 * @param Trend Against Base: green filled pips when better, red when worse, the accent colour otherwise.
	 * @return The bar.
	 */
	UWidget* BuildStatBar(UTexture2D* StatIcon, const int32 Level, const EUnitVariantStatTrend Trend);

	/**
	 * @brief A row of the panel: an icon in a well, then a title (with its change tag) over a detail widget.
	 * @param Icon Goes into the well.
	 * @param IconSize Width and height of the well.
	 * @param Title Upper-cased title.
	 * @param Change Adds the NEW or REMOVED tag and dims removed rows.
	 * @param Detail Under the title (description or stat line).
	 */
	void AddIconRow(UWidget* Icon, const FVector2D& IconSize, const FText& Title, const EUnitVariantChange Change,
	                UWidget* Detail);
	UWidget* BuildTitleRow(const FText& Title, const EUnitVariantChange Change);
	void AddToSection(UWidget* Row);

	bool GetIsValidPlate() const;
};
