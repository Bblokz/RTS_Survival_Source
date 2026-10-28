#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "RTS_Survival/Subsystems/FireSubsystem/ERTSFireType.h"
#include "RTS_Survival/Subsystems/FireSubsystem/FRTSFireEffectParams.h"

#include "WreckTurretTossSettings.generated.h"

class USoundCue;

/**
 * @brief Tuning for the ammo cook-off death where the wreck's turret is blown off the hull.
 * Set per tank on ATankMaster; the values are handed to the UWreckTurretTossComponent on the spawned wreck.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckTurretTossSettings
{
	GENERATED_BODY()

	// When off, this tank keeps its normal wreck when it dies during an ammo cook-off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	bool bTossTurretOnCookOffDeath = true;

	// Name of the wreck's hull static mesh component; the ammo fire burns in its turret ring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	FName HullComponentName = TEXT("Hull");

	// Name of the wreck's turret static mesh component that is thrown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	FName TurretComponentName = TEXT("Turret");

	// Hull socket where the turret sits on the ring; the toss and hull fire start here when the socket exists.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wreck")
	FName TurretRingSocketName = TEXT("T_0");

	// Upward launch speed range in cm/s (mass independent).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch", meta=(ClampMin="0"))
	FVector2D UpSpeedRange = FVector2D(1150.0, 1550.0);

	// Sideways launch speed range in cm/s in a random horizontal direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch", meta=(ClampMin="0"))
	FVector2D SideSpeedRange = FVector2D(250.0, 600.0);

	// Tumble speed range in degrees per second around a mostly horizontal axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch", meta=(ClampMin="0"))
	FVector2D SpinDegreesRange = FVector2D(150.0, 420.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	ERTS_ExplosionType LaunchExplosionType = ERTS_ExplosionType::TankTurret;

	// Plays instead of the launch explosion type's own sound; empty keeps that sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Launch")
	TObjectPtr<USoundCue> LaunchSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hull Fire")
	ERTSFireType HullFireType = ERTSFireType::AmmoCookOffInf;

	// Seconds the ammo fire keeps blasting out of the open turret ring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hull Fire", meta=(ClampMin="0"))
	float HullFireLifeTime = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hull Fire")
	FVector HullFireScale = FVector(1.25);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hull Fire")
	FRTSFireEffectParams HullFireEffectParams;

	// Offset from the turret ring for the hull fire, in hull space.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hull Fire")
	FVector HullFireOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret Fire")
	bool bBurnTurretWhileFlying = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret Fire", meta=(EditCondition="bBurnTurretWhileFlying"))
	ERTSFireType TurretFireType = ERTSFireType::TankFire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret Fire",
		meta=(EditCondition="bBurnTurretWhileFlying", ClampMin="0"))
	float TurretFireLifeTime = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Turret Fire", meta=(EditCondition="bBurnTurretWhileFlying"))
	FVector TurretFireScale = FVector(0.6);

	// Metal chunks, shells, ammo boxes and cans thrown out of the hull (pieces set in Project Settings > Wreck Debris).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	bool bThrowDebris = true;

	// Multiplies the piece count, which already grows with the hull's size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(EditCondition="bThrowDebris", ClampMin="0"))
	float DebrisCountScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing")
	bool bPlayLandingImpact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(EditCondition="bPlayLandingImpact"))
	ERTS_ExplosionType LandingExplosionType = ERTS_ExplosionType::Small;

	// Plays instead of the landing explosion type's own sound; empty keeps that sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(EditCondition="bPlayLandingImpact"))
	TObjectPtr<USoundCue> LandingSound = nullptr;

	// Below this linear speed (cm/s) for the settle duration the turret counts as landed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleLinearSpeed = 25.f;

	// Below this angular speed (deg/s) for the settle duration the turret counts as landed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleAngularSpeed = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0"))
	float SettleDuration = 0.4f;

	// Physics is always switched off after this many seconds, even when the turret still rolls.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Landing", meta=(ClampMin="0.5"))
	float MaxFlightTime = 9.f;
};
