// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadiusImprovedRangeAura.h"

#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

URadiusImprovedRangeAura::URadiusImprovedRangeAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.HostBehaviourIcon = EBehaviourIcon::RangeBoostRadius;
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_ImprovedRangeArea;
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyOnlyOnEnter;
}

void URadiusImprovedRangeAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.TitleText = RadiusSettings.HostBehaviourText;

	const float TotalRangePercentageGain = GetTotalRangePercentageGain();
	const FString RangePercentageText = FString::SanitizeFloat(TotalRangePercentageGain) + "%";
	const FString RangeDescription = FRTSRichTextConverter::MakeRTSRich(
		"Range by " + RangePercentageText,
		ERTSRichText::Text_Exp);
	const FString RadiusDescription = FRTSRichTextConverter::MakeRTSRich(
		FString::SanitizeFloat(GetAoeBehaviourSettings().Radius / 100.f) + " m radius",
		ERTSRichText::Text_Exp);

	UIData.DescriptionText = "Improves " + RangeDescription + " in " + RadiusDescription;
	Behaviour.SetCustomUIData(UIData);
}

float URadiusImprovedRangeAura::GetTotalRangePercentageGain() const
{
	float TotalRangePercentageGain = 0.f;
	for (const TSubclassOf<UBehaviour> BehaviourType : GetAoeBehaviourSettings().BehaviourTypes)
	{
		if (not BehaviourType)
		{
			continue;
		}

		const UBehaviourWeapon* BehaviourWeapon = Cast<UBehaviourWeapon>(BehaviourType->GetDefaultObject());
		if (not BehaviourWeapon)
		{
			continue;
		}

		const float RangeMultiplier = BehaviourWeapon->GetBehaviourWeaponMultipliers().RangeMlt;
		if (RangeMultiplier <= 1.0f || FMath::IsNearlyZero(RangeMultiplier))
		{
			continue;
		}

		TotalRangePercentageGain += (RangeMultiplier - 1.0f) * 100.0f;
	}

	return TotalRangePercentageGain;
}
