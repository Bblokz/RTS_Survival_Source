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
 * What an info takeover introduces. Vehicles and squads exist now (both in the vehicle widget, each with its own
 * library); buildings come later and share the pause, the typewriter, the sounds and the one "never show again" switch.
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
	BxpPackUp,
	// How the Hazmat Vultures' AT stick grenade volley works, with a short in-game clip; opened by the Show Vultures AT
	// Grenade Takeover node.
	VulturesAt,
	// How the Panzer II's Rattle-Feed autocannon burst works, with a short in-game clip; opened by the Show Rattle-Feed
	// Takeover node.
	RattleFeed,
	// How the Panzer 38(t)'s Adrenaline Shot works, with a short in-game clip; opened by the Show Adrenaline Shot
	// Takeover node.
	AdrenalineShot,
	// How the Wespe's Barrage aim ability works, with a short in-game clip; opened by the Show Wespe Barrage Takeover
	// node.
	WespeBarrage,
	// How a tank digs in behind a sandbag wall, with a short in-game clip; opened by the Show Dig In Takeover node.
	DigIn
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

/**
 * @brief One prefab of the squad info takeover library (DT_SquadInfoTakeovers): the hand-written part of the widget.
 * Soldiers, health and weapons are read off the live squad (or solo infantry unit) the mission passes.
 */
USTRUCT(BlueprintType)
struct FSquadInfoTakeoverEntry : public FTableRowBase
{
	GENERATED_BODY()

	// Shown as the typewritten title; empty uses the unit's training title or subtype name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Info")
	FText DisplayName;

	// The text left of the picture. Any length: the column wraps and scrolls. Rich text tags of the body style set work.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Info", meta = (MultiLine = true))
	FText Description;

	// The year the squad's kit was fielded, "1942"; "????" for fictional squads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Info")
	FText ServiceYear;

	// The picture; empty uses the unit's DT_TrainingOptions brush, else no picture is shown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Info")
	TSoftObjectPtr<UTexture2D> Image;

	// The squad controller or solo infantry unit this entry was written for; a reference only, not used by the node.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Info")
	TSoftClassPtr<AActor> ReferenceSquadClass;
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

/**
 * @brief Everything a squad info takeover shows, taken when the mission node is called: a queued takeover can open
 * after the squad died, so the widget never reads the squad itself.
 */
USTRUCT()
struct FSquadInfoTakeoverContent
{
	GENERATED_BODY()

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	FText ServiceYear;

	// The entry's texture, kept loaded by the content while it waits in the queue.
	UPROPERTY()
	TObjectPtr<UTexture2D> ImageTexture = nullptr;

	// Training brush when the entry gave no texture.
	UPROPERTY()
	FSlateBrush ImageBrush;

	UPROPERTY()
	bool bHasImage = false;

	// The weapons of every soldier, identical ones merged (4x Mosin rifle).
	UPROPERTY()
	TArray<FUnitVariantWeaponStats> Weapons;

	// Living soldiers when the node was called.
	UPROPERTY()
	int32 SoldierCount = 0;

	// Max health of the toughest soldier; zero when unknown.
	UPROPERTY()
	float HealthPerSoldier = 0.f;
};
