#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckRegularDeathSettings.h"

class AActor;
class UStaticMeshComponent;

/** Everything a regular-death wreck needs from its tank, read before the tank is destroyed. */
struct FWreckRegularDeathRequest
{
	FWreckRegularDeathSettings Settings;
	ERegularDeathVariant Variant = ERegularDeathVariant::Standard;
	// Travel direction of the projectile that killed the tank; zero for any other death.
	FVector KillShotDirection = FVector::ZeroVector;
	bool bIsSeen = true;
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
	/** @return False when no burst started (unseen, capped or no system); the caller then plays the chain reaction. */
	static bool StartDebris(const AActor& Wreck, UStaticMeshComponent& Hull, const FWreckRegularDeathRequest& Request);
	static FVector GetDebrisThrowDirection(const FWreckNiagaraDebrisSettings& Debris, const FVector& KillShotDirection);
	static int32 GetDebrisPieceCount(const FWreckNiagaraDebrisSettings& Debris, const UStaticMeshComponent& Hull);
	static FVector GetRandomHullTopPoint(const UStaticMeshComponent& Hull);
	static TArray<FVector> GetChainReactionAnchors(const UStaticMeshComponent& Hull, const UStaticMeshComponent* Turret);
};
