#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckRegularDeathSettings.h"

class AActor;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMeshComponent;
class UWreckDeathVariantSubsystem;

/** Everything a regular-death wreck needs from its tank, read before the tank is destroyed. */
struct FWreckRegularDeathRequest
{
	FWreckRegularDeathSettings Settings;
	ERegularDeathVariant Variant = ERegularDeathVariant::Standard;
	// Travel direction of the projectile that killed the tank; zero for any other death.
	FVector KillShotDirection = FVector::ZeroVector;
	bool bIsSeen = true;
	// Picks the wheel set (German or Soviet) and how many wheels WheelThrow throws.
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;
	// Only vehicles driven by tracks have road wheels to throw; wheeled cars and trucks never play WheelThrow.
	bool bHasTrackWheels = false;
};

/** One Niagara mesh burst (debris chunks or wheels) as the shared spawn path needs it. */
struct FWreckMeshBurstSpawn
{
	FVector Origin = FVector::ZeroVector;
	int32 PieceCount = 1;
	FVector ThrowDirection = FVector::UpVector;
	float ConeDegrees = 0.f;
	float SpreadRadius = 0.f;
	// Null keeps the system's own material.
	UMaterialInterface* Material = nullptr;
	int32 BurstSlots = 1;
	float LieSeconds = 0.f;
	float SinkSeconds = 1.f;
};

/**
 * @brief Adds a regular death's variation to a freshly spawned wreck: pose and char always, then the drawn variant's
 * extra layer. The wreck Blueprint's own explosion sequence keeps playing underneath. Fire-and-forget: the explosion
 * manager and Niagara own everything that happens later, so the wreck gets no component.
 */
class RTS_SURVIVAL_API FWreckRegularDeath
{
public:
	/**
	 * @brief Call from the wreck spawn callback for every spawned actor; actors without the hull mesh are skipped.
	 * @param Wreck The spawned actor.
	 * @param Request Settings, variant and kill shot taken from the tank.
	 */
	static void StartOnWreck(AActor* Wreck, const FWreckRegularDeathRequest& Request);

private:
	static UStaticMeshComponent* FindNamedMesh(const AActor& Wreck, FName ComponentName);
	static void VaryPose(AActor& Wreck, UStaticMeshComponent* Turret, const FWreckRegularDeathPoseSettings& Pose);
	static void ApplyChar(AActor& Wreck, const UStaticMeshComponent& Hull, const FWreckRegularDeathPoseSettings& Pose);
	static void StartChainReaction(const AActor& Wreck, const UStaticMeshComponent& Hull,
	                               const UStaticMeshComponent* Turret, const FWreckChainReactionSettings& Chain);
	/**
	 * @return The variant that played: the drawn debris variant, Debris when a heavy burst did not fit the budget,
	 * else ChainReaction.
	 */
	static ERegularDeathVariant StartDebrisWithFallback(const AActor& Wreck, UStaticMeshComponent& Hull,
	                                                    const FWreckRegularDeathRequest& Request);
	/** @return False when no burst started (unseen, over the burst budget or no system). */
	static bool StartDebris(const AActor& Wreck, UStaticMeshComponent& Hull, const FWreckRegularDeathRequest& Request,
	                        bool bIsHeavy);
	/** @return False when no burst started (unseen, no track wheels, over the burst budget or no system). */
	static bool StartWheelThrow(const AActor& Wreck, const UStaticMeshComponent& Hull,
	                            const FWreckRegularDeathRequest& Request);
	static int32 GetWheelCount(const FWreckWheelThrowSettings& WheelThrow, ETankSubtype TankSubtype);
	/** @return +1 to throw off the hull's right side, -1 for the left: the side the killing shot exits, else random. */
	static float GetWheelThrowSide(const UStaticMeshComponent& Hull, const FVector& KillShotDirection);
	/** @return The variant subsystem when another burst taking BurstSlots may start; null otherwise. */
	static UWreckDeathVariantSubsystem* GetSubsystemForBurst(const AActor& Wreck, bool bIsSeen, bool bOnlyWhenSeen,
	                                                         int32 BurstSlots);
	/**
	 * @brief Spawns, parameterises and registers one Niagara mesh burst so Debris and WheelThrow share the user
	 * parameters, the burst budget and the sinking.
	 * @param Wreck World context.
	 * @param System NS_WreckDebrisBurst or one of its wheel copies.
	 * @param Burst Where and how to throw.
	 * @param VariantSubsystem Tracks the burst until its pieces sank.
	 * @return False when the component could not spawn.
	 */
	static bool SpawnMeshBurst(const AActor& Wreck, UNiagaraSystem& System, const FWreckMeshBurstSpawn& Burst,
	                           UWreckDeathVariantSubsystem& VariantSubsystem);
	static FVector GetDebrisThrowDirection(const FWreckNiagaraDebrisSettings& Debris, const FVector& KillShotDirection);
	static int32 GetDebrisPieceCount(const FWreckNiagaraDebrisSettings& Debris, const UStaticMeshComponent& Hull,
	                                 bool bIsHeavy);
	static FVector GetRandomHullTopPoint(const UStaticMeshComponent& Hull);
	static TArray<FVector> GetChainReactionAnchors(const UStaticMeshComponent& Hull, const UStaticMeshComponent* Turret);
};
