// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CoolDownItem.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

const FName UW_CoolDownItem::S_IconTextureParameterName(TEXT("IconTexture"));
const FName UW_CoolDownItem::S_CooldownStartTimeParameterName(TEXT("CooldownStartTimeSeconds"));
const FName UW_CoolDownItem::S_CooldownDurationParameterName(TEXT("CooldownDurationSeconds"));
const FName UW_CoolDownItem::S_PauseAlphaParameterName(TEXT("CooldownPauseAlpha"));
const FName UW_CoolDownItem::S_PauseTimeParameterName(TEXT("CooldownPausedTimeSeconds"));

bool UW_CoolDownItem::GetIsOnCoolDown() const
{
	if (not bM_IsOnCooldown)
	{
		return false;
	}

	return GetCooldownRemaining() > 0.0f;
}

UButton* UW_CoolDownItem::GetButton() const
{
	if (not GetIsValidButton())
	{
		return nullptr;
	}

	return M_Button.Get();
}

void UW_CoolDownItem::Init(
	const TWeakObjectPtr<UObject>& WeakWorldContext,
	UTexture2D* const IconTexture,
	const float CooldownSeconds,
	const bool bStartOnCooldown,
	const bool bStartPaused,
	const float SecondsLeft)
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
	bM_IsClockPaused = false;

	M_DynamicMaterial = nullptr;
	M_WeakWorldContext = WeakWorldContext;
	M_WeakTimerWorld.Reset();

	M_CooldownDurationSeconds = FMath::Max(CooldownSeconds, 0.0f);
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownRemainingSeconds = 0.0f;
	M_CooldownPausedTimeSeconds = 0.0f;

	if (not GetIsValidButton())
	{
		return;
	}

	if (not CacheDynamicMaterialFromImage())
	{
		return;
	}

	ApplyIconTextureParameter(IconTexture);

	if (not CacheTimerWorldFromContext())
	{
		ApplyCooldownMaterialCompletedState();
		ApplyPauseMaterialState();
		return;
	}

	bM_WasInitialized = true;

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds))
	{
		ApplyCooldownMaterialCompletedState();
		ApplyPauseMaterialState();
		return;
	}

	if (bStartPaused)
	{
		bM_IsClockPaused = true;
		M_CooldownPausedTimeSeconds = CurrentWorldTimeSeconds;
	}
	else
	{
		bM_IsClockPaused = false;
		M_CooldownPausedTimeSeconds = 0.0f;
	}

	ApplyPauseMaterialState();

	if (SecondsLeft > 0.0f)
	{
		const float ClampedSecondsLeft = FMath::Clamp(
			SecondsLeft,
			0.0f,
			M_CooldownDurationSeconds);

		if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds
			|| ClampedSecondsLeft <= S_MinimumCooldownDurationSeconds)
		{
			CompleteCooldownInternal();
			return;
		}

		const float EffectiveCooldownTimeSeconds = bM_IsClockPaused
			? M_CooldownPausedTimeSeconds
			: CurrentWorldTimeSeconds;

		M_CooldownRemainingSeconds = ClampedSecondsLeft;

		const float CooldownElapsedSeconds = M_CooldownDurationSeconds - M_CooldownRemainingSeconds;
		M_CooldownStartTimeSeconds = EffectiveCooldownTimeSeconds - CooldownElapsedSeconds;
		bM_IsOnCooldown = true;

		ApplyCooldownMaterialActiveState();
		ApplyPauseMaterialState();
		RefreshCooldownTimerForCurrentPauseState();
		return;
	}

	if (bStartOnCooldown)
	{
		StartCooldown(M_CooldownDurationSeconds);
		return;
	}

	InstantlyResetCooldown();
}



float UW_CoolDownItem::GetCooldownRemaining() const
{
	if (not bM_IsOnCooldown)
	{
		return 0.0f;
	}

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		return 0.0f;
	}

	float EffectiveCooldownTimeSeconds = 0.0f;

	if (not TryGetEffectiveCooldownTimeSeconds(EffectiveCooldownTimeSeconds, false))
	{
		return M_CooldownRemainingSeconds;
	}

	return CalculateCooldownRemainingSeconds(EffectiveCooldownTimeSeconds);
}

void UW_CoolDownItem::StartCooldown(const float CooldownTime)
{
	if (not GetWasInitialized())
	{
		return;
	}

	if (not UpdateCooldownStateFromCurrentTime())
	{
		return;
	}

	if (bM_IsOnCooldown)
	{
		RefreshCooldownTimerForCurrentPauseState();
		return;
	}

	M_CooldownDurationSeconds = FMath::Max(CooldownTime, 0.0f);

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		InstantlyResetCooldown();
		return;
	}

	float EffectiveCooldownTimeSeconds = 0.0f;

	if (not TryGetEffectiveCooldownTimeSeconds(EffectiveCooldownTimeSeconds))
	{
		return;
	}

	M_CooldownStartTimeSeconds = EffectiveCooldownTimeSeconds;
	M_CooldownRemainingSeconds = M_CooldownDurationSeconds;
	bM_IsOnCooldown = true;

	ApplyCooldownMaterialActiveState();
	ApplyPauseMaterialState();
	RefreshCooldownTimerForCurrentPauseState();
}

void UW_CoolDownItem::SetCooldownState(
	const float CooldownDurationSeconds,
	const float CooldownRemainingSeconds)
{
	if (not GetWasInitialized())
	{
		return;
	}

	const float SanitizedCooldownRemainingSeconds = FMath::Max(CooldownRemainingSeconds, 0.0f);

	M_CooldownDurationSeconds = FMath::Max(
		FMath::Max(CooldownDurationSeconds, 0.0f),
		SanitizedCooldownRemainingSeconds);

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds
		|| SanitizedCooldownRemainingSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	float EffectiveCooldownTimeSeconds = 0.0f;

	if (not TryGetEffectiveCooldownTimeSeconds(EffectiveCooldownTimeSeconds))
	{
		CompleteCooldownInternal();
		return;
	}

	M_CooldownRemainingSeconds = FMath::Min(
		SanitizedCooldownRemainingSeconds,
		M_CooldownDurationSeconds);

	const float CooldownElapsedSeconds = M_CooldownDurationSeconds - M_CooldownRemainingSeconds;
	M_CooldownStartTimeSeconds = EffectiveCooldownTimeSeconds - CooldownElapsedSeconds;
	bM_IsOnCooldown = true;

	ApplyCooldownMaterialActiveState();
	ApplyPauseMaterialState();
	RefreshCooldownTimerForCurrentPauseState();
}

void UW_CoolDownItem::InstantlyResetCooldown()
{
	if (not GetWasInitialized())
	{
		return;
	}

	CompleteCooldownInternal();
}

void UW_CoolDownItem::ClearCooldownMaterial()
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
	bM_IsClockPaused = false;

	M_WeakWorldContext.Reset();
	M_WeakTimerWorld.Reset();

	M_CooldownDurationSeconds = 0.0f;
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownRemainingSeconds = 0.0f;
	M_CooldownPausedTimeSeconds = 0.0f;

	if (not CacheDynamicMaterialFromImage())
	{
		return;
	}

	M_DynamicMaterial->SetTextureParameterValue(S_IconTextureParameterName, nullptr);
	ApplyCooldownMaterialCompletedState();
	ApplyPauseMaterialState();
}

void UW_CoolDownItem::PauseClock(const bool bPause)
{
	if (not GetWasInitialized())
	{
		return;
	}

	if (bPause)
	{
		StartClockPauseInternal();
		return;
	}

	StopClockPauseInternal();
}

void UW_CoolDownItem::UpgradeCooldown(const float NewCooldown, const bool bResetCooldownState)
{
	if (not GetWasInitialized())
	{
		return;
	}

	M_CooldownDurationSeconds = FMath::Max(NewCooldown, 0.0f);

	if (bResetCooldownState)
	{
		InstantlyResetCooldown();
		return;
	}

	if (not bM_IsOnCooldown)
	{
		M_CooldownRemainingSeconds = 0.0f;
		ApplyCooldownMaterialCompletedState();
		ApplyPauseMaterialState();
		return;
	}

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	float EffectiveCooldownTimeSeconds = 0.0f;

	if (not TryGetEffectiveCooldownTimeSeconds(EffectiveCooldownTimeSeconds))
	{
		CompleteCooldownInternal();
		return;
	}

	M_CooldownRemainingSeconds = CalculateCooldownRemainingSeconds(EffectiveCooldownTimeSeconds);

	if (M_CooldownRemainingSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	ApplyCooldownMaterialActiveState();
	ApplyPauseMaterialState();
	RefreshCooldownTimerForCurrentPauseState();
}

void UW_CoolDownItem::NativeDestruct()
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
	bM_IsClockPaused = false;
	M_CooldownRemainingSeconds = 0.0f;
	M_CooldownPausedTimeSeconds = 0.0f;

	Super::NativeDestruct();
}

void UW_CoolDownItem::BeginDestroy()
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
	bM_IsClockPaused = false;

	M_DynamicMaterial = nullptr;
	M_WeakWorldContext.Reset();
	M_WeakTimerWorld.Reset();

	Super::BeginDestroy();
}

bool UW_CoolDownItem::CacheDynamicMaterialFromImage()
{
	if (not GetIsValidImage())
	{
		return false;
	}

	M_DynamicMaterial = M_Image->GetDynamicMaterial();

	if (not GetIsValidDynamicMaterial())
	{
		return false;
	}

	return true;
}

bool UW_CoolDownItem::CacheTimerWorldFromContext()
{
	M_WeakTimerWorld.Reset();

	UWorld* const TimerWorld = GetTimerWorldFromContext(true);

	if (not IsValid(TimerWorld))
	{
		return false;
	}

	M_WeakTimerWorld = TimerWorld;
	return true;
}

void UW_CoolDownItem::ApplyIconTextureParameter(UTexture2D* const IconTexture) const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	if (not IsValid(IconTexture))
	{
		ReportInvalidState(TEXT("IconTexture is invalid."));
		return;
	}

	M_DynamicMaterial->SetTextureParameterValue(S_IconTextureParameterName, IconTexture);
}

void UW_CoolDownItem::ApplyCooldownMaterialActiveState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	M_DynamicMaterial->SetScalarParameterValue(
		S_CooldownStartTimeParameterName,
		M_CooldownStartTimeSeconds);

	M_DynamicMaterial->SetScalarParameterValue(
		S_CooldownDurationParameterName,
		M_CooldownDurationSeconds);
}

void UW_CoolDownItem::ApplyCooldownMaterialCompletedState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	M_DynamicMaterial->SetScalarParameterValue(S_CooldownStartTimeParameterName, 0.0f);
	M_DynamicMaterial->SetScalarParameterValue(S_CooldownDurationParameterName, 0.0f);
}

void UW_CoolDownItem::ApplyPauseMaterialState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	const float PauseAlpha = bM_IsClockPaused ? 1.0f : 0.0f;
	const float PauseTimeSeconds = bM_IsClockPaused ? M_CooldownPausedTimeSeconds : 0.0f;

	M_DynamicMaterial->SetScalarParameterValue(S_PauseTimeParameterName, PauseTimeSeconds);
	M_DynamicMaterial->SetScalarParameterValue(S_PauseAlphaParameterName, PauseAlpha);
}

void UW_CoolDownItem::CompleteCooldownInternal()
{
	bM_IsOnCooldown = false;
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownRemainingSeconds = 0.0f;

	ClearCooldownTimer();
	ApplyCooldownMaterialCompletedState();
	ApplyPauseMaterialState();
}

void UW_CoolDownItem::StartClockPauseInternal()
{
	if (bM_IsClockPaused)
	{
		ClearCooldownTimer();
		ApplyPauseMaterialState();
		return;
	}

	if (not UpdateCooldownStateFromCurrentTime())
	{
		CompleteCooldownInternal();
		return;
	}

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds))
	{
		return;
	}

	M_CooldownPausedTimeSeconds = CurrentWorldTimeSeconds;
	bM_IsClockPaused = true;

	ClearCooldownTimer();
	ApplyPauseMaterialState();
}

void UW_CoolDownItem::StopClockPauseInternal()
{
	if (not bM_IsClockPaused)
	{
		ApplyPauseMaterialState();
		RefreshCooldownTimerForCurrentPauseState();
		return;
	}

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds))
	{
		return;
	}

	const float PauseDurationSeconds = CalculatePauseDurationSeconds(CurrentWorldTimeSeconds);

	if (bM_IsOnCooldown)
	{
		M_CooldownStartTimeSeconds += PauseDurationSeconds;
	}

	bM_IsClockPaused = false;
	M_CooldownPausedTimeSeconds = 0.0f;

	ApplyPauseMaterialState();

	if (not bM_IsOnCooldown)
	{
		M_CooldownRemainingSeconds = 0.0f;
		return;
	}

	ApplyCooldownMaterialActiveState();

	if (not UpdateCooldownStateFromCurrentTime())
	{
		CompleteCooldownInternal();
		return;
	}

	RefreshCooldownTimerForCurrentPauseState();
}

void UW_CoolDownItem::HandleCooldownStateUpdateTimerElapsed()
{
	if (not GetWasInitialized(false))
	{
		return;
	}

	if (bM_IsClockPaused)
	{
		ClearCooldownTimer();
		return;
	}

	if (not UpdateCooldownStateFromCurrentTime(false))
	{
		CompleteCooldownInternal();
		return;
	}

	if (not bM_IsOnCooldown)
	{
		return;
	}

	ScheduleNextCooldownStateUpdateTimer();
}

void UW_CoolDownItem::EnsureCooldownStateUpdateTimerIsScheduled()
{
	if (bM_IsClockPaused)
	{
		ClearCooldownTimer();
		return;
	}

	if (GetIsCooldownTimerActive())
	{
		return;
	}

	ScheduleNextCooldownStateUpdateTimer();
}

void UW_CoolDownItem::RefreshCooldownTimerForCurrentPauseState()
{
	if (not bM_IsOnCooldown)
	{
		ClearCooldownTimer();
		return;
	}

	if (bM_IsClockPaused)
	{
		ClearCooldownTimer();
		return;
	}

	ScheduleNextCooldownStateUpdateTimer();
}

void UW_CoolDownItem::ScheduleNextCooldownStateUpdateTimer()
{
	ClearCooldownTimer();

	if (not bM_IsOnCooldown)
	{
		return;
	}

	if (bM_IsClockPaused)
	{
		return;
	}

	if (M_CooldownRemainingSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	UWorld* const TimerWorld = GetTimerWorld(true);

	if (not IsValid(TimerWorld))
	{
		CompleteCooldownInternal();
		return;
	}

	const TWeakObjectPtr<UW_CoolDownItem> WeakThis(this);

	FTimerDelegate CooldownStateUpdateDelegate;
	CooldownStateUpdateDelegate.BindLambda(
		[WeakThis]()
		{
			UW_CoolDownItem* const CooldownItem = WeakThis.Get();

			if (not IsValid(CooldownItem))
			{
				return;
			}

			CooldownItem->HandleCooldownStateUpdateTimerElapsed();
		});

	TimerWorld->GetTimerManager().SetTimer(
		M_CooldownTimerHandle,
		CooldownStateUpdateDelegate,
		GetNextCooldownStateUpdateDelaySeconds(),
		false);
}

void UW_CoolDownItem::ClearCooldownTimer()
{
	if (not M_CooldownTimerHandle.IsValid())
	{
		return;
	}

	UWorld* const TimerWorld = GetTimerWorld(false);

	if (IsValid(TimerWorld))
	{
		TimerWorld->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	}

	M_CooldownTimerHandle.Invalidate();
}

bool UW_CoolDownItem::UpdateCooldownStateFromCurrentTime(const bool bReportError)
{
	if (not bM_IsOnCooldown)
	{
		M_CooldownRemainingSeconds = 0.0f;
		return true;
	}

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return true;
	}

	float EffectiveCooldownTimeSeconds = 0.0f;

	if (not TryGetEffectiveCooldownTimeSeconds(EffectiveCooldownTimeSeconds, bReportError))
	{
		return false;
	}

	M_CooldownRemainingSeconds = CalculateCooldownRemainingSeconds(EffectiveCooldownTimeSeconds);

	if (M_CooldownRemainingSeconds > S_MinimumCooldownDurationSeconds)
	{
		return true;
	}

	CompleteCooldownInternal();
	return true;
}

bool UW_CoolDownItem::TryGetCurrentWorldTimeSeconds(
	float& OutCurrentWorldTimeSeconds,
	const bool bReportError) const
{
	OutCurrentWorldTimeSeconds = 0.0f;

	UWorld* const TimerWorld = GetTimerWorld(bReportError);

	if (not IsValid(TimerWorld))
	{
		return false;
	}

	OutCurrentWorldTimeSeconds = TimerWorld->GetTimeSeconds();
	return true;
}

bool UW_CoolDownItem::TryGetEffectiveCooldownTimeSeconds(
	float& OutEffectiveCooldownTimeSeconds,
	const bool bReportError) const
{
	if (bM_IsClockPaused)
	{
		OutEffectiveCooldownTimeSeconds = M_CooldownPausedTimeSeconds;
		return true;
	}

	return TryGetCurrentWorldTimeSeconds(OutEffectiveCooldownTimeSeconds, bReportError);
}

float UW_CoolDownItem::CalculateCooldownRemainingSeconds(const float CurrentWorldTimeSeconds) const
{
	const float ElapsedSeconds = FMath::Max(0.0f, CurrentWorldTimeSeconds - M_CooldownStartTimeSeconds);
	return FMath::Max(0.0f, M_CooldownDurationSeconds - ElapsedSeconds);
}

float UW_CoolDownItem::CalculatePauseDurationSeconds(const float CurrentWorldTimeSeconds) const
{
	return FMath::Max(0.0f, CurrentWorldTimeSeconds - M_CooldownPausedTimeSeconds);
}

float UW_CoolDownItem::GetNextCooldownStateUpdateDelaySeconds() const
{
	const float ClampedDelaySeconds = FMath::Min(
		M_CooldownRemainingSeconds,
		S_CooldownStateUpdateIntervalSeconds);

	return FMath::Max(ClampedDelaySeconds, S_MinimumTimerDelaySeconds);
}

bool UW_CoolDownItem::GetIsCooldownTimerActive() const
{
	if (not M_CooldownTimerHandle.IsValid())
	{
		return false;
	}

	UWorld* const TimerWorld = GetTimerWorld(false);

	if (not IsValid(TimerWorld))
	{
		return false;
	}

	return TimerWorld->GetTimerManager().IsTimerActive(M_CooldownTimerHandle);
}

UWorld* UW_CoolDownItem::GetTimerWorld(const bool bReportError) const
{
	if (GetIsValidTimerWorld(false))
	{
		return M_WeakTimerWorld.Get();
	}

	return GetTimerWorldFromContext(bReportError);
}

UWorld* UW_CoolDownItem::GetTimerWorldFromContext(const bool bReportError) const
{
	if (not GetIsValidWorldContext(bReportError))
	{
		return nullptr;
	}

	UObject* const WorldContextObject = M_WeakWorldContext.Get();
	UWorld* const TimerWorld = WorldContextObject->GetWorld();

	if (IsValid(TimerWorld))
	{
		return TimerWorld;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("M_WeakWorldContext does not provide a valid UWorld."));
	}

	return nullptr;
}

bool UW_CoolDownItem::GetWasInitialized(const bool bReportError) const
{
	if (bM_WasInitialized)
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("The widget was used before Init completed successfully."));
	}

	return false;
}

bool UW_CoolDownItem::GetIsValidButton() const
{
	if (IsValid(M_Button.Get()))
	{
		return true;
	}

	ReportInvalidState(TEXT("M_Button is invalid. Check the BindWidget name in the widget blueprint."));
	return false;
}

bool UW_CoolDownItem::GetIsValidImage() const
{
	if (IsValid(M_Image.Get()))
	{
		return true;
	}

	ReportInvalidState(TEXT("M_Image is invalid. Check the BindWidget name in the widget blueprint."));
	return false;
}

bool UW_CoolDownItem::GetIsValidDynamicMaterial() const
{
	if (IsValid(M_DynamicMaterial.Get()))
	{
		return true;
	}

	ReportInvalidState(TEXT("M_DynamicMaterial is invalid. Set the cooldown UI material on M_Image in the widget defaults."));
	return false;
}

bool UW_CoolDownItem::GetIsValidWorldContext(const bool bReportError) const
{
	if (M_WeakWorldContext.IsValid())
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("M_WeakWorldContext is invalid."));
	}

	return false;
}

bool UW_CoolDownItem::GetIsValidTimerWorld(const bool bReportError) const
{
	if (M_WeakTimerWorld.IsValid())
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("M_WeakTimerWorld is invalid."));
	}

	return false;
}

void UW_CoolDownItem::ReportInvalidState(const FString& ErrorMessage) const
{
	const FString FullErrorMessage = FString::Printf(
		TEXT("UW_CoolDownItem '%s': %s"),
		*GetNameSafe(this),
		*ErrorMessage);

	RTSFunctionLibrary::ReportError(FullErrorMessage);
}
