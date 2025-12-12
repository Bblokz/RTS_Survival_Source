#pragma once
#include "CoreMinimal.h"

#include "GameUI_InitData.generated.h"

class UW_SelectedUnitDescription;
class UW_AmmoPicker;
class UW_SelectedUnitInfo;
class UW_ItemActionUI;
class UW_WeaponDescription;
class UW_WeaponItem;

USTRUCT(Blueprintable)
struct FInit_WeaponUI
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UW_WeaponItem*> WeaponUIElements = {};

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UW_WeaponDescription* WeaponDescription = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UW_AmmoPicker* AmmoPicker = nullptr;
};

USTRUCT(Blueprintable)
struct FInit_ActionUI
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UW_SelectedUnitInfo* SelectedUnitInfo = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UW_SelectedUnitDescription* SelectedUnitDescription = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UUserWidget* ActionUIDescription = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<UW_ItemActionUI*> ActionUIElementsInMenu = {};
};
