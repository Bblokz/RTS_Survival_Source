// Copyright Bas Blokzijl - All rights reserved.
// TimeProgressBarWidget.cpp

#include "TimeProgressBarWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTimeProgressBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	M_PauseState.bIsPaused = false;
	M_PauseState.M_TotalPausedDuration = 0.0f;
}

void UTimeProgressBarWidget::ResumeFromPause()
{
	const float CurrentTime = M_World->GetTimeSeconds();
	M_PauseState.M_TotalPausedDuration += CurrentTime - M_PauseState.M_PausedTime;
	M_PauseState.bIsPaused = false;
	if(M_PauseState.bM_WasHiddenByPause)
	{
		SetVisibility(ESlateVisibility::Visible);
		M_PauseState.bM_WasHiddenByPause = false;
	}

	// Restart timers
	M_World->GetTimerManager().SetTimer(M_ProgressUpdateHandle, this,
	                                    &UTimeProgressBarWidget::UpdateProgressBar,
	                                    DeveloperSettings::Optimization::UpdateIntervalProgressBar, true);
}

void UTimeProgressBarWidget::StartNewProgressBar(const float Time)
{
	// Starting new progress bar
	M_TotalTime = Time;
	M_ProgressBar->SetVisibility(ESlateVisibility::Visible);
	M_ProgressBar->SetPercent(0.0f);
	M_ProgressText->SetVisibility(ESlateVisibility::Visible);
	M_StartTime = M_World->GetTimeSeconds();
	M_PauseState.M_TotalPausedDuration = 0.0f;

	M_World->GetTimerManager().SetTimer(M_ProgressUpdateHandle, this,
	                                    &UTimeProgressBarWidget::UpdateProgressBar,
	                                    DeveloperSettings::Optimization::UpdateIntervalProgressBar, true);
}

void UTimeProgressBarWidget::InitTimeProgressComponent(
	UWidgetComponent* WidgetComponentOfBar)
{
	if (M_ProgressText)
	{
		// get font
		auto Font = M_ProgressText->GetFont();
		Font.Size = 30;
		M_ProgressText->SetFont(Font);
	}
	M_ProgressBar->SetVisibility(ESlateVisibility::Hidden);
	M_ProgressBar->SetPercent(0.0f);
	M_ProgressText->SetVisibility(ESlateVisibility::Hidden);
	M_WidgetComponent = WidgetComponentOfBar;
	if (GetIsValidWidgetComp())
	{
		M_WidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	}
}

void UTimeProgressBarWidget::StartProgressBar(const float Time)
{
	M_World = GetWorld();
	if (not M_World)
	{
		return;
	}
	if (M_PauseState.bIsPaused)
	{
		// Resuming from pause
		ResumeFromPause();
	}
	else
	{
		StartNewProgressBar(Time);
	}
}

void UTimeProgressBarWidget::PauseProgressBar(const bool bHideBarWhilePaused)
{
	if (not M_World || M_PauseState.bIsPaused)
	{
		return;
	}
	M_PauseState.bIsPaused = true;
	M_PauseState.M_PausedTime = M_World->GetTimeSeconds();

	M_World->GetTimerManager().ClearTimer(M_ProgressUpdateHandle);
	M_World->GetTimerManager().ClearTimer(M_BarRotationHandle);
	if (bHideBarWhilePaused)
	{
		SetVisibility(ESlateVisibility::Hidden);
		M_PauseState.bM_WasHiddenByPause = true;
	}
}

void UTimeProgressBarWidget::ResetProgressBarKeepVisible()
{
	if (M_World)
	{
		M_World->GetTimerManager().ClearTimer(M_ProgressUpdateHandle);
		M_World->GetTimerManager().ClearTimer(M_BarRotationHandle);
	}
	// Total reset and keep visible.
	M_TotalTime = 0;
	M_ProgressBar->SetVisibility(ESlateVisibility::Visible);
	M_ProgressBar->SetPercent(0.0f);
	M_ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%f%%"), 0.0)));
	M_ProgressText->SetVisibility(ESlateVisibility::Visible);
	M_StartTime = 0;
	M_PauseState.M_TotalPausedDuration = 0.0f;
	M_PauseState.M_PausedTime = 0;
	M_PauseState.bIsPaused = false;
}

void UTimeProgressBarWidget::StopProgressBar()
{
	if (M_World)
	{
		M_World->GetTimerManager().ClearTimer(M_ProgressUpdateHandle);
		M_World->GetTimerManager().ClearTimer(M_BarRotationHandle);
	}
	else
	{
		M_World = GetWorld();
		if (M_World)
		{
			M_World->GetTimerManager().ClearTimer(M_ProgressUpdateHandle);
			M_World->GetTimerManager().ClearTimer(M_BarRotationHandle);
		}
	}
	M_ProgressBar->SetVisibility(ESlateVisibility::Hidden);
	M_ProgressText->SetVisibility(ESlateVisibility::Hidden);
	M_PauseState.bIsPaused = false;
	M_PauseState.M_TotalPausedDuration = 0.0f;
}

float UTimeProgressBarWidget::GetTimeElapsed() const
{
	if (M_World)
	{
		float CurrentTime = M_World->GetTimeSeconds();
		float ElapsedTime = CurrentTime - M_StartTime - M_PauseState.M_TotalPausedDuration;
		return ElapsedTime;
	}
	return 0.0f;
}

void UTimeProgressBarWidget::SetLocalLocation(const FVector& NewLocation) const
{
	M_WidgetComponent->SetRelativeLocation(NewLocation);
}

FVector UTimeProgressBarWidget::GetLocalLocation() const
{
	return M_WidgetComponent->GetRelativeLocation();
}

void UTimeProgressBarWidget::UpdateProgressBar()
{
	if (not M_World)
	{
		return;
	}
	// Calculate elapsed time and progress.
	const float CurrentTime = M_World->GetTimeSeconds();
	const float ElapsedTime = CurrentTime - M_StartTime - M_PauseState.M_TotalPausedDuration;
	const float Progress = FMath::Clamp(ElapsedTime / M_TotalTime, 0.0f, 1.0f);
	M_ProgressBar->SetPercent(Progress);

	// Update the progress text
	const int32 Percentage = FMath::RoundToInt(Progress * 100);
	M_ProgressText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Percentage)));

	if (ElapsedTime >= M_TotalTime)
	{
		StopProgressBar();
	}
}

bool UTimeProgressBarWidget::GetIsValidWidgetComp() const
{
	if (IsValid(M_WidgetComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid or null WidgetComponent in TimeProgressBarWidget!");
	return false;
}
