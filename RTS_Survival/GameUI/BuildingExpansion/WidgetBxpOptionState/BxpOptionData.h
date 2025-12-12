#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"

#include "BxpOptionData.generated.h"

/** Which group/column an option should appear in. */
UENUM(BlueprintType)
enum class EBxpOptionSection : uint8
{
	BOS_Tech     UMETA(DisplayName="Tech"),
	BOS_Economic UMETA(DisplayName="Economic"),
	BOS_Defense  UMETA(DisplayName="Defense"),
};

USTRUCT(BlueprintType)
struct FBxpOptionData
{
	GENERATED_BODY()

	// Defines how this building expansion option can be placed.
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FBxpConstructionRules BxpConstructionRules;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	EBuildingExpansionType ExpansionType;

	/** In which Option section this expansion should be shown. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EBxpOptionSection Section = EBxpOptionSection::BOS_Tech;
};
