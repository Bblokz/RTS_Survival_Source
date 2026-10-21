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

};

static FString Global_RocketAbilityEnumString(const ERocketAbility Item)
{
    const FName Name = StaticEnum<ERocketAbility>()->GetNameByValue((int64)Item);
    const FName CleanName = FName(*Name.ToString().RightChop(FString("ERocketAbility::").Len()));
    return CleanName.ToString();
}
