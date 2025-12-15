// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BehaviourDescription.h"

#include "Components/Border.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Behaviours/Lifetime/BehaviourLifeTime.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_BehaviourDescription::SetupDescription(const FBehaviourUIData& InBehaviourUIData) const
{
	if (not DescriptionBox || not TitleBox)
	{
		return;
	}
	DescriptionBox->SetText(FText::FromString(InBehaviourUIData.DescriptionText));
	TitleBox->SetText(FText::FromString(InBehaviourUIData.TitleText));
	SetupLifeTimeDescription(InBehaviourUIData);
}

void UW_BehaviourDescription::SetupLifeTimeDescription(const FBehaviourUIData& InBehaviourUIData) const
{
	if (not TypeDescriptionBox)
	{
		return;
	}
	switch (InBehaviourUIData.LifeTimeType)
	{
	case EBehaviourLifeTime::None:
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourDescription::SetupLifeTimeDescription: LifeTimeType is None!"));
		break;
	case EBehaviourLifeTime::Permanent:
		TypeDescriptionBox->SetText(
			FRTSRichTextConverter::MakeRTSRichText("Permanent Passive", ERTSRichText::Text_Cursive));
		break;
	case EBehaviourLifeTime::Timed:
		TypeDescriptionBox->SetText(FRTSRichTextConverter::MakeRTSRichText(
			"Timed Passive: " + FString::SanitizeFloat(InBehaviourUIData.TotalLifeTime) + " Seconds",
			ERTSRichText::Text_Cursive));
		break;
	default: ;
	}
}

void UW_BehaviourDescription::SetupStyle(const FBehaviourUIData& InBehaviourUIData) const
{
	if(not BehaviourBorder)
	{
		return;
	}
	if(not BehaviourDescriptionStyles.Contains(InBehaviourUIData.BuffDebuffType))
	{
		const FString TypeAsString = UEnum::GetValueAsString(InBehaviourUIData.BuffDebuffType);
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourDescription::SetupStyle: No style found for given BuffDebuffType!"
				"\n type: " + TypeAsString));
		return;
	}
	const FBehaviourDescriptionStyle& Style = BehaviourDescriptionStyles[InBehaviourUIData.BuffDebuffType];
	BehaviourBorder->SetBrushFromTexture(Style.PanelTexture);
	
}
