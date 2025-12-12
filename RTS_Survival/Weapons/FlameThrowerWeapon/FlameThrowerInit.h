#pragma once

#include "CoreMinimal.h"
#include "FlameThrowerSettings.h"
#include "RTS_Survival/Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h"

#include "FlameThrowerInit.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateFlameThrower
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 OwningPlayer = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EWeaponName WeaponName = EWeaponName::T26_Mg;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TScriptInterface<IWeaponOwner> WeaponOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FireSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FFlameThrowerSettings FlameSettings;
};