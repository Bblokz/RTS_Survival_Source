#pragma once

#include "CoreMinimal.h"
#include "PlayerCardData/PlayerCardData.h"
#include "PlayerWorldData/FPlayerWorldSaveData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"

#include "FPlayerProfileSaveData.generated.h"


enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerProfileSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSFaction PlayerFaction = ERTSFaction::GerBreakthroughDoctrine;
	
	// Replaces everything the old USavePlayerProfile stored (card-related).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerCardSaveData CardData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerWorldSaveData WorldData;
};
