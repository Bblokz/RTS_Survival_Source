#pragma once

#include "CoreMinimal.h"

#include "FieldConstructionData.generated.h"

enum class EFieldConstructionType : uint8;

USTRUCT(Blueprintable)
struct FFieldConstructionData
{
	GENERATED_BODY()

	// Affects how fast the field construction's preview is rotated.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DeltaDegreeOnPreviewRotation = 10.f;

	// Whether this field construction needs to be within the build radii available to the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNeedsToBeWithinBuildRadii = false;

	EFieldConstructionType FieldConstructionType;
};
