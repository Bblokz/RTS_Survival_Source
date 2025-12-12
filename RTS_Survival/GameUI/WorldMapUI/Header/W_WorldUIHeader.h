// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "W_WorldUIHeader.generated.h"

class UW_TurnCounter;
/**
 * This class implements the navigation header of the world map UI as well as the turn counter and
 * resources.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldUIHeader : public UUserWidget
{
	GENERATED_BODY()

protected:
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_TurnCounter> W_BP_HeaderTurns;
	
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_CommandPerksButton;
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_ArmyLayoutButton;
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_WorldMapButton;
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_ArchiveButton;
UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_TechTree;

private:
	static bool EnsureButtonIsValid(const TObjectPtr<UButton>& ButtonToCheck);
};
