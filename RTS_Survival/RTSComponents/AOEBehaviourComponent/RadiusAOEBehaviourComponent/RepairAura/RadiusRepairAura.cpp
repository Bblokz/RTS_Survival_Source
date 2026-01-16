// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RadiusRepairAura.h"

#include "RTS_Survival/Behaviours/Derived/Heal/SingleHealBehaviour.h"
#include "RTS_Survival/Behaviours/Derived/Heal/TickingHealBehaviour.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

URadiusRepairAura::URadiusRepairAura()
	: URadiusAOEBehaviourComponent()
{
	RadiusSettings.HostBehaviourIcon = EBehaviourIcon::RepairRadius;
	RadiusSettings.RadiusType = ERTSRadiusType::FullCircle_RepairRange;
	// We expect the healing to use timed behaviours that will fade.
	AOEBehaviourSettings.ApplyStrategy = EInAOEBehaviourApplyStrategy::ApplyEveryTick;
	
}

bool URadiusRepairAura::IsValidTarget(AActor* ValidActor) const
{
	return FRTSRepairHelpers::GetIsUnitValidForRepairs(ValidActor);
}

void URadiusRepairAura::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	UIData.TitleText = RadiusSettings.HostBehaviourText;
	const float RepairPerSecond = GetRepairPerSecondFromBehaviours();
	SetDescriptionWithRepairAmount(UIData, RepairPerSecond);
	Behaviour.SetCustomUIData(UIData);
}
	

float URadiusRepairAura::GetRepairPerSecondFromBehaviours() const
{
	float TotalHealingPerSecond = 0.f;
	for (const auto EachBehaviour : GetAoeBehaviourSettings().BehaviourTypes)
	{
		if (not EachBehaviour->IsChildOf(UTickingHealBehaviour::StaticClass())
			|| not EachBehaviour->IsChildOf(USingleHealBehaviour::StaticClass()))
		{
			continue;
		}
		const auto TickHeal = Cast<UTickingHealBehaviour>(EachBehaviour->GetDefaultObject());
		if (TickHeal)
		{
			TotalHealingPerSecond += TickHeal->GetHealingPerSecond();
			continue;
		}
		const auto SingleHeal = Cast<USingleHealBehaviour>(EachBehaviour->GetDefaultObject());
		if (SingleHeal)
		{
			TotalHealingPerSecond += SingleHeal->GetHealing();
		}
	}
	return TotalHealingPerSecond;
}

void URadiusRepairAura::SetDescriptionWithRepairAmount(FBehaviourUIData& OutUIData, const float RepairPerSecond) const
{
	FString Description = "Repairs "+ FRTSRichTextConverter::MakeRTSRich(FString::SanitizeFloat(RepairPerSecond) + " HP/s",
		ERTSRichText::Text_Exp) + " in " + FRTSRichTextConverter::MakeRTSRich(FString::SanitizeFloat(GetAoeBehaviourSettings().Radius/100) + " m radius",
			ERTSRichText::Text_Exp);
	OutUIData.DescriptionText = Description;
}
