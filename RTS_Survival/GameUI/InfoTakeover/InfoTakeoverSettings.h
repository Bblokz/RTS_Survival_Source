// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "InfoTakeoverSettings.generated.h"

class UDataTable;
class UW_CameraControlsTakeover;
class UW_CoverTakeover;
class UW_NomadicConversionTakeover;
class UW_BxpPackUpTakeover;
class UW_SpacedArmourTakeover;
class UW_VehicleInfoTakeover;

/**
 * @brief Project Settings > Game > Info Takeovers: which widget Blueprint and which prefab library the mission nodes
 * use. Defaults point at GameUI/InfoTakeover, so nothing has to be set for the shipped assets.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Info Takeovers"))
class RTS_SURVIVAL_API UInfoTakeoverSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UInfoTakeoverSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	static const UInfoTakeoverSettings* Get() { return GetDefault<UInfoTakeoverSettings>(); }

	// WBP_VehicleInfoTakeover: the Blueprint child that carries the style.
	UPROPERTY(EditAnywhere, Config, Category = "Vehicles")
	TSoftClassPtr<UW_VehicleInfoTakeover> VehicleTakeoverWidgetClass;

	// DT_VehicleInfoTakeovers: one row (FVehicleInfoTakeoverEntry) per prefab, picked by row name in the mission node.
	UPROPERTY(EditAnywhere, Config, Category = "Vehicles",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/RTS_Survival.VehicleInfoTakeoverEntry"))
	TSoftObjectPtr<UDataTable> VehicleLibrary;

	// WBP_CameraControlsTakeover: the Blueprint child that carries the style and the animated illustrations.
	UPROPERTY(EditAnywhere, Config, Category = "Tutorials")
	TSoftClassPtr<UW_CameraControlsTakeover> CameraControlsTakeoverWidgetClass;

	// WBP_CoverTakeover: the Blueprint child that carries the style for the cover field manual.
	UPROPERTY(EditAnywhere, Config, Category = "Tutorials")
	TSoftClassPtr<UW_CoverTakeover> CoverTakeoverWidgetClass;

	// WBP_SpacedArmourTakeover: the Blueprint child that carries the style for the spaced armour field manual.
	UPROPERTY(EditAnywhere, Config, Category = "Tutorials")
	TSoftClassPtr<UW_SpacedArmourTakeover> SpacedArmourTakeoverWidgetClass;

	// WBP_NomadicConversionTakeover: the Blueprint child that carries the style for the nomadic conversion field manual.
	UPROPERTY(EditAnywhere, Config, Category = "Tutorials")
	TSoftClassPtr<UW_NomadicConversionTakeover> NomadicConversionTakeoverWidgetClass;

	// WBP_BxpPackUpTakeover: the Blueprint child that carries the style for the building expansion pack-up field manual.
	UPROPERTY(EditAnywhere, Config, Category = "Tutorials")
	TSoftClassPtr<UW_BxpPackUpTakeover> BxpPackUpTakeoverWidgetClass;
};
