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

	FDamageEvent M_DamageEvent;
};
