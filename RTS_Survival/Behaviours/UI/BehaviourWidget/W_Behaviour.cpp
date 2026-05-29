// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_Behaviour.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "RTS_Survival/Behaviours/ProjectSettings/BehaviourButtonSettings.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIconStyleDataAsset.h"
#include "RTS_Survival/Behaviours/UI/BehaviourContainer/W_BehaviourContainer.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Styling/SlateBrush.h"

void UW_Behaviour::InitBehaviourWidget(UW_BehaviourContainer* InBehaviourContainer)
{
        M_BehaviourContainer = InBehaviourContainer;
        if (not GetIsValidBehaviourContainer())
        {
                return;
        }

        if (not GetIsValidBehaviourButton())
        {
                return;
        }

        BehaviourButton->OnHovered.RemoveDynamic(this, &UW_Behaviour::OnHoveredButton);
        BehaviourButton->OnUnhovered.RemoveDynamic(this, &UW_Behaviour::OnUnHoverButton);
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

        RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
                this,
                "M_BehaviourContainer",
                "UW_Behaviour::GetIsValidBehaviourContainer",
                this
        );
        return false;
}

bool UW_Behaviour::GetIsValidBehaviourButton() const
{
        if (IsValid(BehaviourButton))
        {
                return true;
        }

        RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
                this,
                "BehaviourButton",
                "UW_Behaviour::GetIsValidBehaviourButton",
                this
        );
        return false;
}

bool UW_Behaviour::GetIsValidBehaviourImage() const
{
        if (IsValid(BehaviourImage))
        {
                return true;
        }

        RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
                this,
                "BehaviourImage",
                "UW_Behaviour::GetIsValidBehaviourImage",
                this
        );
        return false;
}

const UBehaviourButtonSettings* UW_Behaviour::GetBehaviourButtonSettings()
{
        return UBehaviourButtonSettings::Get();
}

bool UW_Behaviour::GetIsValidBehaviourButtonSettings(const UBehaviourButtonSettings* BehaviourButtonSettings)
{
        if (IsValid(BehaviourButtonSettings))
        {
                return true;
        }

        RTSFunctionLibrary::ReportError(TEXT("UW_Behaviour::GetIsValidBehaviourButtonSettings: Behaviour button settings are invalid."));
        return false;
}

bool UW_Behaviour::GetIsValidBehaviourIconStyleDataAsset(
        const UBehaviourIconStyleDataAsset* BehaviourIconStyleDataAsset)
{
        if (IsValid(BehaviourIconStyleDataAsset))
        {
                return true;
        }

        RTSFunctionLibrary::ReportError(
                TEXT("UW_Behaviour::GetIsValidBehaviourIconStyleDataAsset: Behaviour icon style Data Asset is not configured "
                     "or could not be loaded."));
        return false;
}

void UW_Behaviour::ClearBehaviourIconBrush()
{
        if (not GetIsValidBehaviourImage())
        {
                return;
        }

        M_AppliedIconTexture = nullptr;
        BehaviourImage->SetBrush(FSlateNoResource());
}

void UW_Behaviour::ApplyBehaviourIcon()
{
        if (not GetIsValidBehaviourImage())
        {
                return;
        }

        const UBehaviourButtonSettings* BehaviourButtonSettings = GetBehaviourButtonSettings();
        if (not GetIsValidBehaviourButtonSettings(BehaviourButtonSettings))
        {
                ClearBehaviourIconBrush();
                return;
        }

        const UBehaviourIconStyleDataAsset* BehaviourIconStyleDataAsset =
                BehaviourButtonSettings->GetBehaviourIconStyleDataAsset();
        if (not GetIsValidBehaviourIconStyleDataAsset(BehaviourIconStyleDataAsset))
        {
                ClearBehaviourIconBrush();
                return;
        }

        const FBehaviourIconWidgetStyle* BehaviourIconWidgetStyle =
                GetBehaviourIconWidgetStyle(BehaviourIconStyleDataAsset);
        if (not BehaviourIconWidgetStyle)
        {
                ClearBehaviourIconBrush();
                return;
        }

        if (not ApplyBehaviourIconTexture(*BehaviourIconWidgetStyle))
        {
                ClearBehaviourIconBrush();
        }
}

const FBehaviourIconWidgetStyle* UW_Behaviour::GetBehaviourIconWidgetStyle(
        const UBehaviourIconStyleDataAsset* BehaviourIconStyleDataAsset) const
{
        const FBehaviourIconWidgetStyle* BehaviourIconWidgetStyle =
                BehaviourIconStyleDataAsset->FindBehaviourIconStyle(M_BehaviourUIData.BehaviourIcon);
        if (BehaviourIconWidgetStyle)
        {
                return BehaviourIconWidgetStyle;
        }

        const FString IconAsString = UEnum::GetValueAsString(M_BehaviourUIData.BehaviourIcon);
        RTSFunctionLibrary::ReportError(
                TEXT("UW_Behaviour::GetBehaviourIconWidgetStyle: Unable to find style for behaviour icon.")
                "\n Icon: " + IconAsString);
        return nullptr;
}

bool UW_Behaviour::ApplyBehaviourIconTexture(const FBehaviourIconWidgetStyle& BehaviourIconWidgetStyle)
{
        if (not IsValid(BehaviourIconWidgetStyle.IconTexture))
        {
                const FString IconAsString = UEnum::GetValueAsString(M_BehaviourUIData.BehaviourIcon);
                RTSFunctionLibrary::ReportError(
                        TEXT("UW_Behaviour::ApplyBehaviourIconTexture: Behaviour icon style has an invalid texture.")
                        "\n Icon: " + IconAsString);
                return false;
        }

        M_AppliedIconTexture = BehaviourIconWidgetStyle.IconTexture;
        BehaviourImage->SetBrushFromTexture(M_AppliedIconTexture, false);
        return true;
}
