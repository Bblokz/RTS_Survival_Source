// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_AsyncWorldGeneration.generated.h"

class UProgressBar;
class URichTextBlock;

/**
 * @brief Used by the world player controller while a new campaign is generated.
 * It owns the animated loading text and exposes progress updates to the generator flow.
 */
UCLASS()
class RTS_SURVIVAL_API UW_AsyncWorldGeneration : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetGenerationProgress(const FText& DescriptionText, float PercentageValue);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> Percentage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<URichTextBlock> Loading;

private:
	void StartLoadingAnimation();
	void StopLoadingAnimation();
	void UpdateLoadingText();
	bool GetIsValidPercentage() const;
	bool GetIsValidDescription() const;
	bool GetIsValidProgressBar() const;
	bool GetIsValidLoading() const;

	FTimerHandle M_LoadingAnimationTimerHandle;
	int32 M_LoadingDotCount = 1;
};
