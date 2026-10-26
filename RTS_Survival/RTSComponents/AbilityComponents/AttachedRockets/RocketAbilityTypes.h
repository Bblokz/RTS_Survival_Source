#pragma once

#include "CoreMinimal.h"

#include "RocketAbilityTypes.generated.h"


UENUM(BlueprintType)
enum class ERocketAbility : uint8
{
	None,
	HetzerRockets UMETA(DisplayName = "Hetzer Rockets"),
	JagdPantherRockets UMETA(DisplayName = "JagdPanther Rockets"),
	// Append only: Blueprints store these values.
	Pz38tRockets UMETA(DisplayName = "Pz 38(t) Rockets"),
	// Tiger II Rocketeer: 7 large rockets on the turret racks, fired one by one.
	TigerIIRockets UMETA(DisplayName = "Tiger II Rockets"),
	// Sd.Kfz. 251 Mortar Incendiary: six 32 cm incendiary rockets that leave burning ground.
	Sdkfz251IncendiaryRockets UMETA(DisplayName = "Sd.Kfz. 251 Incendiary Rockets"),
	// Sd.Kfz. 251 Mortar Guided Missile: four missiles that home onto one target per salvo.
	Sdkfz251GuidedMissiles UMETA(DisplayName = "Sd.Kfz. 251 Guided Missiles"),

};

static FString Global_RocketAbilityEnumString(const ERocketAbility Item)
{
    const FName Name = StaticEnum<ERocketAbility>()->GetNameByValue((int64)Item);
    const FName CleanName = FName(*Name.ToString().RightChop(FString("ERocketAbility::").Len()));
    return CleanName.ToString();
}
