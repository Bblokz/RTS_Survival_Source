// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Styling/SlateBrush.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantProbes.h"

#include "InfoTakeoverTypes.generated.h"

class ATankMaster;
class UTexture2D;

/**
 * What an info takeover introduces. Vehicles exist now; buildings and squads get their own widget and library later and
 * share the pause, the typewriter, the sounds and the one "never show these info widgets again" switch.
 */
UENUM(BlueprintType)
enum class EInfoTakeoverKind : uint8
{
	Vehicle,
	Building,
	Squad,
	// The field manual on moving the camera; no unit content, opened by the Show Camera Controls Takeover node.
	CameraControls,
	// What the green, yellow and red squad move preview means; opened by the Show Cover Takeover node.
	Cover,
	// How spaced armour plates protect a tank's sides and come off under fire; opened by the Show Spaced Armour
	// Takeover node.
	SpacedArmour,
	// How a nomadic truck converts into a building and packs up again; opened by the Show Nomadic Conversion Takeover
	// node.
	NomadicConversion,
	// How building expansions pack up with their nomadic building and come back after it deploys; opened by the Show
	// Building Expansion Pack Up Takeover node.
	BxpPackUp
};

/**
 * @brief One prefab of the vehicle info takeover library (DT_VehicleInfoTakeovers): the hand-written part of the
 * widget. Weapons and armour are read off the live vehicle the mission passes, so they always match the balance.
 */
USTRUCT(BlueprintType)
struct FVehicleInfoTakeoverEntry : public FTableRowBase
{
	GENERATED_BODY()

	// Shown as the typewritten title; empty uses the unit's training title or subtype name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Info")
	FText DisplayName;

	// The text left of the picture. Any length: the column wraps and scrolls. Rich text tags of the body style set work.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Info", meta = (MultiLine = true))
	FText Description;

	// "1931"; "????" for fictional vehicles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Info")
	FText ProductionYear;

	// The picture; empty uses the unit's DT_TrainingOptions brush, else no picture is shown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Info")
	TSoftObjectPtr<UTexture2D> Image;

	// The vehicle this entry was written for; tests spawn it to show the entry. Not used by the mission node.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle Info")
	TSoftClassPtr<ATankMaster> ReferenceVehicleClass;
};

/** The thinnest plate of one hull side, as the widget names it. */
USTRUCT()
struct FInfoTakeoverWeakestPlate
{
	GENERATED_BODY()

	UPROPERTY()
	FText PlateName;

	UPROPERTY()
	float ArmorMm = 0.f;

	// Add-on panels on that plate; they can be shot off.
	UPROPERTY()
	float AddOnArmorMm = 0.f;

	UPROPERTY()
	bool bIsValid = false;
};

/** The armour lines of a vehicle info takeover, read from the registered plates of the live vehicle. */
USTRUCT()
struct FInfoTakeoverArmorSummary
{
	GENERATED_BODY()

	UPROPERTY()
	float FrontMinMm = 0.f;

	UPROPERTY()
	float FrontMaxMm = 0.f;

	UPROPERTY()
	bool bHasFront = false;

	// Highest add-on mm over the front plates; zero without panels.
	UPROPERTY()
	float FrontAddOnMaxMm = 0.f;

	UPROPERTY()
	FInfoTakeoverWeakestPlate WeakestSide;

	UPROPERTY()
	FInfoTakeoverWeakestPlate WeakestRear;
};

/**
 * @brief Everything a vehicle info takeover shows, taken when the mission node is called: a queued takeover can open
 * after the vehicle died, so the widget never reads the vehicle itself.
 */
USTRUCT()
struct FVehicleInfoTakeoverContent
{
	GENERATED_BODY()

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	FText ProductionYear;

	// The entry's texture, kept loaded by the content while it waits in the queue.
	UPROPERTY()
	TObjectPtr<UTexture2D> ImageTexture = nullptr;

	// Training brush when the entry gave no texture.
	UPROPERTY()
	FSlateBrush ImageBrush;

	UPROPERTY()
	bool bHasImage = false;

	UPROPERTY()
	TArray<FUnitVariantWeaponStats> Weapons;

	UPROPERTY()
	FInfoTakeoverArmorSummary Armor;
};
