// Copyright (C) Bas Blokzijl


#include "USquadHealthComponent.h"

#include "RTS_Survival/GameUI/Healthbar/W_HealthBar.h"
#include "RTS_Survival/Units/Squads/SquadUnitHpComp/SquadUnitHealthComponent.h"

void USquadHealthComponent::ResetUnitSnapshots()
{
	M_UnitToMaxCur.Empty();
	M_FullSquadMaxHealth = 0.f;
	bM_HasFullMaxInitialized = false;
}

void USquadHealthComponent::SeedUnitSnapshot(USquadUnitHealthComponent* UnitHealth,
                                             const float UnitMax,
                                             const float UnitCurrent)
{
	if (not IsValid(UnitHealth))
	{
		return;
	}

	const TWeakObjectPtr<USquadUnitHealthComponent> Key(UnitHealth);
	M_UnitToMaxCur.Add(Key, FVector2D(UnitMax, UnitCurrent));
}

void USquadHealthComponent::InitializeFullSquadMax(const float FullMaxHealth)
{
	M_FullSquadMaxHealth = FMath::Max(0.f, FullMaxHealth);
	bM_HasFullMaxInitialized = true;

	// Set max without accidentally changing current via percentage logic.
	SetFullSquadMax_Internal(M_FullSquadMaxHealth);
}

void USquadHealthComponent::SetMaxHealth(const float NewMaxHealth)
{
	if (not bM_HasFullMaxInitialized)
	{
		Super::SetMaxHealth(NewMaxHealth);
		return;
	}

	// Once initialized, ignore attempts to change max away from full-squad max.
	if (FMath::IsNearlyEqual(NewMaxHealth, M_FullSquadMaxHealth))
	{
		Super::SetMaxHealth(NewMaxHealth);
		return;
	}
	// Intentionally no-op. (Keeps widget/max stable for full squad.)
}

void USquadHealthComponent::OnWidgetInitialized()
{
        Super::OnWidgetInitialized();

        if (not bM_OnWidgetInitLoadBrushAsset)
        {
                return;
        }

        UW_HealthBar* HealthBarWidget = GetHealthBarWidget();
        if (not IsValid(HealthBarWidget))
        {
                RTSFunctionLibrary::ReportError("Squad waited for health bar widget to initialize, but it is invalid."
                                                                  "cannot set squad weapon icon.");
                return;
        }

        HealthBarWidget->UpdateSquadWeaponIcon(M_SquadWeaponIconAssetToSet);
        bM_OnWidgetInitLoadBrushAsset = false;
        M_SquadWeaponIconAssetToSet = nullptr;
}

void USquadHealthComponent::ApplyDeltaToTotals(const float DeltaMax,
                                               const float DeltaCur,
                                               const bool bApplyCur,
                                               const bool bApplyMax)
{
	if (FMath::IsNearlyZero(DeltaMax) && (FMath::IsNearlyZero(DeltaCur) || not bApplyCur))
	{
		return;
	}

	if (bApplyMax)
	{
		const float NewFullMax = bM_HasFullMaxInitialized
			? (M_FullSquadMaxHealth + DeltaMax)
			: (GetMaxHealth() + DeltaMax);

		SetFullSquadMax_Internal(NewFullMax);
	}

	if (bApplyCur)
	{
		ApplyDeltaToCurrentOnly(DeltaCur);
	}
}


void USquadHealthComponent::ApplyDeltaToCurrentOnly(const float DeltaCur)
{
	if (FMath::IsNearlyZero(DeltaCur))
	{
		return;
	}

	const float NewCur = FMath::Clamp(GetCurrentHealth() + DeltaCur, 0.f, GetMaxHealth());
	Super::SetCurrentHealth(NewCur);
}

void USquadHealthComponent::SetFullSquadMax_Internal(const float NewFullMax)
{
	const float ClampedNewFullMax = FMath::Max(0.f, NewFullMax);

	const float SavedCurrent = GetCurrentHealth();
	M_FullSquadMaxHealth = ClampedNewFullMax;

	// Update widget/max via base, then restore current unchanged.
	UHealthComponent::SetMaxHealth(M_FullSquadMaxHealth);
	UHealthComponent::SetCurrentHealth(SavedCurrent);
}

void USquadHealthComponent::SquadTakeDamage(const float Damage)
{
	if (Damage <= 0.f)
	{
		return;
	}

	const float NewCur = FMath::Max(0.f, GetCurrentHealth() - Damage);
	SetCurrentHealth(NewCur);
}

void USquadHealthComponent::SquadHeal(const float HealAmount)
{
	if (HealAmount <= 0.f)
	{
		return;
	}

	// Use base Heal to benefit from clamping + UI callbacks.
	Heal(HealAmount);
}

void USquadHealthComponent::UpdateSquadWeaponIcon(USlateBrushAsset* NewWeaponIconAsset)
{
	UW_HealthBar* HealthBarWidget = GetHealthBarWidget();
	if(IsValid(HealthBarWidget))
	{
		HealthBarWidget->UpdateSquadWeaponIcon(NewWeaponIconAsset);	
	}
	else
	{
		bM_OnWidgetInitLoadBrushAsset = true;
		M_SquadWeaponIconAssetToSet = NewWeaponIconAsset;
	}
}

void USquadHealthComponent::OnUnitStateChanged(USquadUnitHealthComponent* UnitHealth,
                                               const float NewUnitMax,
                                               const float NewUnitCurrent,
                                               const bool bAffectSquadCurrent,
                                               const bool bAffectSquadMax)
{
	if (not IsValid(UnitHealth))
	{
		return;
	}

	const TWeakObjectPtr<USquadUnitHealthComponent> Key(UnitHealth);
	const FVector2D* Prev = M_UnitToMaxCur.Find(Key);

	const float PrevMax = Prev ? Prev->X : 0.f;
	const float PrevCur = Prev ? Prev->Y : 0.f;

	M_UnitToMaxCur.Add(Key, FVector2D(NewUnitMax, NewUnitCurrent));

	const float DeltaMax = NewUnitMax - PrevMax;
	const float DeltaCur = NewUnitCurrent - PrevCur;

	ApplyDeltaToTotals(DeltaMax, DeltaCur, bAffectSquadCurrent, bAffectSquadMax);
}

void USquadHealthComponent::UnregisterUnit(USquadUnitHealthComponent* UnitHealth)
{
	if (not IsValid(UnitHealth))
	{
		return;
	}

	const TWeakObjectPtr<USquadUnitHealthComponent> Key(UnitHealth);
	const FVector2D* Prev = M_UnitToMaxCur.Find(Key);
	if (not Prev)
	{
		return;
	}

	// Only subtract remaining current HP.
	ApplyDeltaToCurrentOnly(-Prev->Y);

	M_UnitToMaxCur.Remove(Key);
}

