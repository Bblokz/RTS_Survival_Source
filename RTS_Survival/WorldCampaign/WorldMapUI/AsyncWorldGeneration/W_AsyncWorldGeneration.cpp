// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_AsyncWorldGeneration.h"

#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace AsyncWorldGenerationWidgetConstants
{
	constexpr int32 MinLoadingDots = 1;
	constexpr int32 MaxLoadingDots = 5;
	constexpr float LoadingAnimationIntervalSeconds = 1.f;
	constexpr float PercentageToProgressRatio = 0.01f;
	const FString ArmorOpenTag = TEXT("<Armor>");
	const FString ArmorCloseTag = TEXT("</>");
}

void UW_AsyncWorldGeneration::NativeConstruct()
{
	Super::NativeConstruct();
	StartLoadingAnimation();
}

void UW_AsyncWorldGeneration::NativeDestruct()
{
	StopLoadingAnimation();
	Super::NativeDestruct();
}

void UW_AsyncWorldGeneration::SetGenerationProgress(const FText& DescriptionText, const float PercentageValue)
{
	const float ClampedPercentage = FMath::Clamp(PercentageValue, 0.f, 100.f);
	const FString PercentageString = FString::Printf(TEXT("%s%.0f%%%s"),
	                                               *AsyncWorldGenerationWidgetConstants::ArmorOpenTag,
	                                               ClampedPercentage,
	                                               *AsyncWorldGenerationWidgetConstants::ArmorCloseTag);

	if (GetIsValidPercentage())
	{
		Percentage->SetText(FText::FromString(PercentageString));
	}

	if (GetIsValidDescription())
	{
		Description->SetText(DescriptionText);
	}

	if (GetIsValidProgressBar())
	{
		ProgressBar->SetPercent(ClampedPercentage * AsyncWorldGenerationWidgetConstants::PercentageToProgressRatio);
	}
}

void UW_AsyncWorldGeneration::StartLoadingAnimation()
{
	UpdateLoadingText();
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		M_LoadingAnimationTimerHandle,
		this,
		&UW_AsyncWorldGeneration::UpdateLoadingText,
		AsyncWorldGenerationWidgetConstants::LoadingAnimationIntervalSeconds,
		true
	);
}

void UW_AsyncWorldGeneration::StopLoadingAnimation()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_LoadingAnimationTimerHandle);
}

void UW_AsyncWorldGeneration::UpdateLoadingText()
{
	if (GetIsValidLoading())
	{
		Loading->SetText(FText::FromString(
			FString::Printf(TEXT("Loading%s"), *FString::ChrN(M_LoadingDotCount, TEXT('.')))
		));
	}

	M_LoadingDotCount++;
	if (M_LoadingDotCount > AsyncWorldGenerationWidgetConstants::MaxLoadingDots)
	{
		M_LoadingDotCount = AsyncWorldGenerationWidgetConstants::MinLoadingDots;
	}
}

bool UW_AsyncWorldGeneration::GetIsValidPercentage() const
{
	if (IsValid(Percentage))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("Percentage"),
		TEXT("GetIsValidPercentage"),
		this
	);
	return false;
}

bool UW_AsyncWorldGeneration::GetIsValidDescription() const
{
	if (IsValid(Description))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("Description"),
		TEXT("GetIsValidDescription"),
		this
	);
	return false;
}

bool UW_AsyncWorldGeneration::GetIsValidProgressBar() const
{
	if (IsValid(ProgressBar))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("ProgressBar"),
		TEXT("GetIsValidProgressBar"),
		this
	);
	return false;
}

bool UW_AsyncWorldGeneration::GetIsValidLoading() const
{
	if (IsValid(Loading))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("Loading"),
		TEXT("GetIsValidLoading"),
		this
	);
	return false;
}
