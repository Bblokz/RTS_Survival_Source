// Copyright (C) Bas Blokzijl

#include "SquadUnitHealthComponent.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/USquadHealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void USquadUnitHealthComponent::SetSquadHealthComponent(USquadHealthComponent* InSquadHealth)
{
	M_SquadHealthComp = InSquadHealth;
	// Do not push immediately here; the Squad Controller will sum/seed totals after linking all members.
}

bool USquadUnitHealthComponent::TakeDamage(float& InOutDamage, const ERTSDamageType DamageType)
{
	// Let the base component apply resistances/reductions and mutate InOutDamage to the *actual dealt* amount.
	const bool bDied = Super::TakeDamage(InOutDamage, DamageType);

	// Forward the dealt damage to the squad aggregate, then refresh our snapshot without double applying.
	if (GetIsValidSquadHealth_NoReport() && InOutDamage > 0.f)
	{
		M_SquadHealthComp->SquadTakeDamage(InOutDamage);
		M_SquadHealthComp->OnUnitStateChanged(this, GetMaxHealth(), GetCurrentHealth(), /*bAffectSquadCurrent*/ false);
	}
	return bDied;
}

void USquadUnitHealthComponent::SetCurrentHealth(const float NewCurrentHealth)
{
	Super::SetCurrentHealth(NewCurrentHealth);
	// Current changed out-of-band → apply to squad by current delta.
	NotifySquadOfState(/*bAffectSquadCurrent*/ true);
}

bool USquadUnitHealthComponent::Heal(const float HealAmount)
{
	const float Before = GetCurrentHealth();
	const bool bFull = Super::Heal(HealAmount);
	const float After  = GetCurrentHealth();
	const float Delta  = FMath::Max(0.f, After - Before);

	if (GetIsValidSquadHealth_NoReport() && Delta > 0.f)
	{
		M_SquadHealthComp->SquadHeal(Delta);
		// Refresh snapshot; delta already applied above.
		M_SquadHealthComp->OnUnitStateChanged(this, GetMaxHealth(), GetCurrentHealth(), /*bAffectSquadCurrent*/ false);
	}
	return bFull;
}

void USquadUnitHealthComponent::SetMaxHealth(const float NewMaxHealth)
{
	Super::SetMaxHealth(NewMaxHealth);
	// Max (and possibly Current) changed → aggregate computes both deltas and applies them.
	NotifySquadOfState(/*bAffectSquadCurrent*/ true);
}

void USquadUnitHealthComponent::OnOverwiteHealthbarVisiblityPlayer(const ERTSPlayerHealthBarVisibilityStrategy Strategy)
{
	(void)Strategy;
	RTSFunctionLibrary::ReportError(TEXT("Squad unit health components do not support changing health bar visibility."));
}

void USquadUnitHealthComponent::OnOverwiteHealthbarVisiblityEnemy(const ERTSEnemyHealthBarVisibilityStrategy Strategy)
{
	(void)Strategy;
	RTSFunctionLibrary::ReportError(TEXT("Squad unit health components do not support changing health bar visibility."));
}

void USquadUnitHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_BindOwnerDeath();
}

void USquadUnitHealthComponent::BeginPlay_ApplyUserSettingsHealthBarVisibility()
{
	return;
}

bool USquadUnitHealthComponent::GetIsValidSquadHealth_NoReport() const
{
	return M_SquadHealthComp.IsValid();
}

void USquadUnitHealthComponent::NotifySquadOfState(const bool bAffectSquadCurrent) const
{
	if (not GetIsValidSquadHealth_NoReport())
	{
		return;
	}

	M_SquadHealthComp->OnUnitStateChanged(
		const_cast<USquadUnitHealthComponent*>(this),
		GetMaxHealth(),
		GetCurrentHealth(),
		bAffectSquadCurrent);
}

void USquadUnitHealthComponent::BeginPlay_BindOwnerDeath()
{
	ACharacterObjectsMaster* OwnerAsHp = Cast<ACharacterObjectsMaster>(GetOwner());
	if (not IsValid(OwnerAsHp))
	{
		return;
	}

	TWeakObjectPtr<USquadUnitHealthComponent> WeakThis(this);
	OwnerAsHp->OnUnitDies.AddLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnOwnerDies();
	});
}

void USquadUnitHealthComponent::OnOwnerDies()
{
	if (not GetIsValidSquadHealth_NoReport())
	{
		return;
	}
	M_SquadHealthComp->UnregisterUnit(this);
}
