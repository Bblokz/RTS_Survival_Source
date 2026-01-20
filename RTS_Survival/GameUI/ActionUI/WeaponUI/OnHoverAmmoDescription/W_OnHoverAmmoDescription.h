// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_OnHoverAmmoDescription.generated.h"

enum class EWeaponShellType : uint8;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_OnHoverAmmoDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowAmmoDescription(const EWeaponShellType ShellType);
	void HideAmmoDescription();

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void BP_SetAmmoDescription(const EWeaponShellType ShellType);
};
