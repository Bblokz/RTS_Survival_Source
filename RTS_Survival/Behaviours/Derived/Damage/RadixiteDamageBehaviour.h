// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

#include "RadixiteDamageBehaviour.generated.h"

/**
 * @brief Applies a timed Radixite damage-over-time effect to actors hit by Radixite rounds.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API URadixiteDamageBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	URadixiteDamageBehaviour();

	void SetDamagePerSecond(const float NewDamagePerSecond);
	float GetDamagePerSecond() const;

	/**
	 * @brief One entry for every weapon type with a per-weapon radixite damage over time (projectiles and lasers),
	 * so a new hit restarts the effect with this weapon's damage per second instead of the class default.
	 * @param HitActor Actor that was hit; ignored when it has no behaviour component.
	 * @param DurationSeconds Lifetime of the effect.
	 * @param DamagePerSecond Radiation damage per second while the effect lasts.
	 */
	static void ApplyRadixiteDamageToActor(AActor* HitActor, const float DurationSeconds, const float DamagePerSecond);

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnTick(const float DeltaTime) override;
	virtual void OnRefreshed(UBehaviour* RefreshingBehaviour) override;

private:
	void ApplyDamage(const float DamageAmount) const;

	static constexpr float DefaultDamagePerSecond = 5.f;
	static constexpr float DefaultDurationSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radixite Damage", meta = (AllowPrivateAccess = "true"))
	float DamagePerSecond = DefaultDamagePerSecond;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Radixite Damage", meta = (AllowPrivateAccess = "true"))
	ERTSDamageType DamageType = ERTSDamageType::Radiation;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_DamageReceiver;

	UPROPERTY()
	float M_ReceiverDamageMultiplier = 1.f;

	FDamageEvent M_DamageEvent;
};
