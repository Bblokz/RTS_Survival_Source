// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_PreviewStats.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

UW_PreviewStats::UW_PreviewStats(FObjectInitializer const& ObjectInitializer)
	: UUserWidget(ObjectInitializer), StatsText(nullptr)
{
	M_BaseRich = ERTSRichText::Text_Armor;
}

void UW_PreviewStats::UpdateInformation(
	const float Degrees,
	const float Incline,
	const bool bIsInclineValid,
	const bool bShowDistance,
	const float Distance,
	const float MaxDistance,
	const bool bNoValidBuildRadii,
	const bool bShowConfirmPlacementOnly) const
{
	FString Description = "";
	AddPlacementInstructions(Description);
	if (bShowConfirmPlacementOnly)
	{
		StatsText->SetText(FText::FromString(Description));
		return;
	}
	AddRotationInstructions(Description, FMath::RoundToInt(Degrees));
	AddInclineInstructions(Description, Incline, bIsInclineValid);
	AddDistanceInstructions(bNoValidBuildRadii, bShowDistance, Distance, MaxDistance, Description);
	StatsText->SetText(FText::FromString(Description));
}


void UW_PreviewStats::InitW_PreviewStats() const
{
	if (IsValid(StatsText))
	{
		StatsText->SetText(FText::FromString(""));
	}
}

void UW_PreviewStats::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}


void UW_PreviewStats::SetTextBlockVisbility(UTextBlock* TextBlock, const bool bVisible) const
{
	if (not TextBlock)
	{
		return;
	}
	TextBlock->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_PreviewStats::AddPlacementInstructions(FString& InOutString) const
{
	const FString PrimaryClick = FRTSRichTextConverter::MakeRTSRich("LMB", M_BaseRich);
	const FString SecondaryClick = FRTSRichTextConverter::MakeRTSRich("RMB", M_BaseRich);
	InOutString += PrimaryClick + FRTSRichTextConverter::MakeRTSRich(" to place or ", M_BaseRich) +
		SecondaryClick + FRTSRichTextConverter::MakeRTSRich(" to cancel", M_BaseRich);
}

void UW_PreviewStats::AddRotationInstructions(FString& InOutString, const int32 RotationDegrees) const
{
	AddEndLine(InOutString);
	const FString LeftRotate = FRTSRichTextConverter::MakeRTSRich("'<'", M_BaseRich);
	const FString RightRotate = FRTSRichTextConverter::MakeRTSRich("'>'", M_BaseRich);
	const FString RotationDegStr = FString::FromInt(RotationDegrees);
	const FString Degrees = FRTSRichTextConverter::MakeRTSRich(": ", M_BaseRich) + FRTSRichTextConverter::MakeRTSRich(
		RotationDegStr, ERTSRichText::Text_DescriptionHelper) + FRTSRichTextConverter::MakeRTSRich(
		" degrees", M_BaseRich);
	InOutString += FRTSRichTextConverter::MakeRTSRich("Use ", M_BaseRich) + LeftRotate +
		FRTSRichTextConverter::MakeRTSRich(" and ", M_BaseRich) +
		RightRotate + Degrees;
}

void UW_PreviewStats::AddInclineInstructions(FString& InOutString, const float Incline,
                                             const bool bIsInclineValid) const
{
	const FString InclineStr = FString::SanitizeFloat(Incline, 1);
	AddEndLine(InOutString);
	if (not bIsInclineValid)
	{
		InOutString += FRTSRichTextConverter::MakeRTSRich("Incline too steep", ERTSRichText::Text_Error);
		return;
	}
	InOutString += FRTSRichTextConverter::MakeRTSRich("Incline: ", M_BaseRich) + FRTSRichTextConverter::MakeRTSRich(
		InclineStr, ERTSRichText::Text_DescriptionHelper);
}

void UW_PreviewStats::AddDistanceInstructions(const bool bNoValidBuildRadii, const bool bShowDistance,
                                              const float Distance, const float MaxDistance, FString& InOutString) const
{
	if (bNoValidBuildRadii)
	{
		AddEndLine(InOutString);
		InOutString += FRTSRichTextConverter::MakeRTSRich("Build the HQ to obtain Build Radius",
		                                                  ERTSRichText::Text_Error);
		return;
	}
	if (not bShowDistance)
	{
		return;
	}
	AddEndLine(InOutString);

	if (Distance > MaxDistance)
	{
		InOutString += FRTSRichTextConverter::MakeRTSRich("Out of Build Radius", ERTSRichText::Text_Error);
		return;
	}
	const FString DistanceSantized = FString::SanitizeFloat(Distance, 1);
	const FString DistanceStr = FRTSRichTextConverter::MakeRTSRich(DistanceSantized,
	                                                               ERTSRichText::Text_DescriptionHelper);
	const FString MaxDistanceStr = FRTSRichTextConverter::MakeRTSRich(FString::SanitizeFloat(MaxDistance, 1),
	                                                                  ERTSRichText::Text_DescriptionHelper);
	InOutString += FRTSRichTextConverter::MakeRTSRich("Distance: ", M_BaseRich) + DistanceStr +
		FRTSRichTextConverter::MakeRTSRich(" / ", M_BaseRich) + MaxDistanceStr;
}

void UW_PreviewStats::AddEndLine(FString& InOutString) const
{
	InOutString += "\n";
}
