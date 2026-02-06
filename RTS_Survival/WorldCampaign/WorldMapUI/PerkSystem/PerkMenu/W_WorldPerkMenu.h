// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_WorldPerkMenu.generated.h"

class UW_WorldMenu;
class UW_WorldPerkGraph;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldPerkMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitPerkMenu(UW_WorldMenu* WorldMenu);

protected:
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkGraph> InfantryMechanizedPerkGraph;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkGraph> LightMediumPerkGraph;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkGraph> HeavyPerkGraph;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_WorldPerkGraph> AircraftPerkGraph;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;
	bool GetIsValidWorldMenu() const;
};
