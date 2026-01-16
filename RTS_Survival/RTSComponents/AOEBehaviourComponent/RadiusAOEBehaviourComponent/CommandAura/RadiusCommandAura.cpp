// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadiusCommandAura.h"

#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

URadiusCommandAura::URadiusCommandAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.HostBehaviourIcon = EBehaviourIcon::MoraleBoostRadius;
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_CommandAura;
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyOnlyOnEnter;
}

void URadiusCommandAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.TitleText = RadiusSettings.HostBehaviourTitleText;

	const float AccuracyPercentageGain = GetTotalAccuracyPercentageGain();
	const float ReloadTimePercentageReduction = GetTotalReloadTimePercentageReduction();

	const FString AccuracyDescription = FRTSRichTextConverter::MakeRTSRich(
		"Accuracy by " + FString::SanitizeFloat(AccuracyPercentageGain) + "%",
		ERTSRichText::Text_Exp);
	const FString ReloadDescription = FRTSRichTextConverter::MakeRTSRich(
		"Reload time by " + FString::SanitizeFloat(ReloadTimePercentageReduction) + "%",
		ERTSRichText::Text_Exp);
	const FString RadiusDescription = FRTSRichTextConverter::MakeRTSRich(
		FString::SanitizeFloat(GetAoeBehaviourSettings().Radius / 100.f) + " m radius",
		ERTSRichText::Text_Exp);

	UIData.DescriptionText = "Improves " + AccuracyDescription + " and " + ReloadDescription + " in " +
		RadiusDescription;
	Behaviour.SetCustomUIData(UIData);
}

float URadiusCommandAura::GetTotalAccuracyPercentageGain() const
{
	float TotalAccuracyPercentageGain = 0.f;
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

		const float AccuracyMultiplier = BehaviourWeapon->GetBehaviourWeaponMultipliers().AccuracyMlt;
		if (AccuracyMultiplier <= 1.0f || FMath::IsNearlyZero(AccuracyMultiplier))
		{
			continue;
		}

		TotalAccuracyPercentageGain += (AccuracyMultiplier - 1.0f) * 100.0f;
	}

	return TotalAccuracyPercentageGain;
}

float URadiusCommandAura::GetTotalReloadTimePercentageReduction() const
{
	float TotalReloadTimePercentageReduction = 0.f;
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

		const float ReloadSpeedMultiplier = BehaviourWeapon->GetBehaviourWeaponMultipliers().ReloadSpeedMlt;
		if (ReloadSpeedMultiplier <= 0.0f || ReloadSpeedMultiplier >= 1.0f)
		{
			continue;
		}

		TotalReloadTimePercentageReduction += (1.0f - ReloadSpeedMultiplier) * 100.0f;
	}

	return TotalReloadTimePercentageReduction;
}
