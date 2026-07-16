#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldStrengthContribution.generated.h"

/**
 * @brief Describes one typed, signed contribution to a world map object's operation strength.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldStrengthContribution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	EWorldStrengthTypes StrengthType = EWorldStrengthTypes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength",
		meta = (MultiLine = true))
	FText StrengthReason;

	// Positive values strengthen the enemy operation; negative values reduce its strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	int32 StrengthPercentage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	EWorldFortificationStrength FortificationModification = EWorldFortificationStrength::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	EWorldFieldDivisions FieldDivision = EWorldFieldDivisions::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	EWorldStrategicSupport StrategicSupport = EWorldStrategicSupport::None;

	// Division key or supporting world object's anchor key; intrinsic strength has no source key.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	FGuid SourceKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Campaign|Mission Context|Strength")
	int32 OwningPlayer = INDEX_NONE;
};
