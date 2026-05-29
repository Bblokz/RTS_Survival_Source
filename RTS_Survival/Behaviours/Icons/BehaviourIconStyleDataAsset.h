// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#include "BehaviourIconStyleDataAsset.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FBehaviourIconWidgetStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour Button")
	TObjectPtr<UTexture2D> IconTexture = nullptr;
};

/**
 * @brief Create this asset in the Content Browser so behaviour widgets share one cooked icon style source.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UBehaviourIconStyleDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour Button Styles", meta=(ForceInlineRow))
	TMap<EBehaviourIcon, FBehaviourIconWidgetStyle> BehaviourIconStyles;

	const FBehaviourIconWidgetStyle* FindBehaviourIconStyle(const EBehaviourIcon BehaviourIcon) const;
};
