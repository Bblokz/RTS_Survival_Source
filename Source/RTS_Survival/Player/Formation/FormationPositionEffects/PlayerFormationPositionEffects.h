#pragma once

#include "CoreMinimal.h"

#include "PlayerFormationPositionEffects.generated.h"


class AFormationEffectActor;
class UW_FormationPicker;

USTRUCT(BlueprintType)
struct FPlayerFormationPositionEffects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AFormationEffectActor> FormationPositionEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PositionEffectsDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PositionEffectsScale = FVector(1.f, 1.f, 1.f);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PositionEffectsOffset = FVector(0.f, 0.f, 200.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FormationWidget")
	TSubclassOf<UW_FormationPicker> FormationPickerWidgetClass;
};

UENUM(BlueprintType)
enum class EFormationEffect : uint8
{
	None,
	Arrow,
	ArrowSpearFormation,
	ArrowThinSpearFormation,
	ArrowCircleFormation,
	PositionMarker
};
