// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "MissionDebriefTypes.generated.h"

UENUM(BlueprintType)
enum class EMissionDebriefOutcome : uint8
{
	Victory,
	Defeat
};

/**
 * @brief CPU copy of the order sheet's burn field (the paper material's dissolve value before the burn amount is
 * subtracted), so the debrief stage can place flames, embers and smoke on the burning edge.
 * Filled by Source/Tools/MissionDebrief/build_debrief_burn_field.py from the mock-up export; one grid per outcome
 * (defeat spares the stamped letters, victory never burns).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UMissionDebriefBurnField : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * @brief Bilinear sample of the burn field at paper UV (u right, v towards the top edge of the sheet).
	 * @param Outcome Which grid to read.
	 * @param U Paper u in 0..1.
	 * @param V Paper v in 0..1.
	 * @return The field value; the paper is burnt where it is below the current burn amount.
	 */
	float SampleField(const EMissionDebriefOutcome Outcome, const float U, const float V) const;

	bool GetHasGrid(const EMissionDebriefOutcome Outcome) const;

	UPROPERTY(EditAnywhere, Category = "Burn Field")
	int32 GridWidth = 0;

	UPROPERTY(EditAnywhere, Category = "Burn Field")
	int32 GridHeight = 0;

	// Row-major, row 0 at v = 0.
	UPROPERTY(EditAnywhere, Category = "Burn Field")
	TArray<float> VictoryField;

	UPROPERTY(EditAnywhere, Category = "Burn Field")
	TArray<float> DefeatField;

private:
	const TArray<float>& GetField(const EMissionDebriefOutcome Outcome) const;
};
