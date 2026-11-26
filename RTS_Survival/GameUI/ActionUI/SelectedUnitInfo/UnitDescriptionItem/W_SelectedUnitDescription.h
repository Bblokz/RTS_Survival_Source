// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GameUI/Healthbar/Healthbar_TargetTypeIconState/TargetTypeIconState.h"
#include "W_SelectedUnitDescription.generated.h"

enum class ERTSRichText : uint8;
class UTacticalIconCatalog;
struct FTacticalIconInfo;
struct FFireResistanceData;
class UHorizontalBox;
class UPanelWidget;
struct FResistanceAndDamageReductionData;
class UArmorCalculation;
enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
enum class ENomadicSubtype : uint8;
enum class EAllUnitType : uint8;
class UVerticalBox;
class USizeBox;
class UImage;
class UArmor;

/**
 *  The hover logic of this happens in the Tick of W_SelectedUnitInfo
 */
UCLASS()
class RTS_SURVIVAL_API UW_SelectedUnitDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupUnitDescription(const AActor* SelectedActor,
	                          const EAllUnitType PrimaryUnitType,
	                          const ENomadicSubtype NomadicSubtype,
	                          const ETankSubtype TankSubtype,
	                          const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype);

	/**
	 * @brief Shows the tactical icons engraved on the unit portrait, each with a one-line explanation, in place of the
	 * armor type icon and name. Falls back to the armor type for portraits without tactical icons.
	 * Cheap to call every tick: does nothing while the portrait stays the same.
	 * @param PortraitResource The resource object of the selected unit's portrait brush.
	 */
	void SetupTacticalIcons(const UObject* PortraitResource);

protected:

	void NativeOnInitialized() override;
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* UnitDescription;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UVerticalBox> HullArmorBox;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* HullArmorDescription;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UVerticalBox> TurretArmorBox;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* TurretArmorDescription;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UVerticalBox* FireResistanceBox;
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* FireResistanceDescription;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UVerticalBox* LaserResistanceBox;
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* LaserResistanceDescription;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UVerticalBox* RadiationResistanceBox;
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* RadiationResistanceDescription;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UVerticalBox* AmmoTypeBox;
	// Armor type icon + name; only shown for portraits without tactical icons. The overlay holds both.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UPanelWidget> ArmorTypeOverlay;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	UImage* TargetTypeIcon;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	URichTextBlock* ArmorTypeText;

	// The tactical icons of the portrait (left corner first), each a row of badge + one-line explanation.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UVerticalBox> TacticalIconsBox;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UHorizontalBox> TacticalIconRow1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UImage> TacticalIconImage1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> TacticalIconText1;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UHorizontalBox> TacticalIconRow2;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UImage> TacticalIconImage2;
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> TacticalIconText2;

	// Which tactical icons each portrait carries (DA_TacticalIconCatalog). Hard reference so the badges are cooked.
	UPROPERTY(EditDefaultsOnly, Category = "TacticalIcons")
	TObjectPtr<UTacticalIconCatalog> TacticalIconCatalog;
	

	UFUNCTION(BlueprintImplementableEvent)
	void OnSetupUnitDescription(const EAllUnitType PrimaryUnitType,
	                            const ENomadicSubtype NomadicSubtype,
	                            const ETankSubtype TankSubtype,
	                            const ESquadSubtype SquadSubtype, const FResistanceAndDamageReductionData ResistanceData, const EBuildingExpansionType
	                            BxpSubtype);

	
	/** Formats a float to text with a specified number of decimal places */
	UFUNCTION(BlueprintCallable, Category = "WeaponDescription")
	FText FormatFloatToText(float Value, int32 DecimalPlaces) const;

private:
	void InitArmorToStringMap(const TArray<UArmor*>& ArmorLayout);

	void SetupHullArmor(UArmorCalculation* ArmorCalculation);
	void SetupTurretArmor(UArmorCalculation* ArmorCalculation);
	void SetupResistanceDescriptions(const FResistanceAndDamageReductionData& ResistanceData, const AActor* SelectedActor);

	void SetupArmorIcon(const ETargetTypeIcon NewTargetTypeIcon, const AActor* SelectedActor);
	void SetupLaserResistance(const FDamageMltPerSide& LaserMltPerPart) const;
	void SetupRadiationResistance(const FDamageMltPerSide& RadiationMltPerPart) const;
	void SetupFireResistance(const FFireResistanceData& FireResistanceData) const;

	FString GetMltPerPartDescription(const FDamageMltPerSide& MltPerPart) const;
	int32 GetDmgPercentageFromMlt(const float Mlt) const;

	FString RTSRichtText(const FString& Text, const ERTSRichText TextType) const;

	void SetupTacticalIconRow(UHorizontalBox* Row, UImage* Image, URichTextBlock* Text,
	                          const FTacticalIconInfo* IconInfo) const;
	void SetArmorTypeVisible(const bool bVisible) const;
	FString GetTacticalIconRichDescription(const FText& Description) const;

	// The portrait the tactical icons were last set up for; null when never set up.
	TWeakObjectPtr<const UObject> M_TacticalIconsPortrait;
	bool bM_HasSetupTacticalIcons = false;


	UPROPERTY()
	FTargetTypeIconState M_TargetTypeIconState;

	
};
