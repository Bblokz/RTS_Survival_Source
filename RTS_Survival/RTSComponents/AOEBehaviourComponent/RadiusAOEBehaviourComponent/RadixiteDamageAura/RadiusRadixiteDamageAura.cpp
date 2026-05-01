// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadiusRadixiteDamageAura.h"

#include "RTS_Survival/Behaviours/Derived/Damage/RadixiteDamageBehaviour.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

URadiusRadixiteDamageAura::URadiusRadixiteDamageAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.HostBehaviourIcon = EBehaviourIcon::RadiationRadius;
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_RadiationAura;
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyEveryTick;
	AOEBehaviourSettings.bSearchForEnemies = true;
}

void URadiusRadixiteDamageAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.TitleText = RadiusSettings.HostBehaviourTitleText;
	const float DamagePerSecond = GetDamagePerSecondFromBehaviours();
	SetDescriptionWithDamageAmount(UIData, DamagePerSecond);
	Behaviour.SetCustomUIData(UIData);
}

float URadiusRadixiteDamageAura::GetDamagePerSecondFromBehaviours() const
{
	float TotalDamagePerSecond = 0.f;
	for (const TSubclassOf<UBehaviour> BehaviourType : GetAoeBehaviourSettings().BehaviourTypes)
	{
		if (not IsValid(BehaviourType))
		{
			continue;
		}

		if (not BehaviourType->IsChildOf(URadixiteDamageBehaviour::StaticClass()))
		{
			continue;
		}

		const URadixiteDamageBehaviour* RadixiteDamageBehaviour = Cast<URadixiteDamageBehaviour>(
			BehaviourType->GetDefaultObject());
		if (not IsValid(RadixiteDamageBehaviour))
		{
			continue;
		}

		TotalDamagePerSecond += RadixiteDamageBehaviour->GetDamagePerSecond();
	}

	return TotalDamagePerSecond;
}

void URadiusRadixiteDamageAura::SetDescriptionWithDamageAmount(
	FBehaviourUIData& OutUIData,
	const float DamagePerSecond) const
{
	const FString DamageDescription = FRTSRichTextConverter::MakeRTSRich(
		FString::SanitizeFloat(DamagePerSecond) + " radiation DPS",
		ERTSRichText::Text_Bad14);
	const FString RadiusDescription = FRTSRichTextConverter::MakeRTSRich(
		FString::SanitizeFloat(GetAoeBehaviourSettings().Radius / 100.f) + " m radius",
		ERTSRichText::Text_Exp);

	OutUIData.DescriptionText = "Applies " + DamageDescription + " in " + RadiusDescription;
}
