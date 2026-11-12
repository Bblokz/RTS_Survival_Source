// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "W_InfoTakeover.h"

#include "W_VehicleInfoTakeover.generated.h"

class UDataTable;
class UTexture2D;
class UVerticalBox;
class UWidget;

/**
 * @brief The vehicle info takeover: typewritten name, the designer's description on the left, and on the right the
 * picture, production year, armour and the weapons with the unit variant screen's pictograms. Opened by
 * UInfoTakeoverSubsystem from the mission nodes; its Blueprint child (WBP_VehicleInfoTakeover) holds the style.
 */
UCLASS()
class RTS_SURVIVAL_API UW_VehicleInfoTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	/** Builds the body for Content; call before OpenTakeover. */
	void SetupVehicle(const FVehicleInfoTakeoverContent& Content);

private:
	UPROPERTY()
	FVehicleInfoTakeoverContent M_Content;

	// WeaponUI_Data and the stat pictograms, kept loaded while the widget lives.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_WeaponCardTable;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_StatIcons;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_SpecColumn;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_WeaponColumn;

	void BuildBody();
	UWidget* BuildDescriptionColumn();
	UWidget* BuildFactsColumn();
	UWidget* BuildPicture();
	UWidget* BuildGlassPane(UWidget* Content) const;

	void AddSectionLabel(UVerticalBox& Column, const FText& Label, const bool bIsFirstInColumn);
	void AddSpecRow(const FText& Label, const FText& Value);
	void AddArmorRows();
	void AddWeaponRows();
	void AddWeaponRow(const FUnitVariantWeaponStats& Weapon);

	static FText FormatMm(const float Millimetres);
	static FText FormatPlate(const FInfoTakeoverWeakestPlate& Plate);
};
