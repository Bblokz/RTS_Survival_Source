#pragma once

#include "CoreMinimal.h"
#include "PlayerWorldData/FPlayerData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"

#include "FPlayerProfileSaveData.generated.h"


enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerProfileSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSFaction PlayerFaction = ERTSFaction::GerBreakthroughDoctrine;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerData PlayerData;
};
