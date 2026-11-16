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
 * picture, production year, armour and the weapons with the unit variant screen's pictograms. Squads open in the same
 * widget (SetupSquad) with soldiers and health in place of the armour. Opened by UInfoTakeoverSubsystem from the
 * mission nodes; its Blueprint child (WBP_VehicleInfoTakeover) holds the style for both.
 */
UCLASS()
class RTS_SURVIVAL_API UW_VehicleInfoTakeover : public UW_InfoTakeover
{
	GENERATED_BODY()

public:
	/** Builds the body for Content; call before OpenTakeover. */
	void SetupVehicle(const FVehicleInfoTakeoverContent& Content);

	/** Builds the squad body for Content; call before OpenTakeover. */
	void SetupSquad(const FSquadInfoTakeoverContent& Content);

	// Unit intel: the player may switch these off.
	virtual bool GetOffersNeverShowAgain() const override { return true; }

private:
	// What the body shows; for a squad only the parts both kinds share (name, text, picture, weapons) are filled.
	UPROPERTY()
	FVehicleInfoTakeoverContent M_Content;

	UPROPERTY()
	FSquadInfoTakeoverContent M_SquadContent;

	// Vehicle or Squad: picks the specification rows.
	EInfoTakeoverKind M_UnitKind = EInfoTakeoverKind::Vehicle;

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
	UWidget* BuildGlassPane(UWidget* Content);

	void AddSectionLabel(UVerticalBox& Column, const FText& Label, const bool bIsFirstInColumn);
	void AddSpecRow(const FText& Label, const FText& Value);
	void SetupUnit(const FText& KindLabel);
	void AddSpecificationRows();
	void AddArmorRows();
	void AddSquadRows();
	void AddWeaponRows();
	void AddWeaponRow(const FUnitVariantWeaponStats& Weapon);

	static FText FormatMm(const float Millimetres);
	static FText FormatPlate(const FInfoTakeoverWeakestPlate& Plate);
};
