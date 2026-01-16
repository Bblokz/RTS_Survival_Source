// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RadiusHealAura.h"

#include "RTS_Survival/Behaviours/UI/BehaviourUIData.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"


URadiusHealAura::URadiusHealAura()
	: URadiusRepairAura()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	RadiusSettings.HostBehaviourIcon = EBehaviourIcon::HealRadius;
	RadiusSettings.RadiusType = ERTSRadiusType::Fullcircle_HealAura;

}

bool URadiusHealAura::IsValidTarget(AActor* ValidActor) const
{
	return FRTSRepairHelpers::GetIsUnitValidForHealing(ValidActor);
}

void URadiusHealAura::SetDescriptionWithRepairAmount(FBehaviourUIData& OutUIData, const float RepairPerSecond) const
{
	FString Description = "Heals "+ FRTSRichTextConverter::MakeRTSRich(FString::SanitizeFloat(RepairPerSecond) + " HP/s",
		ERTSRichText::Text_Exp) + " in " + FRTSRichTextConverter::MakeRTSRich(FString::SanitizeFloat(GetAoeBehaviourSettings().Radius/100) + " m radius",
			ERTSRichText::Text_Exp);
	OutUIData.DescriptionText = Description;
}


