// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "NomadicAirDeliveryTypes.generated.h"

class AAircraftMaster;
class AGliderTankDelivery;
class ANomadicVehicle;
class UNiagaraSystem;

// Called once per delivery: with the nomadic when it is on the ground and ordered to build, with null when it failed.
DECLARE_DELEGATE_OneParam(FOnNomadicAirDeliveryLanded, ANomadicVehicle*);

UENUM(BlueprintType)
enum class ENomadicAirDeliveryMethod : uint8
{
	// A German cargo plane carries the truck slung underneath it on ropes and lowers it on the build spot.
	CargoPlaneRopes,
	// The Mammut glider (towed in like the super heavy tank drops) lands, the truck drives out to the build spot.
	MammutGlider
};

/**
 * @brief Which aircraft, beacon and landing distances the nomadic air deliveries use. The defaults are the planes
 * the global abilities fly (all already cooked through DA_GlobalAbilitySystem); a mission can override them.
 */
USTRUCT(BlueprintType)
struct FNomadicAirDeliverySettings
{
	GENERATED_BODY()

	// The plane that carries the rope-slung trucks; the German tank drops fly the same Ju 390.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AAircraftMaster> CargoPlaneClass = TSoftClassPtr<AAircraftMaster>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed.Ger_Aircraft_JU390B_AirborneUnarmed_C")));

	// The towed glider of the super heavy tank drops; used for the HQ.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AGliderTankDelivery> HQGliderDeliveryClass = TSoftClassPtr<AGliderTankDelivery>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/BP_GliderTankDelivery_Mammut.BP_GliderTankDelivery_Mammut_C")));

	// The smoke beacon of the global abilities; it burns on the build spot until the truck is down.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UNiagaraSystem> BeaconEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_GlobalAbility_Beacon.NS_GlobalAbility_Beacon")));

	// Set on the beacon's MarkerColor parameter; blue marks a friendly delivery (attack abilities use red).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor BeaconColor = FLinearColor(0.05f, 0.3f, 1.f, 1.f);

	// How long the cargo plane hovers after it let go of the ropes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float CargoPlaneHoverSeconds = 2.f;

	// Skid length of the glider when the mission gives no touchdown point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1000.0"))
	float GliderSkidLength = 5000.f;

	// Extra room between the glider's stop point plus the truck's exit drive and the build spot, so the HQ does not
	// unfold into the empty glider while it lingers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float GliderStopClearance = 1200.f;

	// Longest tow approach before the release; the map edge usually comes first.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float GliderMaxApproachDistance = 30000.f;
};

/** @brief One nomadic to bring in by air and build on the spot; filled in by the mission or the test. */
USTRUCT()
struct FNomadicAirDeliveryOrder
{
	GENERATED_BODY()

	ENomadicAirDeliveryMethod Method = ENomadicAirDeliveryMethod::CargoPlaneRopes;

	ENomadicSubtype NomadicSubtype = ENomadicSubtype::Nomadic_None;

	// Where the building goes; only X and Y count, the ground below is used.
	FVector BuildLocation = FVector::ZeroVector;

	// Only the yaw is used.
	FRotator BuildRotation = FRotator::ZeroRotator;

	// Glider only: where it touches down; it skids toward the build spot. Zero: it comes in from the aircraft spawn
	// side and touches down GliderSkidLength before its stop point.
	FVector GliderTouchdownLocation = FVector::ZeroVector;

	int32 OwningPlayer = 1;

	FNomadicAirDeliverySettings Settings;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnyNomadicAirDeliveryFinished, ANomadicVehicle*, const FNomadicAirDeliveryOrder&);
