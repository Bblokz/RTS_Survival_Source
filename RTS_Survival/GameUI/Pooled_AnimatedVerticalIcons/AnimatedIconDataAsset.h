#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AnimatedIconTypes.h"
#include "AnimatedIconDataAsset.generated.h"

class UTexture2D;
class UW_RTSVerticalAnimatedIcon;

/** @brief Keeps artwork and its presentation defaults together for each gameplay icon. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSVerticalAnimatedIconDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animated Icons")
	TSoftObjectPtr<UTexture2D> Texture;

	// Slate units, independent of source texture resolution; artwork is fitted without stretching.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animated Icons", meta=(ClampMin="1.0"))
	FVector2D DisplaySize = FVector2D(48.0, 48.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animated Icons")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animated Icons")
	FRTSVerticalAnimIconSettings DefaultAnimationSettings;
};

/** @brief Create one asset in the Content Browser and assign it in Project Settings > Game > Animated Icons. */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UAnimatedIconDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UAnimatedIconDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool", meta=(ClampMin="1", UIMin="1"))
	int32 PoolSize = 32;

	// The native class builds the layout itself; a Blueprint subclass can supply a custom layout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pool")
	TSoftClassPtr<UW_RTSVerticalAnimatedIcon> WidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Icons", meta=(ForceInlineRow))
	TMap<ERTSVerticalAnimatedIcon, FRTSVerticalAnimatedIconDefinition> IconDefinitions;
};
