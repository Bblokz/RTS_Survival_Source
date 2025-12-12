// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HealthBarSettings/HealthBarVisibilitySettings.h"
#include "Healthbar_TargetTypeIconState/TargetTypeIconState.h"
#include "RTS_Survival/GameUI/Healthbar/HealthBarSettings/HealthBarSettings.h"
#include "Slate/SlateBrushAsset.h"

#include "W_HealthBar.generated.h"

enum class EWeaponShellType : uint8;
class UTextBlock;
class UImage;
class UHealthComponent;
class UWidgetAnimation;

USTRUCT()
struct FBarState
{
	GENERATED_BODY()
	// between 0 and 1 for progress bar
	// Set to latest value.
	float M_CurrentPercentage = 1;

	// Time when the last damage to the bar occured.
	double M_WorldTimeAtDamageTaken = 0.0f;
};
/**
 * Damage is applied to the healthbar directly with the exception of the Fire Damage;
 * fire damage is applied to the threshold bar which fills when damage builds up; once this bar is full
 * the unit is overheated and takes fire damage.
 */
UCLASS()
class RTS_SURVIVAL_API UW_HealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	// Supposed to be called by owning health component that will propagate health update on upgrades or begin play.
	void OnDamaged(const float NewPercentage);
	// The new percentage to fill the armor_fire_threshold bar.
	void OnFireDamage(const float NewPercentage);
	// On recovery tick of fire threshold; value in threshold bar decreases.
	void OnFireRecovery(const float NewPercentage);
	// To set the amount of slices on the bar.
	void UpdateMaxHealth(const float MaxHealth) const;
	void UpdateMaxFireThreshold (const float MaxFireThreshold) const;

	// For squad units, gets the icon from settings and loads it in blueprints.
	void UpdateSquadWeaponIcon(USlateBrushAsset* NewWeaponIconAsset);
	
	void SetupUnitName(const FString& UnitName);
	void SetSettingsFromHealthComponent(UHealthComponent* OwningHealthComp,
	                                    const FHealthBarCustomization& CustomizationSettings, const
	                                    int8 OwningPlayer);

	// Makes the target type icon invisible if not set.
	void ChangeTargetTypeIcon(const ETargetTypeIcon Icon, const int32 OwningPlayer);

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitRTSHealthBar(
		UWidgetAnimation* DamageAnimation, UMaterialInstanceDynamic* DMIHealthBar,
		UMaterialInstanceDynamic* DMIDamagePoint,
		FDamagePointDynamicMaterialParameter PointDamageParameters,
		FHealthBarDynamicMaterialParameter HealthBarParameters, FHealthBarDMIColorParameter HealthBarColorParameters, UMaterialInstanceDynamic
		* DMIFireThresholdBar, UImage* TargetTypeIconImage
	);

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline float GetCurrentPercentage() const
	{
		return M_HealthBar_State.M_CurrentPercentage;
	}

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline FString GetUnitName() const
	{
		return M_UnitName;
	}

	// If asset is null hide the weapon icon.
	UFUNCTION(BlueprintImplementableEvent, Category="SquadWeapon")
	void BP_UpdateSquadWeaponIcon(USlateBrushAsset* NewWeaponIconAsset);

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> M_HealthBarImage = nullptr;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UImage> M_FireThresholdBarImage = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TObjectPtr<UTextBlock> M_UnitNameText = nullptr;
	
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	UWidgetAnimation* M_DamageAnimation = nullptr;

	FTargetTypeIconState M_TargetTypeIconState;

	FHealthBarCustomization M_CustomizationSettings;
	// Set after adjusting the customization settings depending on the owning player.
	FLinearColor M_CustomHealthBarColor;
	void AdjustCustomizationForPlayer(const int8 OwningPlayer);
	void ApplyCustomization() const;
	const FLinearColor& GetColorGradientForHeath(const float Percentage) const;


	bool GetIsValidDamageAnimation() const;

	void OnDamage_PointDamageAnimation(const float StartHealth, const float TargetHealth);
	void OnDamage_HealthBarAnimation(const float StartHealth, const float TargetHealth) const;
	void OnFireDamage_AnimationIncreaseThresholdBar(const float NewThresholdFill)const;

	void OnHeal_HealthBarAnimation(const float TargetHealth) const;
	void OnHeal_PointDamageAnimation(const float TargetHealth) const;
	void OnFireRecovery_AnimationRecoverThresholdBar(const float CurrentThresholdValue, const float NewThresholdValue) const;

	void UpdateColorForNewPercentage(const float NewPercentage) const;

	// Holds the dynamic material parameters for the point damage animation.
	FDamagePointDynamicMaterialParameter M_PointDamageParameters;

	// Holds the dynamic material parameters for the health bar.
	FHealthBarDynamicMaterialParameter M_HealthBarParameters;

	// Holds the dynamic material parameters for the health bar color.
	FHealthBarDMIColorParameter M_HealthBarColorParameters;

	UPROPERTY()
	UMaterialInstanceDynamic* M_DMI_HealthBar = nullptr;
	FBarState M_HealthBar_State;
	
	UPROPERTY()
	UMaterialInstanceDynamic* M_DMI_FireThresholdBar = nullptr;
	FBarState M_FireThresholdBar_State;

	bool GetIsValidHealthBarDynamicMaterial() const;
	bool GetIsValidFireThresholdBarDynamicMaterial() const;

	UPROPERTY()
	UMaterialInstanceDynamic* M_DMI_DamagePoint = nullptr;

	TWeakObjectPtr<UHealthComponent> M_OwningHealthComponent = nullptr;

	bool GetIsValidOwningHealthComp() const;
	bool EnsureHealthBarImageIsValid() const;

	// Set by owning health component's reference to the RTS component.
	FString M_UnitName;

	void ReportErrorWithOwner(const FString& Message) const;

};
