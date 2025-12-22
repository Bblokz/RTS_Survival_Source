// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GameUI/Healthbar/Healthbar_TargetTypeIconState/TargetTypeIconState.h"
#include "W_SelectedUnitDescription.generated.h"

enum class ERTSRichText : uint8;
struct FFireResistanceData;
class UHorizontalBox;
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
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* TargetTypeIcon;
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* ArmorTypeText;
	

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
	void SetupResistanceDescriptions(const FResistanceAndDamageReductionData& ResistanceData);

	void SetupArmorIcon(const ETargetTypeIcon NewTargetTypeIcon);
	void SetupLaserResistance(const FDamageMltPerSide& LaserMltPerPart) const;
	void SetupRadiationResistance(const FDamageMltPerSide& RadiationMltPerPart) const;
	void SetupFireResistance(const FFireResistanceData& FireResistanceData) const;

	FString GetMltPerPartDescription(const FDamageMltPerSide& MltPerPart) const;
	int32 GetDmgPercentageFromMlt(const float Mlt) const;

	FString RTSRichtText(const FString& Text, const ERTSRichText TextType) const;


	UPROPERTY()
	FTargetTypeIconState M_TargetTypeIconState;

	
};
