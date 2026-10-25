#pragma once

#include "CoreMinimal.h"
#include "FlameThrowerSettings.h"
#include "RTS_Survival/Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponNameConversion.h"

#include "FlameThrowerInit.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FInitWeaponStateFlameThrower
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 OwningPlayer = 0;

	// FName because EWeaponName is uint32 and so not Blueprint-exposable; resolve it with GetWeaponNameEnum().
	// Keep the default at T26_Mg: Blueprints that left the pin at its default rely on it.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (GetOptions = "RTSBlueprintFunctionLibrary.GetWeaponNameOptions"))
	FName WeaponName = FName(TEXT("T26_Mg"));

	EWeaponName GetWeaponNameEnum() const { return Global_GetWeaponNameFromFName(WeaponName); }

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TScriptInterface<IWeaponOwner> WeaponOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UMeshComponent* MeshComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FireSocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FFlameThrowerSettings FlameSettings;
};