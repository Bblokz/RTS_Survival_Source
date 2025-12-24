// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_HealthBar.h"

#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Healthbar_TargetTypeIconState/ProjectSettings/TargetTypeIconSettings.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


void UW_HealthBar::OnDamaged(const float NewPercentage)
{
	if (not GetIsValidHealthBarDynamicMaterial() || not GetIsValidOwningHealthComp())
	{
		return;
	}
	if (NewPercentage >= M_HealthBar_State.M_CurrentPercentage)
	{
		OnHeal_PointDamageAnimation(NewPercentage);
		OnHeal_HealthBarAnimation(NewPercentage);
	}
	else
	{
		M_HealthBar_State.M_WorldTimeAtDamageTaken = M_OwningHealthComponent->GetWorld()->GetTimeSeconds();
		OnDamage_PointDamageAnimation(M_HealthBar_State.M_CurrentPercentage, NewPercentage);
		OnDamage_HealthBarAnimation(M_HealthBar_State.M_CurrentPercentage, NewPercentage);
	}
	UpdateColorForNewPercentage(NewPercentage);
	M_HealthBar_State.M_CurrentPercentage = NewPercentage;
}

void UW_HealthBar::OnFireDamage(const float NewPercentage)
{
	// NewPercentage is 0..1
	if (M_FireThresholdBarImage && NewPercentage > 0.f)
	{
		M_FireThresholdBarImage->SetVisibility(ESlateVisibility::Visible);
	}

	OnFireDamage_AnimationIncreaseThresholdBar(NewPercentage);
	M_FireThresholdBar_State.M_CurrentPercentage = NewPercentage;
}

void UW_HealthBar::OnFireRecovery(const float NewPercentage)
{
	if (not GetIsValidOwningHealthComp())
	{
		return;
	}

	M_FireThresholdBar_State.M_WorldTimeAtDamageTaken = M_OwningHealthComponent->GetWorld()->GetTimeSeconds();
	OnFireRecovery_AnimationRecoverThresholdBar(M_FireThresholdBar_State.M_CurrentPercentage, NewPercentage);
	M_FireThresholdBar_State.M_CurrentPercentage = NewPercentage;

	if (M_FireThresholdBarImage && NewPercentage <= KINDA_SMALL_NUMBER)
	{
		M_FireThresholdBarImage->SetVisibility(ESlateVisibility::Hidden);
	}
}


void UW_HealthBar::UpdateMaxHealth(const float MaxHealth) const
{
	if (not GetIsValidHealthBarDynamicMaterial())
	{
		return;
	}
	const int32 AmountSlices = FMath::Floor(
		MaxHealth / DeveloperSettings::GamePlay::HealthCompAndHealthBar::HPBarSliceRatio);
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarColorParameters.AmountSliceName, AmountSlices);
}

void UW_HealthBar::UpdateMaxFireThreshold(const float MaxFireThreshold) const
{
	if (not GetIsValidFireThresholdBarDynamicMaterial())
	{
		return;
	}
	const int32 AmountSlices = FMath::Floor(
		MaxFireThreshold / DeveloperSettings::GamePlay::HealthCompAndHealthBar::FireThresholdBarSliceRatio);
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarColorParameters.AmountSliceName, AmountSlices);
}

void UW_HealthBar::UpdateSquadWeaponIcon(const FSquadWeaponIconDisplaySettings& NewWeaponIconSettings)
{
	BP_UpdateSquadWeaponIcon(NewWeaponIconSettings);
}

void UW_HealthBar::SetupUnitName(const FString& UnitName)
{
	M_UnitName = UnitName;
	if (M_UnitNameText)
	{
		const FText Text = FText::FromString(UnitName);
		M_UnitNameText->SetText(Text);
	}
}


void UW_HealthBar::SetSettingsFromHealthComponent(UHealthComponent* OwningHealthComp,
                                                  const FHealthBarCustomization& CustomizationSettings,
                                                  const int8 OwningPlayer)
{
	M_OwningHealthComponent = OwningHealthComp;
	M_CustomizationSettings = CustomizationSettings;
	AdjustCustomizationForPlayer(OwningPlayer);
	ApplyCustomization();
}

void UW_HealthBar::ChangeTargetTypeIcon(const ETargetTypeIcon Icon, const int32 OwningPlayer)
{
	if (not M_TargetTypeIconState.GetImplementsTargetTypeIcon())
	{
		return;
	}
	const AActor* ActorContext = nullptr;
	if (M_OwningHealthComponent.IsValid())
	{
		ActorContext = M_OwningHealthComponent->GetOwner();
	}
	M_TargetTypeIconState.SetNewTargetTypeIcon(OwningPlayer, Icon, ActorContext);
}

void UW_HealthBar::InitRTSHealthBar(UWidgetAnimation* DamageAnimation, UMaterialInstanceDynamic* DMIHealthBar,
                                    UMaterialInstanceDynamic* DMIDamagePoint, FDamagePointDynamicMaterialParameter
                                    PointDamageParameters, FHealthBarDynamicMaterialParameter HealthBarParameters,
                                    FHealthBarDMIColorParameter
                                    HealthBarColorParameters, UMaterialInstanceDynamic* DMIFireThresholdBar,
                                    UImage* TargetTypeIconImage)
{
	M_DamageAnimation = DamageAnimation;
	M_DMI_HealthBar = DMIHealthBar;
	M_DMI_FireThresholdBar = DMIFireThresholdBar;
	M_DMI_DamagePoint = DMIDamagePoint;
	M_PointDamageParameters = PointDamageParameters;
	M_HealthBarParameters = HealthBarParameters;
	M_TargetTypeIconState.TargetTypeIconImage = TargetTypeIconImage;
	if (GetIsValidHealthBarDynamicMaterial())
	{
		// Initialise with 1.0f as the health bar is full at the start.
		M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.StartHealthName, 1.0f);
		M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.TargetHealthName, 1.0f);
	}
	if (M_DMI_DamagePoint)
	{
		M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.StartHealthName, 1.0f);
		M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.TargetHealthName, 1.0f);
	}
}

void UW_HealthBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (M_FireThresholdBarImage)
	{
		M_FireThresholdBarImage->SetVisibility(ESlateVisibility::Hidden);
	}

	// Load target type icon from game settings.
	if (const UTargetTypeIconSettings* Settings = UTargetTypeIconSettings::Get())
	{
		TMap<ETargetTypeIcon, FTargetTypeIconBrushes> ResolvedMap;
		Settings->ResolveTypeToBrushMap(ResolvedMap);

		if (ResolvedMap.Num() > 0)
		{
			M_TargetTypeIconState.TypeToBrush = MoveTemp(ResolvedMap);
		}
	}
}

void UW_HealthBar::AdjustCustomizationForPlayer(const int8 OwningPlayer)
{
	if (OwningPlayer != 1 && M_CustomizationSettings.bUseEnemyColorIfEnemy)
	{
		M_CustomizationSettings.bUseGreenRedGradient = false;
		M_CustomHealthBarColor = M_CustomizationSettings.EnemyColor;
		return;
	}
	if (not M_CustomizationSettings.bUseGreenRedGradient)
	{
		// Ready overwrite color.
		M_CustomHealthBarColor = M_CustomizationSettings.OverWriteGradientColor;
	}
}

void UW_HealthBar::ApplyCustomization() const
{
	if (not GetIsValidHealthBarDynamicMaterial())
	{
		return;
	}
	if (M_CustomizationSettings.bUseGreenRedGradient)
	{
		// Set to white to ensure that the coloring of the health image's gradient is not affected by the color on the shader. 
		const FLinearColor White = FLinearColor::White;
		M_DMI_HealthBar->SetVectorParameterValue(M_HealthBarColorParameters.ProgressBarColorName, White);
		// If using the gradient is true then we are not an enemy and can use the default damage color.
		M_DMI_HealthBar->SetVectorParameterValue(M_HealthBarColorParameters.DamageBarColorName,
		                                         M_CustomizationSettings.DamageBarColor);
		if (M_HealthBarImage)
		{
			const FLinearColor& Color = GetColorGradientForHeath(1.0);
			M_HealthBarImage->SetColorAndOpacity(Color);
		}
		if (M_UnitNameText)
		{
			const FLinearColor& Color = GetColorGradientForHeath(1.0);
			M_UnitNameText->SetColorAndOpacity(Color);
		}
	}
	else
	{
		// Set by adjustment for player.
		M_DMI_HealthBar->SetVectorParameterValue(M_HealthBarColorParameters.ProgressBarColorName,
		                                         M_CustomHealthBarColor);
		M_DMI_HealthBar->SetVectorParameterValue(M_HealthBarColorParameters.DamageBarColorName,
		                                         M_CustomizationSettings.EnemyDamageColor);
		if (M_UnitNameText)
		{
			M_UnitNameText->SetColorAndOpacity(M_CustomizationSettings.EnemyColor);
		}
	}
	M_DMI_HealthBar->SetVectorParameterValue(M_HealthBarColorParameters.BgBarColorName,
	                                         M_CustomizationSettings.BackgroundColor);
}

const FLinearColor& UW_HealthBar::GetColorGradientForHeath(const float Percentage) const
{
	// 1) map [0.2,1.0] → [0.0,1.0], clamped
	const float Normalized = FMath::GetMappedRangeValueClamped(
		FVector2f(0.2f, 1.0f),
		FVector2f(0.0f, 1.0f),
		Percentage
	);

	// 2) lerp HSV from pure red → pure green
	static FLinearColor GradientColor;
	GradientColor = FLinearColor::LerpUsingHSV(
		FLinearColor::Red,
		FLinearColor::Green,
		Normalized
	);

	return GradientColor;
}


bool UW_HealthBar::GetIsValidDamageAnimation() const
{
	if (IsValid(M_DamageAnimation))
	{
		return true;
	}
	ReportErrorWithOwner("No valid damage animation set.");
	return false;
}

void UW_HealthBar::OnDamage_PointDamageAnimation(const float StartHealth, const float TargetHealth)
{
	if (not M_DMI_DamagePoint)
	{
		// Return silently as some healthbars will be setup with no point damage animation UI elements.
		return;
	}
	// IF the elements do exist we also expect the animation.
	if (not GetIsValidDamageAnimation())
	{
		return;
	}
	M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.StartHealthName, StartHealth);
	M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.TargetHealthName, TargetHealth);
	M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.StartTimeDamageName,
	                                           M_HealthBar_State.M_WorldTimeAtDamageTaken);
	// Check if we are rendered.
	if (IsRendered())
	{
		PlayAnimation(M_DamageAnimation);
	}
}

void UW_HealthBar::OnDamage_HealthBarAnimation(const float StartHealth, const float TargetHealth) const
{
	if (not GetIsValidHealthBarDynamicMaterial())
	{
		return;
	}
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.StartHealthName, StartHealth);
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.TargetHealthName, TargetHealth);
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.StartTimeDamageName,
	                                         M_HealthBar_State.M_WorldTimeAtDamageTaken);
}

void UW_HealthBar::OnFireDamage_AnimationIncreaseThresholdBar(const float NewThresholdFill) const
{
	if (not GetIsValidFireThresholdBarDynamicMaterial())
	{
		return;
	}
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarParameters.StartHealthName, NewThresholdFill);
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarParameters.TargetHealthName, NewThresholdFill);
}

void UW_HealthBar::OnHeal_HealthBarAnimation(const float TargetHealth) const
{
	if (not GetIsValidHealthBarDynamicMaterial())
	{
		return;
	}
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.TargetHealthName, TargetHealth);
	M_DMI_HealthBar->SetScalarParameterValue(M_HealthBarParameters.StartHealthName, TargetHealth);
}

void UW_HealthBar::OnHeal_PointDamageAnimation(const float TargetHealth) const
{
	if (not M_DMI_DamagePoint)
	{
		// Return silently as some healthbars will be setup with no point damage animation UI elements.
		return;
	}
	M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.TargetHealthName, TargetHealth);
	M_DMI_DamagePoint->SetScalarParameterValue(M_PointDamageParameters.StartHealthName, TargetHealth);
}

void UW_HealthBar::OnFireRecovery_AnimationRecoverThresholdBar(const float CurrentThresholdValue,
                                                               const float NewThresholdValue) const
{
	if (not GetIsValidFireThresholdBarDynamicMaterial())
	{
		return;
	}
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarParameters.StartHealthName, CurrentThresholdValue);
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarParameters.TargetHealthName, NewThresholdValue);
	M_DMI_FireThresholdBar->SetScalarParameterValue(M_HealthBarParameters.StartTimeDamageName,
	                                                M_FireThresholdBar_State.M_WorldTimeAtDamageTaken);
}

void UW_HealthBar::UpdateColorForNewPercentage(const float NewPercentage) const
{
	if (not M_CustomizationSettings.bUseGreenRedGradient)
	{
		// If we overwrite the gradient-color then the color was already set.
		return;
	}
	if (not EnsureHealthBarImageIsValid())
	{
		return;
	}
	const FLinearColor& Color = GetColorGradientForHeath(NewPercentage);
	M_HealthBarImage->SetColorAndOpacity(Color);
}

bool UW_HealthBar::GetIsValidHealthBarDynamicMaterial() const
{
	if (IsValid(M_DMI_HealthBar))
	{
		return true;
	}
	ReportErrorWithOwner("No valid health bar dynamic material set.");
	return false;
}

bool UW_HealthBar::GetIsValidFireThresholdBarDynamicMaterial() const
{
	if (IsValid(M_DMI_FireThresholdBar))
	{
		return true;
	}
	ReportErrorWithOwner("No valid armor bar dynamic material set.");
	return false;
}

bool UW_HealthBar::GetIsValidOwningHealthComp() const
{
	if (M_OwningHealthComponent.IsValid())
	{
		return true;
	}
	ReportErrorWithOwner("No valid Owning Health Component");
	return false;
}

bool UW_HealthBar::EnsureHealthBarImageIsValid() const
{
	if (IsValid(M_HealthBarImage))
	{
		return true;
	}
	ReportErrorWithOwner("No valid health bar image set."
		"\n Need M_HealthBarImage");
	return false;
}

void UW_HealthBar::ReportErrorWithOwner(const FString& Message) const
{
	FString OwnerName = "null";
	if (M_OwningHealthComponent.IsValid())
	{
		OwnerName = IsValid(M_OwningHealthComponent->GetOwner())
			            ? M_OwningHealthComponent->GetOwner()->GetName()
			            : "Null";
	}
	RTSFunctionLibrary::ReportError(Message + "\n With owner: " + OwnerName);
}
