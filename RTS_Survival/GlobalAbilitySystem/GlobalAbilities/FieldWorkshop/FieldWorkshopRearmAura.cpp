// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldWorkshopRearmAura.h"

#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

UFieldWorkshopRearmAura::UFieldWorkshopRearmAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_AmmoResupplyAura;
	// The workshop draws its own radius while it works; hover and selection would stack a second disc on it.
	RadiusSettings.ShowRadiusType = EAOeBehaviourShowRadiusType::None;
	// The reload buff is permanent while in range: applied once on entering, removed on leaving.
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyOnlyOnEnter;
	AOEBehaviourSettings.IntervalSeconds = 1.f;
}

bool UFieldWorkshopRearmAura::IsValidTarget(AActor* ValidActor) const
{
	return IsValid(Cast<ATankMaster>(ValidActor)) && RTSFunctionLibrary::RTSIsValid(ValidActor);
}

void UFieldWorkshopRearmAura::OnTickComponentsInRange(const TArray<UBehaviourComp*>& BehaviourComponentsInRange)
{
	Super::OnTickComponentsInRange(BehaviourComponentsInRange);
	M_SecondsSinceTopUp += GetAoeBehaviourSettings().IntervalSeconds;
	if (M_SecondsSinceTopUp < TopUpIntervalSeconds)
	{
		return;
	}
	M_SecondsSinceTopUp = 0.f;
	TopUpVehiclesInRange(BehaviourComponentsInRange);
}

void UFieldWorkshopRearmAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	constexpr float CentimetresPerMetre = 100.f;
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.TitleText = RadiusSettings.HostBehaviourTitleText;
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.DescriptionText = FString::Printf(
		TEXT("Your vehicles within <Text_Exp>%d m</> reload faster and get every magazine topped up every "
			"<Text_Exp>%d s</>."),
		FMath::RoundToInt(GetAOERadius() / CentimetresPerMetre), FMath::RoundToInt(TopUpIntervalSeconds));
	Behaviour.SetCustomUIData(UIData);
}

void UFieldWorkshopRearmAura::TopUpVehiclesInRange(const TArray<UBehaviourComp*>& BehaviourComponentsInRange)
{
	for (const UBehaviourComp* BehaviourComponent : BehaviourComponentsInRange)
	{
		if (not IsValid(BehaviourComponent))
		{
			continue;
		}
		const ATankMaster* Vehicle = Cast<ATankMaster>(BehaviourComponent->GetOwner());
		if (not IsValid(Vehicle))
		{
			continue;
		}
		const int32 ToppedUp = TopUpVehicle(*Vehicle);
		M_ToppedUpWeaponCount += ToppedUp;
		if (ToppedUp > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("RTS_FIELD_WORKSHOP rearmed=%s weapons=%d"), *Vehicle->GetName(), ToppedUp);
		}
	}
}

int32 UFieldWorkshopRearmAura::TopUpVehicle(const ATankMaster& Vehicle) const
{
	int32 ToppedUp = 0;
	for (UWeaponState* Weapon : FRTSWeaponHelpers::GetWeaponsMountedOnTank(&Vehicle))
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		if (Weapon->IsWeaponFullyLoaded() && not Weapon->GetIsReloading())
		{
			continue;
		}
		Weapon->ForceInstantReload();
		++ToppedUp;
	}
	return ToppedUp;
}
