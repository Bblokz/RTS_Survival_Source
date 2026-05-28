// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_BehaviourDescription.h"

#include "Components/Border.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/Behaviours/Lifetime/BehaviourLifeTime.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "Styling/SlateBrush.h"

void UW_BehaviourDescription::SetupDescription(const FBehaviourUIData& InBehaviourUIData)
{
	if (not DescriptionBox || not TitleBox)
	{
		return;
	}
	DescriptionBox->SetText(FText::FromString(InBehaviourUIData.DescriptionText));
	TitleBox->SetText(FText::FromString(InBehaviourUIData.TitleText));
	SetupLifeTimeDescription(InBehaviourUIData);
	SetupStyle(InBehaviourUIData);
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

void UW_BehaviourDescription::SetupStyle(const FBehaviourUIData& InBehaviourUIData)
{
	if (not BehaviourBorder)
	{
		return;
	}

	const FBehaviourDescriptionStyle* Style = BehaviourDescriptionStyles.Find(InBehaviourUIData.BuffDebuffType);
	if (Style == nullptr)
	{
		const FString TypeAsString = UEnum::GetValueAsString(InBehaviourUIData.BuffDebuffType);
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourDescription::SetupStyle: No style found for given BuffDebuffType!"
				"\n type: " + TypeAsString));
		ClearBehaviourPanelBrush();
		return;
	}

	if (not IsValid(Style->PanelTexture))
	{
		const FString TypeAsString = UEnum::GetValueAsString(InBehaviourUIData.BuffDebuffType);
		RTSFunctionLibrary::ReportError(
			TEXT("UW_BehaviourDescription::SetupStyle: Style panel texture is invalid!"
				"\n type: " + TypeAsString));
		ClearBehaviourPanelBrush();
		return;
	}

	M_AppliedPanelTexture = Style->PanelTexture;
	BehaviourBorder->SetBrushFromTexture(M_AppliedPanelTexture);
	
}

void UW_BehaviourDescription::ClearBehaviourPanelBrush()
{
	if (not BehaviourBorder)
	{
		return;
	}

	M_AppliedPanelTexture = nullptr;
	BehaviourBorder->SetBrush(FSlateNoResource());
}
