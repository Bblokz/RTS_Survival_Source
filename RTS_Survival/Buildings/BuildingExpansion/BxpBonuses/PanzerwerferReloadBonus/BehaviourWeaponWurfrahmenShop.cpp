// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "BehaviourWeaponWurfrahmenShop.h"

#include "RTS_Survival/DeveloperSettings.h"

namespace BehaviourWeaponWurfrahmenShopConstants
{
	// The Panzerwerfer rocket launchers (the 15 cm and the small one on the Pz 38(t) R); guns and MGs keep their reload.
	const FName PanzerwerferWeaponName = TEXT("Panzerwerfer");
	const FName SmallPanzerwerferWeaponName = TEXT("Panzerwerfer_Small");
}

UBehaviourWeaponWurfrahmenShop::UBehaviourWeaponWurfrahmenShop()
{
	using DeveloperSettings::GameBalance::UnitCosts::Bxps::GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
	BehaviourIcon = EBehaviourIcon::RocketBarrage;
	M_BuffType = EBuffDebuffType::Buff;
	M_TitleText = "Wurfrahmen Shop";
	const int32 FasterReloadPercent = FMath::RoundToInt(
		(1.f - GerMechDepotWurfrahmenShopPanzerwerferReloadMlt) * 100.f);
	M_DisplayText = FString::Printf(
		TEXT("A Wurfrahmen shop stands at a depot: rockets reload %d%% faster."), FasterReloadPercent);
	// ReloadSpeed is a duration: the multiplier scales the reload time.
	BehaviourWeaponMultipliers.ReloadSpeedMlt = GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
	WeaponNamesToApplyTo = {
		BehaviourWeaponWurfrahmenShopConstants::PanzerwerferWeaponName,
		BehaviourWeaponWurfrahmenShopConstants::SmallPanzerwerferWeaponName
	};
}
