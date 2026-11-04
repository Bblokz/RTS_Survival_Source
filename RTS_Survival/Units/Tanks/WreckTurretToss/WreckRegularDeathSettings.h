#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"

#include "WreckRegularDeathSettings.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/** Which extra layer a regular death (no ammo cook-off, no engine fire) adds on top of the wreck's own explosion. */
UENUM(BlueprintType)
enum class ERegularDeathVariant : uint8
{
	// Only the wreck Blueprint's own explosion sequence.
	Standard,
	// Extra small secondary explosions over a few seconds, as ammunition and fuel go up piece by piece.
	ChainReaction,
	// A Niagara burst of hull-coloured chunks, thrown away from the killing projectile when there was one.
	Debris,
	// The Debris burst with more pieces; takes more of the concurrent burst budget.
	HeavyDebris,
	// A few road wheels thrown off one side of the hull; tracked German and Soviet vehicles only, others play Debris.
	WheelThrow
};

/** Pose and char variation every regular-death wreck gets, so two wrecks of one tank never sit identically. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckRegularDeathPoseSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose")
	bool bVaryPose = true;

	// The whole wreck turns up to this many degrees either way.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose", meta=(EditCondition="bVaryPose", ClampMin="0"))
	float HullYawJitterDegrees = 6.f;

	// Min and max degrees the turret is knocked round on its ring, in a random direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose", meta=(EditCondition="bVaryPose"))
	FVector2D TurretYawDegrees = FVector2D(4.0, 22.0);

	// The turret tips up to this many degrees off its ring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose", meta=(EditCondition="bVaryPose", ClampMin="0"))
	float TurretTiltDegrees = 3.f;

	// The turret slides up to this far (cm) off the centre of its ring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pose", meta=(EditCondition="bVaryPose", ClampMin="0"))
	float TurretShiftCm = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char")
	bool bVaryChar = true;

	// Chance the wreck gets no char overlay at all; the rest is scorched by a random amount.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char",
		meta=(EditCondition="bVaryChar", ClampMin="0", ClampMax="1"))
	float NoCharChance = 0.35f;

	// Char radius as a share of the hull's bounding radius, picked between min and max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char", meta=(EditCondition="bVaryChar"))
	FVector2D CharRadiusHullScale = FVector2D(0.3, 0.85);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char", meta=(EditCondition="bVaryChar", ClampMin="0"))
	float EmberCoolDownSeconds = 25.f;

	// Shared with the engine fire death (M_WreckChar_Overlay).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Char", meta=(EditCondition="bVaryChar"))
	TSoftObjectPtr<UMaterialInterface> CharOverlayMaterial;
};

/** Extra small explosions for the ChainReaction variant; they use the explosion pools, so their sounds stay the same. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckChainReactionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction")
	ERTS_ExplosionType SecondaryExplosionType = ERTS_ExplosionType::Small;

	// Min and max extra secondary explosions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction")
	FIntPoint SecondaryCount = FIntPoint(3, 6);

	// Seconds after the wreck spawned before the first extra explosion; the wreck's own blast plays first.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction", meta=(ClampMin="0"))
	float FirstDelaySeconds = 0.7f;

	// Min and max seconds between two extra explosions; uneven gaps read as cooking ammunition.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction")
	FVector2D GapSeconds = FVector2D(0.2, 1.1);

	// The chain never runs longer than this (seconds after the wreck spawned).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction", meta=(ClampMin="0.1"))
	float MaxDurationSeconds = 5.f;

	// Explosions land on the hull's Exp sockets and the turret, or on a random point of the hull top this often.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction", meta=(ClampMin="0", ClampMax="1"))
	float RandomHullTopChance = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction", meta=(ClampMin="0", ClampMax="1"))
	float LateExplosionChance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction")
	ERTS_ExplosionType LateExplosionType = ERTS_ExplosionType::Medium;

	// Min and max seconds after the wreck spawned for the late explosion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chain Reaction")
	FVector2D LateExplosionDelaySeconds = FVector2D(4.0, 5.5);
};

/** The Debris variant's Niagara burst; no physics bodies, the pieces bounce, lie a while and sink away. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckNiagaraDebrisSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	TSoftObjectPtr<UNiagaraSystem> DebrisSystem;

	// Chunk material with a HullBaseColor texture parameter; each wreck's hull colour texture is put into a copy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	TSoftObjectPtr<UMaterialInterface> ChunkMaterial;

	// Min and max pieces, from the smallest to the largest hull.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	FIntPoint PieceCount = FIntPoint(6, 14);

	// Hull bounding radii (cm) that map to the min and max piece count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	FVector2D HullRadiusRange = FVector2D(250.0, 600.0);

	// The HeavyDebris variant throws this many times the Debris piece count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="1"))
	float HeavyPieceCountMultiplier = 2.f;

	// Concurrent burst slots a HeavyDebris burst takes (a Debris burst takes one); a full budget falls back to Debris.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="1"))
	int32 HeavyBurstSlots = 2;

	// How far the throw leans away from the killing projectile; 0 throws straight up, 1 flat along the shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="0", ClampMax="1"))
	float KillDirectionLean = 0.6f;

	// Half-angle (degrees) of the throw cone without a known shot direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="0", ClampMax="90"))
	float UpConeDegrees = 70.f;

	// Half-angle (degrees) of the throw cone around the shot direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="0", ClampMax="90"))
	float ShotConeDegrees = 35.f;

	// Seconds the pieces lie on the ground before they start to sink away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="0"))
	float LieSeconds = 12.f;

	// Seconds the pieces take to sink away; the burst then ends and frees its slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris", meta=(ClampMin="0.1"))
	float SinkSeconds = 2.f;

	// Unseen deaths skip the burst: nobody would see the pieces.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Debris")
	bool bOnlyWhenSeen = true;
};

/** Road wheels the WheelThrow variant throws off one side of the hull; they bounce, lie a while and sink away. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckWheelThrowSettings
{
	GENERATED_BODY()

	// Panzer IV road wheels and sprocket; thrown by every German tracked vehicle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	TSoftObjectPtr<UNiagaraSystem> GermanWheelSystem;

	// T-34 road wheels and sprocket; thrown by every Soviet tracked vehicle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	TSoftObjectPtr<UNiagaraSystem> SovietWheelSystem;

	// Min and max wheels for light tanks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	FIntPoint LightTankWheels = FIntPoint(1, 2);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	FIntPoint MediumTankWheels = FIntPoint(2, 3);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	FIntPoint HeavyTankWheels = FIntPoint(3, 4);

	// Half-tracks and other tracked vehicles that are no light, medium or heavy tank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw")
	FIntPoint OtherTrackedWheels = FIntPoint(1, 2);

	// How far the throw leans out of the hull's side; 0 throws straight up, 1 flat sideways.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw", meta=(ClampMin="0", ClampMax="1"))
	float SideLean = 0.55f;

	// Half-angle (degrees) of the throw cone around the side direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw", meta=(ClampMin="0", ClampMax="90"))
	float ConeDegrees = 25.f;

	// Seconds the wheels lie on the ground before they start to sink away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw", meta=(ClampMin="0"))
	float LieSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wheel Throw", meta=(ClampMin="0.1"))
	float SinkSeconds = 2.5f;
};

/**
 * @brief Per-tank tuning for regular deaths. Every regular death draws one of five variants (Standard, ChainReaction,
 * Debris, HeavyDebris, WheelThrow) from a per-match shuffle and always varies the wreck's pose and char; the wreck
 * Blueprint's own explosion sequence plays underneath in all of them.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWreckRegularDeathSettings
{
	GENERATED_BODY()

	FWreckRegularDeathSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death")
	bool bVaryRegularDeaths = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death")
	FName HullComponentName = TEXT("Hull");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death")
	FName TurretComponentName = TEXT("Turret");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death", meta=(EditCondition="bVaryRegularDeaths"))
	FWreckRegularDeathPoseSettings Pose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death", meta=(EditCondition="bVaryRegularDeaths"))
	FWreckChainReactionSettings ChainReaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death", meta=(EditCondition="bVaryRegularDeaths"))
	FWreckNiagaraDebrisSettings Debris;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regular Death", meta=(EditCondition="bVaryRegularDeaths"))
	FWreckWheelThrowSettings WheelThrow;
};
