// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSBlastThrowSubsystem.generated.h"

struct FDamageEvent;

/** @brief How hard one big blast throws what it kills; looked up by the victims through its epicentre. */
USTRUCT()
struct FRTSBlastThrow
{
	GENERATED_BODY()

	FVector Epicenter = FVector::ZeroVector;

	// Multiplies the speed a blast-killed soldier's body is thrown with.
	float InfantryThrowScale = 1.f;

	// A vehicle killed by this blast tosses its turret, away from the epicentre.
	bool bTossTurrets = false;

	// Multiplies the turret's launch speeds.
	float TurretTossSpeedScale = 1.f;

	float ExpireAtSeconds = 0.f;
};

/**
 * @brief Lets one blast throw harder than ordinary splash without a new parameter on every AOE path: the blast
 * registers itself just before it deals its damage, and victims that die to an area damage event from the same
 * epicentre ask for it (ASquadUnit's death ragdoll, ATankMaster's turret toss). Entries expire on their own.
 */
UCLASS()
class RTS_SURVIVAL_API URTSBlastThrowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief Registers the throw of a blast that is about to deal its damage.
	 * @param Throw Epicentre and strengths; ExpireAtSeconds is set here.
	 * @param LifetimeSeconds How long victims of async damage from this epicentre still find it.
	 */
	void RegisterBlastThrow(const FRTSBlastThrow& Throw, const float LifetimeSeconds);

	/**
	 * @brief The registered throw behind an area damage event (blast or vehicle module explosion), if any.
	 * @param WorldContext Any object in the victim's world.
	 * @param DamageEvent The event the victim took.
	 * @param OutEpicenter Where the blast went off, when a throw is found.
	 * @return The throw, or null for ordinary damage; valid until the next registration.
	 */
	static const FRTSBlastThrow* FindForDamageEvent(const UObject* WorldContext, const FDamageEvent& DamageEvent,
	                                                FVector& OutEpicenter);

private:
	TArray<FRTSBlastThrow> M_Throws;

	const FRTSBlastThrow* FindNear(const FVector& Epicenter) const;
	void RemoveExpired(const float NowSeconds);
};
