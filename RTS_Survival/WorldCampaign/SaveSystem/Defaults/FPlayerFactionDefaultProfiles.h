#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/PlayerWorldData/FPlayerData.h"

#include "FPlayerFactionDefaultProfiles.generated.h"

USTRUCT(BlueprintType)
struct FPlayerFactionDefaultProfiles
{
	GENERATED_BODY()

	// One default profile per faction, hardcoded fields (NO MAP).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default Profiles")
	FPlayerData GerBreakthroughDoctrine;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default Profiles")
	FPlayerData GerStrikeDivision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Default Profiles")
	FPlayerData GerItalianFaction;
};
