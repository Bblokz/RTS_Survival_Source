// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DigInProgress.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/CanvasPanelSlot.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_DigInProgress::FillProgressBar(
	const float StartProgress,
	const float TotalProgressionTime,
	UObject* WorldContext, const FVector2D& InDrawSize)
{
	if (WorldContext == nullptr || WorldContext->GetWorld() == nullptr)
	{
		StopBar();
		return;
	}
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ProgressBar->Slot))
	{
		CanvasSlot->SetSize(InDrawSize);
	}
	if (TotalProgressionTime <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("TotalProgressionTime must be >0 for W_DigInProgress '%s'."),
			                *GetName())
			+ TEXT("\nPlease ensure that the TotalProgressionTime is set correctly."));
		StopBar();
		return;
	}

	M_WorldContext = WorldContext;
	M_CurrentProgress = FMath::Clamp(StartProgress, 0.0f, 1.0f);
	M_TimeRemaining = TotalProgressionTime;


	//    => increment = (1.0 - StartProgress) * (0.1 / TotalProgressionTime)
	{
		const float RemainingToFull = 1.0f - M_CurrentProgress;
		M_ProgressIncrement = (RemainingToFull > 0.0f)
			                      ? (RemainingToFull * (0.1f / TotalProgressionTime))
			                      : 0.0f;
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(M_CurrentProgress);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("ProgressBar is null or not bound in W_DigInProgress '%s'."), *GetName()));
		RemoveFromParent();
		return;
	}

	UWorld* World = WorldContext->GetWorld();
	World->GetTimerManager().SetTimer(
		M_ProgressTimerHandle,
		this,
		&UW_DigInProgress::UpdateProgress,
		0.1f, // tick interval
		true // loop
	);
}

void UW_DigInProgress::StopProgressBar()
{
	StopBar();
}

void UW_DigInProgress::UpdateProgress()
{
	M_TimeRemaining -= 0.1f;
	M_CurrentProgress += M_ProgressIncrement;

	const bool bTimeUp = (M_TimeRemaining <= 0.0f);
	const bool bReachedFull = (M_CurrentProgress >= 1.0f);
	if (bTimeUp || bReachedFull)
	{
		M_CurrentProgress = 1.0f;
		if (ProgressBar)
		{
			ProgressBar->SetPercent(M_CurrentProgress);
		}

		StopBar();
		return;
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(M_CurrentProgress);
	}
}

bool UW_DigInProgress::EnsureWorldContextIsValid() const
{
	if (M_WorldContext.IsValid() && M_WorldContext->GetWorld() != nullptr)
	{
		return true;
	}

	const FString Name = GetName();
	RTSFunctionLibrary::ReportError(
		FString::Printf(
			TEXT("Invalid or expired WorldContext for W_DigInProgress: '%s'.\n")
			TEXT("Make sure FillProgressBar was called with a valid UObject whose GetWorld() != nullptr."),
			*Name));
	return false;
}

void UW_DigInProgress::StopBar()
{
	const UObject* LocalContext = M_WorldContext.IsValid() ? M_WorldContext.Get() : this;
	if (const UWorld* World = (LocalContext ? LocalContext->GetWorld() : nullptr))
	{
		World->GetTimerManager().ClearTimer(M_ProgressTimerHandle);
	}
	SetVisibility(ESlateVisibility::Hidden);
}

void UW_DigInProgress::BeginDestroy()
{
	Super::BeginDestroy();

	// Clear our timer from whichever world is still valid (either the stored context or our own).
	const UObject* LocalContext = M_WorldContext.IsValid() ? M_WorldContext.Get() : this;
	if (const UWorld* World = (LocalContext ? LocalContext->GetWorld() : nullptr))
	{
		World->GetTimerManager().ClearTimer(M_ProgressTimerHandle);
	}
}
