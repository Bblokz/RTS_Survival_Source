#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"

#include "FPlayerProfileSaveData.generated.h"


enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerProfileSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	ERTSFaction PlayerFaction  = ERTSFaction::GerBreakthroughDoctrine;
	
};
