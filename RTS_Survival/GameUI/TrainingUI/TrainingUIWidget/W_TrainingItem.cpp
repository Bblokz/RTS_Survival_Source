// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingItem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

UW_TrainingItem::UW_TrainingItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ButtonStyleAsset(nullptr)
	, M_TrainingItemSizeBox(nullptr)
	, M_TrainingItemButton(nullptr)
	, M_TrainingUIManager(nullptr)
	, M_Index(0)
	, M_AnimationStartTime(0.f)
	, M_AnimationEndTime(0.f)
	, M_InitialOpacity(0.f)
	, bM_IsClockPaused(false)
	, M_PauseTime(0.f)
{
}

void UW_TrainingItem::InitW_TrainingItem(
	UTrainingMenuManager* TrainingUIManager,
	int32 IndexInScrollBar)
{
	if (not IsValid(TrainingUIManager))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("TrainingUIManager is not valid in InitW_TrainingItem\n")
			TEXT(" at function UW_TrainingItem::InitW_TrainingItem"));
		return;
	}

	M_TrainingUIManager = TrainingUIManager;
	M_Index = IndexInScrollBar;
}

void UW_TrainingItem::UpdateTrainingItem(const FTrainingWidgetState& TrainingItemState)
{
	M_TrainingItemState = TrainingItemState;
	// propagate to blueprint
	OnUpdateTrainingItem(TrainingItemState);
}


void UW_TrainingItem::StartClock(
	const int32 TimeRemaining,
	const int32 TotalTrainingTime,
	const bool bIsPaused)
{
	if (TimeRemaining < 0 || TotalTrainingTime <= 0)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// 1) Establish a fresh timeline for the remaining duration
		M_AnimationStartTime = World->GetTimeSeconds();
		M_AnimationEndTime   = M_AnimationStartTime + TimeRemaining;

		// 2) Compute and store the opacity at the moment we begin
		const float NormalizedProgress = 1.0f - float(TimeRemaining) / float(TotalTrainingTime);
		M_InitialOpacity = M_LowestPossibleTrainingOpacity
			+ NormalizedProgress * (1.0f - M_LowestPossibleTrainingOpacity);

		// // 3) Paint the very first frame via our common curve helper
		if(M_TrainingItemButton)
		{
			M_TrainingItemButton->SetRenderOpacity(ComputeClockOpacity(M_AnimationStartTime));
			
		}

		// 4) Reset any existing timer
		World->GetTimerManager().ClearTimer(M_ClockTimerHandle);

		// 5) Pause or start immediately
		if (bIsPaused)
		{
			bM_IsClockPaused = true;
			M_PauseTime      = M_AnimationStartTime;
		}
		else
		{
			bM_IsClockPaused = false;
			World->GetTimerManager().SetTimer(
				M_ClockTimerHandle,
				this,
				&UW_TrainingItem::UpdateClockOpacity,
				0.1f,
				true
			);
		}
	}
}

void UW_TrainingItem::StopClock()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ClockTimerHandle);

		if (M_TrainingItemButton)
		{
			// Ensure final state is fully visible
			M_TrainingItemButton->SetRenderOpacity(1.0f);
		}
	}
}

void UW_TrainingItem::SetClockPaused(const bool bPause)
{
	if (UWorld* World = GetWorld())
	{
		if (bPause && not bM_IsClockPaused)
		{
			// Pause: kill the timer, remember when
			World->GetTimerManager().ClearTimer(M_ClockTimerHandle);
			bM_IsClockPaused = true;
			M_PauseTime      = World->GetTimeSeconds();
		}
		else if (not bPause && bM_IsClockPaused)
		{
			// Unpause
			ResumeClock();
		}
	}
}

void UW_TrainingItem::ResumeClock()
{
	if (not bM_IsClockPaused || not GetWorld())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();

	// 1) Shift our timeline forward by the amount of the pause
	const float PauseDelta = Now - M_PauseTime;
	M_AnimationStartTime  += PauseDelta;
	M_AnimationEndTime    += PauseDelta;

	// 2) Paint the very first post-pause frame
	if (M_TrainingItemButton)
	{
		M_TrainingItemButton->SetRenderOpacity(ComputeClockOpacity(Now));
	}

	// 3) Restart ticking
	World->GetTimerManager().SetTimer(
		M_ClockTimerHandle,
		this,
		&UW_TrainingItem::UpdateClockOpacity,
		0.1f,
		true
	);

	bM_IsClockPaused = false;
}

void UW_TrainingItem::UpdateClockOpacity()
{
	if (auto* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();

		if (Now >= M_AnimationEndTime)
		{
			StopClock();
		}
		else if (M_TrainingItemButton)
		{
			M_TrainingItemButton->SetRenderOpacity(ComputeClockOpacity(Now));
		}
	}
}

float UW_TrainingItem::ComputeClockOpacity(const float WorldTime) const
{
	// total span
	const float Span = M_AnimationEndTime - M_AnimationStartTime;
	if (Span <= 0.f)
	{
		return 1.f;
	}

	// 1) linear [0..1]
	const float t = FMath::Clamp(
		(WorldTime - M_AnimationStartTime) / Span,
		0.f, 1.f
	);

	// 2) gamma curve to push most visible change to the end
	constexpr float OpacityGamma = 2.0f;
	const float Curved = FMath::Pow(t, OpacityGamma);

	// 3) blend between initial (when we started) and fully visible
	const float StartingOpacity = FMath::Max(M_InitialOpacity, M_LowestPossibleTrainingOpacity);
	return StartingOpacity + Curved * (1.0f - StartingOpacity);
}

void UW_TrainingItem::OnClickedTrainingItem(const bool bIsLeftClick)
{
	if (not GetIsValidTrainingUIManager())
	{
		return;
	}

	M_TrainingUIManager->OnTrainingItemClicked(M_TrainingItemState, M_Index, bIsLeftClick);
}

void UW_TrainingItem::OnHoverTrainingItem(const bool bIsHovering)
{
	if (not GetIsValidTrainingUIManager())
	{
		return;
	}

	M_TrainingUIManager->OnTrainingItemHovered(M_TrainingItemState, bIsHovering);
}

void UW_TrainingItem::UpdateButtonWithGlobalSlateStyle()
{
	if (ButtonStyleAsset)
	{
		const FButtonStyle* ButtonStyle = ButtonStyleAsset->GetStyle<FButtonStyle>();
		if (ButtonStyle && M_TrainingItemButton)
		{
			M_TrainingItemButton->SetStyle(*ButtonStyle);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ButtonStyle null.\n")
			TEXT(" at widget: ") + GetName() +
			TEXT("\n Forgot to set style reference in UW_TrainingItem::UpdateButtonWithGlobalSlateStyle?")
		);
	}
}

bool UW_TrainingItem::GetIsValidTrainingUIManager() const
{
	if (IsValid(M_TrainingUIManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrainingUIManager",
		"GetIsValidTrainingUIManager",
		this
	);

	return false;
}
