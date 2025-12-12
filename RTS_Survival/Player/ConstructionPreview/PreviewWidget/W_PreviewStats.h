// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "W_PreviewStats.generated.h"

enum class ERTSRichText : uint8;
class URichTextBlock;
/**
 * Call InitW_PreviewStats to initialize the widget with the pre-compiled text formats.
 */
UCLASS()
class RTS_SURVIVAL_API UW_PreviewStats : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_PreviewStats(FObjectInitializer const& ObjectInitializer);

	void UpdateInformation(
		const float Degrees,
		const float Incline,
		const bool bIsInclineValid,
		bool bShowDistance,
		const float Distance = 0,
		const float MaxDistance = 0,
		const bool bNoValidBuildRadii = false,
		const bool bShowConfirmPlacementOnly = false) const;

	// Initializes the widget with the pre-compiled text formats.
	void InitW_PreviewStats() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Displays the instruction info about placing or cancelling the preview.
	UPROPERTY(meta = (BindWidget))
	URichTextBlock* StatsText;

private:
	bool bM_WasLastUsageOriginBxp = false;
	void SetTextBlockVisbility(UTextBlock* TextBlock, const bool bVisible) const;

	void AddPlacementInstructions(FString& InOutString) const;
	void AddRotationInstructions(FString& InOutString, const int32 RotationDegrees) const;
	void AddInclineInstructions(FString& InOutString, const float Incline, const bool bIsInclineValid) const;
	void AddDistanceInstructions(const bool bNoValidBuildRadii,
		const bool bShowDistance,
		const float Distance,
		const float MaxDistance,
		FString& InOutString) const;

	void AddEndLine(FString& InOutString) const;

	ERTSRichText M_BaseRich;

};
