// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_Behaviour.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "RTS_Survival/Behaviours/ProjectSettings/BehaviourButtonSettings.h"
#include "RTS_Survival/Behaviours/UI/BehaviourContainer/W_BehaviourContainer.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_Behaviour::InitBehaviourWidget(UW_BehaviourContainer* InBehaviourContainer)
{
	M_BehaviourContainer = InBehaviourContainer;
	if (not GetIsValidBehaviourContainer())
	{
		return;
	}

	if (not IsValid(BehaviourButton))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "BehaviourButton", "UW_Behaviour::InitBehaviourWidget");
		return;
	}

	BehaviourButton->OnHovered.AddDynamic(this, &UW_Behaviour::OnHoveredButton);
	BehaviourButton->OnUnhovered.AddDynamic(this, &UW_Behaviour::OnUnHoverButton);
}

void UW_Behaviour::SetupBehaviourWidget(const FBehaviourUIData& InBehaviourUIData)
{
	M_BehaviourUIData = InBehaviourUIData;
	ApplyBehaviourIcon();
}

void UW_Behaviour::OnHoveredButton()
{
	if (not GetIsValidBehaviourContainer())
	{
		return;
	}

	M_BehaviourContainer->OnBehaviourHovered(true, M_BehaviourUIData);
}

void UW_Behaviour::OnUnHoverButton()
{
	if (not GetIsValidBehaviourContainer())
	{
		return;
	}

	M_BehaviourContainer->OnBehaviourHovered(false, M_BehaviourUIData);
}

bool UW_Behaviour::GetIsValidBehaviourContainer() const
{
	if (M_BehaviourContainer.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_BehaviourContainer", "UW_Behaviour::GetIsValidBehaviourContainer");
	return false;
}

const UBehaviourButtonSettings* UW_Behaviour::GetBehaviourButtonSettings()
{
	static const UBehaviourButtonSettings* CachedSettings = UBehaviourButtonSettings::Get();
	return CachedSettings;
}

void UW_Behaviour::ApplyBehaviourIcon()
{
	if (not IsValid(BehaviourImage))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this, "BehaviourImage", "UW_Behaviour::ApplyBehaviourIcon");
		return;
	}

	const UBehaviourButtonSettings* BehaviourButtonSettings = GetBehaviourButtonSettings();
	if (BehaviourButtonSettings == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UW_Behaviour::ApplyBehaviourIcon: Unable to access behaviour button settings."));
		return;
	}

	static TMap<EBehaviourIcon, FBehaviourWidgetStyle> CachedBehaviourIconStyles;
	static bool bHasCachedBehaviourIconStyles = false;

	if (not bHasCachedBehaviourIconStyles)
	{
		BehaviourButtonSettings->ResolveBehaviourIconStyles(CachedBehaviourIconStyles);
		bHasCachedBehaviourIconStyles = true;
	}

	const FBehaviourWidgetStyle* BehaviourStyle = CachedBehaviourIconStyles.Find(M_BehaviourUIData.BehaviourIcon);
	if (not BehaviourStyle || not IsValid(BehaviourStyle->IconTexture))
	{
		const FString IconAsString = UEnum::GetValueAsString(M_BehaviourUIData.BehaviourIcon);
		RTSFunctionLibrary::ReportError(
			TEXT("UW_Behaviour::ApplyBehaviourIcon: Unable to find icon texture for behaviour icon.")
			"\n Icon: " + IconAsString);
		return;
	}

	BehaviourImage->SetBrushFromTexture(BehaviourStyle->IconTexture);
}
