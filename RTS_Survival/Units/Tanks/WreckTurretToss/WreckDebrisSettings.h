#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"

#include "WreckDebrisSettings.generated.h"

class UStaticMesh;

/** One kind of debris piece thrown out of a hull when the ammunition detonates. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckDebrisPieceType
{
	GENERATED_BODY()

	// Needs simple collision; pieces without it cannot simulate and are skipped.
	UPROPERTY(EditAnywhere, Category="Debris")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category="Debris", meta=(ClampMin="0.01"))
	FVector2D ScaleRange = FVector2D(1.0, 1.0);

	// Paint the piece with the wreck hull's material so generic chunks read as that vehicle's armour.
	UPROPERTY(EditAnywhere, Category="Debris")
	bool bUseWreckHullMaterial = false;

	// Relative chance of picking this piece type.
	UPROPERTY(EditAnywhere, Category="Debris", meta=(ClampMin="0"))
	float Weight = 1.f;
};

/** One kind of debris burst: which pieces fly, how many, how hard and how many of them burn. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckDebrisBurstProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Pieces")
	TArray<FWreckDebrisPieceType> PieceTypes;

	// Piece count for the smallest and largest hull; the hull's bounding radius picks a value in between.
	UPROPERTY(EditAnywhere, Category="Pieces")
	FIntPoint PieceCountRange = FIntPoint(8, 16);

	// Hull bounding radius (cm) mapped onto PieceCountRange.
	UPROPERTY(EditAnywhere, Category="Pieces")
	FVector2D HullRadiusRange = FVector2D(250.0, 600.0);

	// Pieces start scattered this far (cm) around the burst origin.
	UPROPERTY(EditAnywhere, Category="Pieces", meta=(ClampMin="0"))
	float SpawnScatterRadius = 90.f;

	UPROPERTY(EditAnywhere, Category="Launch", meta=(ClampMin="0"))
	FVector2D UpSpeedRange = FVector2D(550.0, 1250.0);

	UPROPERTY(EditAnywhere, Category="Launch", meta=(ClampMin="0"))
	FVector2D SideSpeedRange = FVector2D(250.0, 900.0);

	UPROPERTY(EditAnywhere, Category="Launch", meta=(ClampMin="0"))
	FVector2D SpinDegreesRange = FVector2D(180.0, 720.0);

	// 0 throws pieces all around; 1 throws every piece within DirectionSpreadDegrees of the burst's direction.
	UPROPERTY(EditAnywhere, Category="Launch", meta=(ClampMin="0", ClampMax="1"))
	float DirectionBias = 0.f;

	// Half angle of the cone around the burst direction that biased pieces fly into.
	UPROPERTY(EditAnywhere, Category="Launch", meta=(ClampMin="0", ClampMax="180"))
	float DirectionSpreadDegrees = 60.f;

	UPROPERTY(EditAnywhere, Category="Fire", meta=(ClampMin="0"))
	int32 BurningPieceCount = 2;

	UPROPERTY(EditAnywhere, Category="Fire")
	ERTSFireType BurningPieceFireType = ERTSFireType::SmallOptim;

	UPROPERTY(EditAnywhere, Category="Fire", meta=(ClampMin="0"))
	float BurningPieceFireLifeTime = 6.f;

	UPROPERTY(EditAnywhere, Category="Fire")
	FVector BurningPieceFireScale = FVector(0.35);
};

/**
 * @brief Project-wide debris thrown out of a wreck when a tank dies during an ammo cook-off or an engine fire
 * (Project Settings > Game > Wreck Debris). Read by UWreckDebrisBurstComponent; per tank only the count scale and
 * on/off live in FWreckTurretTossSettings and FWreckEngineFireDeathSettings.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Wreck Debris"))
class RTS_SURVIVAL_API UWreckDebrisSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWreckDebrisSettings();

	// Metal chunks, shells, ammo boxes and cans blown out of the turret ring when the ammunition detonates.
	UPROPERTY(Config, EditAnywhere, Category="Bursts")
	FWreckDebrisBurstProfile AmmoCookOffBurst;

	// Deck plates, grilles, hatch lids and exhaust parts blown off the engine deck when the fuel explodes.
	UPROPERTY(Config, EditAnywhere, Category="Bursts")
	FWreckDebrisBurstProfile EngineFireBurst;

	// Below these speeds for SettleDuration seconds a piece freezes; MaxFlightTime freezes it regardless.
	UPROPERTY(Config, EditAnywhere, Category="Landing", meta=(ClampMin="0"))
	float SettleLinearSpeed = 20.f;

	UPROPERTY(Config, EditAnywhere, Category="Landing", meta=(ClampMin="0"))
	float SettleAngularSpeed = 30.f;

	UPROPERTY(Config, EditAnywhere, Category="Landing", meta=(ClampMin="0"))
	float SettleDuration = 0.3f;

	UPROPERTY(Config, EditAnywhere, Category="Landing", meta=(ClampMin="0.5"))
	float MaxFlightTime = 7.f;
};
