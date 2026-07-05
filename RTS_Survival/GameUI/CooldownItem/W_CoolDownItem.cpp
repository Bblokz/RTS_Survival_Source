// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CoolDownItem.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace CoolDownItem
{
	static constexpr float MinimumCooldownDurationSeconds = 0.0001f;
	static constexpr float MinimumClockEdgeSoftness01 = 0.00001f;
	static constexpr float MinimumAspectRatio = 0.0001f;

	static const FName IconTextureParameterName(TEXT("IconTexture"));
	static const FName CooldownStartTimeParameterName(TEXT("CooldownStartTimeSeconds"));
	static const FName CooldownDurationParameterName(TEXT("CooldownDurationSeconds"));
	static const FName CooldownClockColorParameterName(TEXT("CooldownClockColor"));
	static const FName ClockEdgeSoftnessParameterName(TEXT("ClockEdgeSoftness01"));
	static const FName ClockStartOffsetParameterName(TEXT("ClockStartOffset01"));
	static const FName AspectRatioParameterName(TEXT("AspectRatio"));

	float Normalize01(const float Value)
	{
		return Value - FMath::FloorToFloat(Value);
	}
}

bool UW_CoolDownItem::GetIsOnCoolDown() const
{
	return bM_IsOnCooldown;
}

void UW_CoolDownItem::Init(
	UMaterialInterface* const CooldownMaterial,
	UTexture2D* const IconTexture,
	UMaterialParameterCollection* const CurrentTimeParameterCollection,
	const FName& CurrentTimeParameterName,
	const float CooldownSeconds,
	const FLinearColor& CooldownClockColor,
	const float ClockEdgeSoftness01,
	const float ClockStartOffset01,
	const float AspectRatio,
	const bool bStartOnCooldown,
	const TWeakObjectPtr<UObject>& WeakWorldContext)
{
	ClearCooldownTimer();

	M_WeakWorldContext = WeakWorldContext;
	M_WeakTimerWorld.Reset();

	M_CurrentTimeParameterCollection = CurrentTimeParameterCollection;
	M_CurrentTimeParameterName = CurrentTimeParameterName;

	M_CooldownDurationSeconds = FMath::Max(CooldownSeconds, 0.0f);
	M_CooldownStartTimeSeconds = 0.0f;
	M_CooldownClockColor = CooldownClockColor;
	M_ClockEdgeSoftness01 = FMath::Max(ClockEdgeSoftness01, CoolDownItem::MinimumClockEdgeSoftness01);
	M_ClockStartOffset01 = CoolDownItem::Normalize01(ClockStartOffset01);
	M_AspectRatio = FMath::Max(AspectRatio, CoolDownItem::MinimumAspectRatio);
	bM_IsOnCooldown = false;

	if (not GetIsValidButton())
	{
		return;
	}

	if (not CacheDynamicMaterial(CooldownMaterial))
	{
		return;
	}

	ApplyMaterialSetupParameters(IconTexture);

	if (not CacheTimerWorldFromContext()
		or not GetIsValidCurrentTimeParameterCollection()
		or not GetIsValidCurrentTimeParameterName())
	{
		ApplyCooldownMaterialDisabledState();
		return;
	}

	if (bStartOnCooldown)
	{
		BeginCooldownInternal();
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

	float CurrentGameTimeSeconds = 0.0f;

	if (not TryGetCurrentGameTimeSeconds(CurrentGameTimeSeconds))
	{
		return 0.0f;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, CurrentGameTimeSeconds - M_CooldownStartTimeSeconds);
	return FMath::Max(0.0f, M_CooldownDurationSeconds - ElapsedSeconds);
}

void UW_CoolDownItem::InstantlyResetCooldown()
{
	bM_IsOnCooldown = false;
	ClearCooldownTimer();
	ApplyCooldownMaterialCompletedState();
}

void UW_CoolDownItem::UpgradeCooldown(const float NewCooldown, const bool bResetCooldownState)
{
	M_CooldownDurationSeconds = FMath::Max(NewCooldown, 0.0f);

	if (bResetCooldownState)
	{
		InstantlyResetCooldown();
		return;
	}

	if (not bM_IsOnCooldown)
	{
		ApplyCooldownMaterialCompletedState();
		return;
	}

	float CurrentGameTimeSeconds = 0.0f;

	if (not TryGetCurrentGameTimeSeconds(CurrentGameTimeSeconds))
	{
		bM_IsOnCooldown = false;
		ClearCooldownTimer();
		ApplyCooldownMaterialDisabledState();
		return;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, CurrentGameTimeSeconds - M_CooldownStartTimeSeconds);

	if (M_CooldownDurationSeconds <= CoolDownItem::MinimumCooldownDurationSeconds
		or ElapsedSeconds >= M_CooldownDurationSeconds)
	{
		InstantlyResetCooldown();
		return;
	}

	ApplyCooldownMaterialActiveState();
	ScheduleCooldownTimer();
}

void UW_CoolDownItem::NativeDestruct()
{
	ClearCooldownTimer();

	bM_IsOnCooldown = false;
	M_DynamicMaterial = nullptr;
	M_CurrentTimeParameterCollection = nullptr;
	M_WeakWorldContext.Reset();
	M_WeakTimerWorld.Reset();

	Super::NativeDestruct();
}

bool UW_CoolDownItem::CacheDynamicMaterial(UMaterialInterface* const CooldownMaterial)
{
	if (not GetIsValidImage())
	{
		return false;
	}

	if (IsValid(CooldownMaterial))
	{
		M_Image->SetBrushFromMaterial(CooldownMaterial);
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

void UW_CoolDownItem::ApplyMaterialSetupParameters(UTexture2D* const IconTexture) const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	if (IsValid(IconTexture))
	{
		M_DynamicMaterial->SetTextureParameterValue(CoolDownItem::IconTextureParameterName, IconTexture);
	}

	M_DynamicMaterial->SetVectorParameterValue(CoolDownItem::CooldownClockColorParameterName, M_CooldownClockColor);
	M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::ClockEdgeSoftnessParameterName, M_ClockEdgeSoftness01);
	M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::ClockStartOffsetParameterName, M_ClockStartOffset01);
	M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::AspectRatioParameterName, M_AspectRatio);
}

void UW_CoolDownItem::ApplyCooldownMaterialActiveState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	M_DynamicMaterial->SetScalarParameterValue(
		CoolDownItem::CooldownDurationParameterName,
		M_CooldownDurationSeconds);

	M_DynamicMaterial->SetScalarParameterValue(
		CoolDownItem::CooldownStartTimeParameterName,
		M_CooldownStartTimeSeconds);
}

void UW_CoolDownItem::ApplyCooldownMaterialCompletedState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	const float SafeCooldownDurationSeconds = FMath::Max(M_CooldownDurationSeconds, 0.0f);

	if (SafeCooldownDurationSeconds <= CoolDownItem::MinimumCooldownDurationSeconds)
	{
		M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownDurationParameterName, 0.0f);
		M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownStartTimeParameterName, 0.0f);
		return;
	}

	float CurrentGameTimeSeconds = 0.0f;

	if (not TryGetCurrentGameTimeSeconds(CurrentGameTimeSeconds))
	{
		M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownDurationParameterName, 0.0f);
		M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownStartTimeParameterName, 0.0f);
		return;
	}

	M_DynamicMaterial->SetScalarParameterValue(
		CoolDownItem::CooldownDurationParameterName,
		SafeCooldownDurationSeconds);

	M_DynamicMaterial->SetScalarParameterValue(
		CoolDownItem::CooldownStartTimeParameterName,
		CurrentGameTimeSeconds - SafeCooldownDurationSeconds);
}

void UW_CoolDownItem::ApplyCooldownMaterialDisabledState() const
{
	if (not GetIsValidDynamicMaterial())
	{
		return;
	}

	M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownDurationParameterName, 0.0f);
	M_DynamicMaterial->SetScalarParameterValue(CoolDownItem::CooldownStartTimeParameterName, 0.0f);
}

void UW_CoolDownItem::BeginCooldownInternal()
{
	ClearCooldownTimer();

	if (M_CooldownDurationSeconds <= CoolDownItem::MinimumCooldownDurationSeconds)
	{
		InstantlyResetCooldown();
		return;
	}

	float CurrentGameTimeSeconds = 0.0f;

	if (not TryGetCurrentGameTimeSeconds(CurrentGameTimeSeconds))
	{
		bM_IsOnCooldown = false;
		ApplyCooldownMaterialDisabledState();
		return;
	}

	M_CooldownStartTimeSeconds = CurrentGameTimeSeconds;
	bM_IsOnCooldown = true;

	ApplyCooldownMaterialActiveState();
	ScheduleCooldownTimer();
}

void UW_CoolDownItem::HandleCooldownFinished()
{
	bM_IsOnCooldown = false;
	ClearCooldownTimer();
	ApplyCooldownMaterialCompletedState();
}

void UW_CoolDownItem::ScheduleCooldownTimer()
{
	ClearCooldownTimer();

	if (not bM_IsOnCooldown)
	{
		return;
	}

	float CurrentGameTimeSeconds = 0.0f;

	if (not TryGetCurrentGameTimeSeconds(CurrentGameTimeSeconds))
	{
		bM_IsOnCooldown = false;
		ApplyCooldownMaterialDisabledState();
		return;
	}

	const float ElapsedSeconds = FMath::Max(0.0f, CurrentGameTimeSeconds - M_CooldownStartTimeSeconds);
	const float RemainingSeconds = FMath::Max(0.0f, M_CooldownDurationSeconds - ElapsedSeconds);

	if (RemainingSeconds <= CoolDownItem::MinimumCooldownDurationSeconds)
	{
		HandleCooldownFinished();
		return;
	}

	UWorld* const TimerWorld = GetTimerWorld(false);

	if (not IsValid(TimerWorld))
	{
		bM_IsOnCooldown = false;
		ApplyCooldownMaterialDisabledState();
		return;
	}

	const TWeakObjectPtr<UW_CoolDownItem> WeakThis(this);

	FTimerDelegate CooldownFinishedDelegate;
	CooldownFinishedDelegate.BindLambda(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HandleCooldownFinished();
		});

	TimerWorld->GetTimerManager().SetTimer(
		M_CooldownTimerHandle,
		CooldownFinishedDelegate,
		RemainingSeconds,
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

bool UW_CoolDownItem::TryGetCurrentGameTimeSeconds(
	float& OutCurrentGameTimeSeconds,
	const bool bReportError) const
{
	OutCurrentGameTimeSeconds = 0.0f;

	if (not GetIsValidCurrentTimeParameterCollection(bReportError))
	{
		return false;
	}

	if (not GetIsValidCurrentTimeParameterName(bReportError))
	{
		return false;
	}

	UWorld* const TimerWorld = GetTimerWorld(bReportError);

	if (not IsValid(TimerWorld))
	{
		return false;
	}

	UMaterialParameterCollectionInstance* const CollectionInstance =
		TimerWorld->GetParameterCollectionInstance(M_CurrentTimeParameterCollection.Get());

	if (not IsValid(CollectionInstance))
	{
		if (bReportError)
		{
			ReportInvalidState(TEXT("Current time Material Parameter Collection instance is invalid."));
		}

		return false;
	}

	if (CollectionInstance->GetScalarParameterValue(M_CurrentTimeParameterName, OutCurrentGameTimeSeconds))
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(FString::Printf(
			TEXT("Current time scalar parameter '%s' was not found in the Material Parameter Collection."),
			*M_CurrentTimeParameterName.ToString()));
	}

	return false;
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

	ReportInvalidState(TEXT("M_DynamicMaterial is invalid. Ensure M_Image uses the cooldown UI material."));
	return false;
}

bool UW_CoolDownItem::GetIsValidCurrentTimeParameterCollection(const bool bReportError) const
{
	if (IsValid(M_CurrentTimeParameterCollection.Get()))
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("M_CurrentTimeParameterCollection is invalid."));
	}

	return false;
}

bool UW_CoolDownItem::GetIsValidCurrentTimeParameterName(const bool bReportError) const
{
	if (not M_CurrentTimeParameterName.IsNone())
	{
		return true;
	}

	if (bReportError)
	{
		ReportInvalidState(TEXT("M_CurrentTimeParameterName is None."));
	}

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
	const FString FullErrorMessage = FString::Printf(TEXT("UW_CoolDownItem: %s"), *ErrorMessage);
	RTSFunctionLibrary::ReportError(FullErrorMessage);
}
