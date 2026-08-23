// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_ShieldBar.generated.h"

/**
 * @brief Base widget used by shield components to present their independent shield capacity.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ShieldBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the Blueprint presentation while keeping capacity normalization in native code. */
	void UpdateShieldValues(float CurrentShields, float MaxShields);

	UFUNCTION(BlueprintPure, Category = "Shield")
	float GetShieldPercentage() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Shield")
	void BP_OnShieldValuesChanged(float CurrentShields, float MaxShields, float ShieldPercentage);

private:
	UPROPERTY()
	float M_CurrentShields = 0.0f;

	UPROPERTY()
	float M_MaxShields = 0.0f;
};
