#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "RTSCommander.generated.h"

UENUM(blueprintType)
enum class ERTSCommander : uint8
{
	NoCommander,
	BalancedCommander
};


USTRUCT(BlueprintType)
struct FRTSCommanderSettings
{
	GENERATED_BODY()
	
	FRTSCommanderSettings();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<EGlobalAbility> GlobalAbilities;
};