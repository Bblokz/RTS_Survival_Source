// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteDamageBehaviour.h"

#include "GameFramework/Actor.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

URadixiteDamageBehaviour::URadixiteDamageBehaviour()
{
	bM_UsesTick = true;
	BehaviourLifeTime = EBehaviourLifeTime::Timed;
	BehaviourStackRule = EBehaviourStackRule::Refresh;
	BehaviourIcon = EBehaviourIcon::RadiationDamage;
	M_BuffType = EBuffDebuffType::Debuff;
	M_LifeTimeDuration = DefaultDurationSeconds;
}

void URadixiteDamageBehaviour::SetDamagePerSecond(const float NewDamagePerSecond)
{
	DamagePerSecond = FMath::Max(0.f, NewDamagePerSecond);
}

float URadixiteDamageBehaviour::GetDamagePerSecond() const
{
	return DamagePerSecond;
}

void URadixiteDamageBehaviour::ApplyRadixiteDamageToActor(AActor* HitActor, const float DurationSeconds,
                                                          const float DamagePerSecond)
{
	if (not IsValid(HitActor) || DurationSeconds <= 0.f)
	{
		return;
	}
	UBehaviourComp* BehaviourComponent = HitActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		return;
	}

	// Configured before the Refresh stack rule runs: OnRefreshed copies the DPS of this new instance.
	BehaviourComponent->AddBehaviourWithDurationConfigured(
		URadixiteDamageBehaviour::StaticClass(),
		DurationSeconds,
		[DamagePerSecond](UBehaviour& NewBehaviour)
		{
			if (URadixiteDamageBehaviour* RadixiteBehaviour = Cast<URadixiteDamageBehaviour>(&NewBehaviour))
			{
				RadixiteBehaviour->SetDamagePerSecond(DamagePerSecond);
			}
		});
}

void URadixiteDamageBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);

	if (not BehaviourOwner)
	{
		return;
	}

	M_DamageReceiver = BehaviourOwner;
	M_DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(DamageType);
}

void URadixiteDamageBehaviour::OnTick(const float DeltaTime)
{
	Super::OnTick(DeltaTime);

	if (not M_DamageReceiver.IsValid())
	{
		return;
	}

	const float DamageAmount = DamagePerSecond * DeltaTime;
	if (DamageAmount <= 0.f)
	{
		return;
	}

	ApplyDamage(DamageAmount);
}

void URadixiteDamageBehaviour::OnRefreshed(UBehaviour* RefreshingBehaviour)
{
	const URadixiteDamageBehaviour* RefreshingRadixite = Cast<URadixiteDamageBehaviour>(RefreshingBehaviour);
	if (not RefreshingRadixite)
	{
		return;
	}

	DamagePerSecond = RefreshingRadixite->DamagePerSecond;
	DamageType = RefreshingRadixite->DamageType;
	M_LifeTimeDuration = RefreshingRadixite->M_LifeTimeDuration;
	M_DamageEvent = FRTSWeaponHelpers::MakeBasicDamageEvent(DamageType);
}

void URadixiteDamageBehaviour::ApplyDamage(const float DamageAmount) const
{
	if (not M_DamageReceiver.IsValid())
	{
		return;
	}

	M_DamageReceiver->TakeDamage(DamageAmount, M_DamageEvent, nullptr, nullptr);
}
