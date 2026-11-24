// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "MegaphoneLineSet.generated.h"

class USoundBase;

/** One broadcast: the spoken wave and its English meaning (the lines are spoken in Russian). */
USTRUCT(BlueprintType)
struct FMegaphoneLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	TObjectPtr<USoundBase> Sound = nullptr;

	/** What the line says in English; kept for subtitles and for designers reading the set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone", meta = (MultiLine = true))
	FText EnglishText;
};

/**
 * @brief A pool of megaphone broadcasts that UMegaphoneBroadcastComponent picks from at random.
 * Make one set per theme (generic Directorate notices, a level-specific notice, ...) and give a placed megaphone one
 * or more sets; SelectionWeight makes the lines of one set come up more often than those of the others.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UMegaphoneLineSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone")
	TArray<FMegaphoneLine> Lines;

	/** Relative chance of each line in this set against the lines of the other sets on the same megaphone
	 * (1 = as likely as a line from another weight-1 set, 3 = three times as likely). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Megaphone", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
