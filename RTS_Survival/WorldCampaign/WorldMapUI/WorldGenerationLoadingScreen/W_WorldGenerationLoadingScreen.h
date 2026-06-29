// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_WorldGenerationLoadingScreen.generated.h"

class UProgressBar;
class URichTextBlock;

/**
 * @brief Loading overlay used while campaign generation finishes its virtual placement pass.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldGenerationLoadingScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGenerationProgress(float ProgressPercent, const FString& DescriptionText);

protected:
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UProgressBar> Progressbar = nullptr;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> StepDescription = nullptr;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> Percentage = nullptr;
};
