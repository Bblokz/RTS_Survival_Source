#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"

#include "FPlayerProfileData.generated.h"


enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerProfileData
{
	GENERATED_BODY()

	UPROPERTY()
	ERTSFaction PlayerFaction  = ERTSFaction::GerBreakthroughDoctrine;
	
};
