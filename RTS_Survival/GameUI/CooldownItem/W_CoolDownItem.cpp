// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CoolDownItem.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

const FName UW_CoolDownItem::S_IconTextureParameterName(TEXT("IconTexture"));
const FName UW_CoolDownItem::S_CooldownStartTimeParameterName(TEXT("CooldownStartTimeSeconds"));
const FName UW_CoolDownItem::S_CooldownDurationParameterName(TEXT("CooldownDurationSeconds"));

bool UW_CoolDownItem::GetIsOnCoolDown() const
{
	if (not bM_IsOnCooldown)
	{
		return false;
	}

	return GetCooldownRemaining() > 0.0f;
}

void UW_CoolDownItem::Init(
	const TWeakObjectPtr<UObject>& WeakWorldContext,
	UTexture2D* const IconTexture,
	const float CooldownSeconds,
	const bool bStartOnCooldown)
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;

	M_DynamicMaterial = nullptr;
	M_WeakWorldContext = WeakWorldContext;
	M_WeakTimerWorld.Reset();

	M_CooldownDurationSeconds = FMath::Max(CooldownSeconds, 0.0f);
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownRemainingSeconds = 0.0f;

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
		return;
	}

	bM_WasInitialized = true;

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

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds, false))
	{
		return M_CooldownRemainingSeconds;
	}

	return CalculateCooldownRemainingSeconds(CurrentWorldTimeSeconds);
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
		EnsureCooldownStateUpdateTimerIsScheduled();
		return;
	}

	M_CooldownDurationSeconds = FMath::Max(CooldownTime, 0.0f);

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		InstantlyResetCooldown();
		return;
	}

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds))
	{
		return;
	}

	M_CooldownStartTimeSeconds = CurrentWorldTimeSeconds;
	M_CooldownRemainingSeconds = M_CooldownDurationSeconds;
	bM_IsOnCooldown = true;

	ApplyCooldownMaterialActiveState();
	ScheduleNextCooldownStateUpdateTimer();
}

void UW_CoolDownItem::InstantlyResetCooldown()
{
	if (not GetWasInitialized())
	{
		return;
	}

	CompleteCooldownInternal();
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
		return;
	}

	if (M_CooldownDurationSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds))
	{
		CompleteCooldownInternal();
		return;
	}

	M_CooldownRemainingSeconds = CalculateCooldownRemainingSeconds(CurrentWorldTimeSeconds);

	if (M_CooldownRemainingSeconds <= S_MinimumCooldownDurationSeconds)
	{
		CompleteCooldownInternal();
		return;
	}

	ApplyCooldownMaterialActiveState();
	ScheduleNextCooldownStateUpdateTimer();
}

void UW_CoolDownItem::NativeDestruct()
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
	M_CooldownRemainingSeconds = 0.0f;

	Super::NativeDestruct();
}

void UW_CoolDownItem::BeginDestroy()
{
	ClearCooldownTimer();

	bM_WasInitialized = false;
	bM_IsOnCooldown = false;
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

void UW_CoolDownItem::CompleteCooldownInternal()
{
	bM_IsOnCooldown = false;
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownRemainingSeconds = 0.0f;

	ClearCooldownTimer();
	ApplyCooldownMaterialCompletedState();
}

void UW_CoolDownItem::HandleCooldownStateUpdateTimerElapsed()
{
	if (not GetWasInitialized(false))
	{
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
	if (GetIsCooldownTimerActive())
	{
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

	float CurrentWorldTimeSeconds = 0.0f;

	if (not TryGetCurrentWorldTimeSeconds(CurrentWorldTimeSeconds, bReportError))
	{
		return false;
	}

	M_CooldownRemainingSeconds = CalculateCooldownRemainingSeconds(CurrentWorldTimeSeconds);

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

float UW_CoolDownItem::CalculateCooldownRemainingSeconds(const float CurrentWorldTimeSeconds) const
{
	const float ElapsedSeconds = FMath::Max(0.0f, CurrentWorldTimeSeconds - M_CooldownStartTimeSeconds);
	return FMath::Max(0.0f, M_CooldownDurationSeconds - ElapsedSeconds);
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
