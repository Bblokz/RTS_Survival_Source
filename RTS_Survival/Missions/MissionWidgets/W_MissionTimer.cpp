#include "W_MissionTimer.h"

#include "Components/RichTextBlock.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionTimerWidget
{
	constexpr float TickIntervalSeconds = 1.0f;
	constexpr float MinimumVisibleTime = 0.0f;
	constexpr int32 SecondsPerMinute = 60;
}

void UW_MissionTimer::InitMissionTimer(const FText& TimerText,
                                       float TimerInSeconds,
                                       const FMissionTimerLifetimeSettings& LifetimeSettings)
{
	if (not GetIsValidTimerText() || not GetIsValidClockText())
	{
		return;
	}

	M_MissionTimerLifetimeSettings = LifetimeSettings;
	M_NextLifetimeEntryIndex = 0;
	M_RemainingSeconds = FMath::Max(MissionTimerWidget::MinimumVisibleTime, TimerInSeconds);

	SetTimerText(TimerText);
	UpdateClockTextFromRemainingSeconds();
	StartCountdownTimer();
}

void UW_MissionTimer::NativeDestruct()
{
	DestroyWidgetAndStopTimer();
	Super::NativeDestruct();
}

void UW_MissionTimer::OnMissionTimerTick()
{
	M_RemainingSeconds = FMath::Max(MissionTimerWidget::MinimumVisibleTime,
	                                M_RemainingSeconds - MissionTimerWidget::TickIntervalSeconds);
	UpdateClockTextFromRemainingSeconds();

	if (M_RemainingSeconds > MissionTimerWidget::MinimumVisibleTime)
	{
		return;
	}

	HandleTimerExpired();
}

void UW_MissionTimer::HandleTimerExpired()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CountdownTimerHandle);
	}

	if (not M_MissionTimerLifetimeSettings.M_TimerLifetimes.IsValidIndex(M_NextLifetimeEntryIndex))
	{
		DestroyWidgetAndStopTimer();
		RemoveFromParent();
		return;
	}

	const FTimerLifeTime& NextTimerLifeTime = M_MissionTimerLifetimeSettings.M_TimerLifetimes[M_NextLifetimeEntryIndex];
	++M_NextLifetimeEntryIndex;

	SetTimerText(NextTimerLifeTime.M_TimerText);
	M_RemainingSeconds = FMath::Max(MissionTimerWidget::MinimumVisibleTime, NextTimerLifeTime.M_Time);
	UpdateClockTextFromRemainingSeconds();
	StartCountdownTimer();
}

void UW_MissionTimer::SetTimerText(const FText& TimerText) const
{
	if (not GetIsValidTimerText())
	{
		return;
	}

	M_TimerText->SetText(TimerText);
}

void UW_MissionTimer::UpdateClockTextFromRemainingSeconds() const
{
	if (not GetIsValidClockText())
	{
		return;
	}

	M_ClockText->SetText(FText::FromString(GetFormattedClockText()));
}

FString UW_MissionTimer::GetFormattedClockText() const
{
	const int32 RemainingTotalSeconds = FMath::Max(0, FMath::CeilToInt(M_RemainingSeconds));
	const int32 RemainingMinutes = RemainingTotalSeconds / MissionTimerWidget::SecondsPerMinute;
	const int32 RemainingSeconds = RemainingTotalSeconds % MissionTimerWidget::SecondsPerMinute;

	if (RemainingMinutes > 0)
	{
		return FString::Printf(TEXT("<Text_Armor>%d:%d</>"), RemainingMinutes, RemainingSeconds);
	}

	if (RemainingSeconds < 10)
	{
		return FString::Printf(TEXT("<Text_Armor>0%d</>"), RemainingSeconds);
	}

	return FString::Printf(TEXT("<Text_Armor>%d</>"), RemainingSeconds);
}

void UW_MissionTimer::StartCountdownTimer()
{
	if (not GetWorld())
	{
		RTSFunctionLibrary::ReportError("Mission timer widget could not start countdown: invalid world.");
		return;
	}

	if (M_RemainingSeconds <= MissionTimerWidget::MinimumVisibleTime)
	{
		HandleTimerExpired();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		M_CountdownTimerHandle,
		this,
		&UW_MissionTimer::OnMissionTimerTick,
		MissionTimerWidget::TickIntervalSeconds,
		true);
}

void UW_MissionTimer::DestroyWidgetAndStopTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CountdownTimerHandle);
	}
}

bool UW_MissionTimer::GetIsValidTimerText() const
{
	if (IsValid(M_TimerText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission timer widget has invalid M_TimerText on: " + GetName());
	return false;
}

bool UW_MissionTimer::GetIsValidClockText() const
{
	if (IsValid(M_ClockText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission timer widget has invalid M_ClockText on: " + GetName());
	return false;
}
