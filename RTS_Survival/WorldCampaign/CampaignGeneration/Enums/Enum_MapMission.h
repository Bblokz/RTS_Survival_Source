#pragma once

#include "CoreMinimal.h"
#include "Enum_MapMission.generated.h"

UENUM(BlueprintType)
enum class EMapMission : uint8
{
	None,
	HetzerMission,
	TigerPantherRange,
	JagdpantherMission,
	SturmtigerDam
};
