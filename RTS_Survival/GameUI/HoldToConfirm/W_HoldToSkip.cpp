#include "W_HoldToSkip.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace HoldToSkipInternal
{
	constexpr float MinHoldDuration = 0.01f;
	constexpr float FullProgress = 1.0f;
	constexpr float EmptyProgress = 0.0f;
	constexpr float BlinkNormalizedMin = 0.0f;
	constexpr float BlinkNormalizedMax = 1.0f;

	/** Forwards Space down / up to the prompt before focus routing, so the skip never depends on keyboard focus. */
	class FHoldToSkipInputProcessor final : public IInputProcessor
	{
	public:
		explicit FHoldToSkipInputProcessor(UW_HoldToSkip* InWidget)
			: M_Widget(InWidget)
		{
		}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			UW_HoldToSkip* const Widget = GetActiveWidget(InKeyEvent);
			if (Widget == nullptr)
			{
				return false;
			}
			Widget->StartHold();
			return true;
		}

		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			UW_HoldToSkip* const Widget = GetActiveWidget(InKeyEvent);
			if (Widget == nullptr)
			{
				return false;
			}
			Widget->StopHold();
			return true;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("HoldToSkip"); }

	private:
		TWeakObjectPtr<UW_HoldToSkip> M_Widget;

		UW_HoldToSkip* GetActiveWidget(const FKeyEvent& InKeyEvent) const
		{
			if (InKeyEvent.GetKey() != EKeys::SpaceBar)
			{
				return nullptr;
			}
			UW_HoldToSkip* const Widget = M_Widget.Get();
			return IsValid(Widget) && Widget->IsInViewport() ? Widget : nullptr;
		}
	};
}

void UW_HoldToSkip::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (M_HoldPromptText != nullptr)
	{
		M_HoldPromptText->SetText(M_DefaultPromptText);
	}

	ResetProgress();
	RegisterInputProcessor();
}

void UW_HoldToSkip::NativeDestruct()
{
	UnregisterInputProcessor();
	Super::NativeDestruct();
}

void UW_HoldToSkip::RegisterInputProcessor()
{
	if (M_InputProcessor.IsValid() || not FSlateApplication::IsInitialized())
	{
		return;
	}
	M_InputProcessor = MakeShared<HoldToSkipInternal::FHoldToSkipInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(M_InputProcessor);
}

void UW_HoldToSkip::UnregisterInputProcessor()
{
	if (not M_InputProcessor.IsValid())
	{
		return;
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(M_InputProcessor);
	}
	M_InputProcessor.Reset();
}

void UW_HoldToSkip::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (not M_HoldRuntimeState.bM_IsHolding)
	{
		UpdateIdlePromptBlink(InDeltaTime);
		return;
	}

	if (M_HoldRuntimeState.bM_HasCompletedInCurrentHold)
	{
		return;
	}
	// The game window lost focus (alt-tab): the key up will never arrive, so the hold must not finish on its own.
	if (FSlateApplication::IsInitialized() && not FSlateApplication::Get().IsActive())
	{
		StopHold();
		return;
	}

	SetPromptOpacity(HoldToSkipInternal::FullProgress);
	UpdateHoldProgress(InDeltaTime);
}

FReply UW_HoldToSkip::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::SpaceBar)
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	StartHold();
	return FReply::Handled();
}

FReply UW_HoldToSkip::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::SpaceBar)
	{
		return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
	}

	StopHold();
	return FReply::Handled();
}

void UW_HoldToSkip::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	// Space comes through the input preprocessor, so losing focus to another widget keeps the hold. Only when the
	// game window itself loses focus (no key up will arrive) does the hold stop.
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive())
	{
		return;
	}
	StopHold();
}

void UW_HoldToSkip::StartHold()
{
	if (M_HoldRuntimeState.bM_IsHolding)
	{
		return;
	}

	M_HoldRuntimeState.bM_IsHolding = true;
	M_HoldRuntimeState.bM_HasCompletedInCurrentHold = false;
	SetPromptOpacity(HoldToSkipInternal::FullProgress);
}

void UW_HoldToSkip::StopHold()
{
	if (not M_HoldRuntimeState.bM_IsHolding)
	{
		return;
	}

	M_HoldRuntimeState.bM_IsHolding = false;
	ResetProgress();
}

void UW_HoldToSkip::ResetProgress()
{
	M_HoldRuntimeState.M_CurrentHoldTime = HoldToSkipInternal::EmptyProgress;
	M_HoldRuntimeState.M_CurrentHoldDelayTime = HoldToSkipInternal::EmptyProgress;
	M_HoldRuntimeState.M_DisplayProgress = HoldToSkipInternal::EmptyProgress;
	M_HoldRuntimeState.M_IdleBlinkElapsedTime = HoldToSkipInternal::EmptyProgress;
	M_HoldRuntimeState.bM_HasCompletedInCurrentHold = false;
	UpdateProgressBarVisual(HoldToSkipInternal::EmptyProgress, HoldToSkipInternal::EmptyProgress);
}

void UW_HoldToSkip::SetHoldDuration(const float InDuration)
{
	M_HoldDuration = FMath::Max(InDuration, HoldToSkipInternal::MinHoldDuration);
}

bool UW_HoldToSkip::GetIsValidProgressBar() const
{
	if (IsValid(M_ProgressBar))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ProgressBar",
		"GetIsValidProgressBar",
		this
	);

	return false;
}

void UW_HoldToSkip::UpdateHoldProgress(const float InDeltaTime)
{
	if (M_HoldRuntimeState.M_CurrentHoldDelayTime < M_HoldStartDelay)
	{
		M_HoldRuntimeState.M_CurrentHoldDelayTime += InDeltaTime;
		UpdateProgressBarVisual(InDeltaTime, HoldToSkipInternal::EmptyProgress);
		return;
	}

	M_HoldRuntimeState.M_CurrentHoldTime += InDeltaTime;
	const float NormalizedProgress = GetNormalizedProgress();
	UpdateProgressBarVisual(InDeltaTime, NormalizedProgress);

	if (NormalizedProgress < HoldToSkipInternal::FullProgress)
	{
		return;
	}

	HandleHoldCompleted();
}

void UW_HoldToSkip::UpdateProgressBarVisual(const float InDeltaTime, const float InTargetProgress)
{
	if (not GetIsValidProgressBar())
	{
		return;
	}

	M_HoldRuntimeState.M_DisplayProgress = FMath::FInterpTo(
		M_HoldRuntimeState.M_DisplayProgress,
		InTargetProgress,
		InDeltaTime,
		M_ProgressInterpSpeed
	);

	M_HoldRuntimeState.M_DisplayProgress = FMath::Clamp(
		M_HoldRuntimeState.M_DisplayProgress,
		HoldToSkipInternal::EmptyProgress,
		HoldToSkipInternal::FullProgress
	);

	M_ProgressBar->SetPercent(M_HoldRuntimeState.M_DisplayProgress);
}

void UW_HoldToSkip::HandleHoldCompleted()
{
	if (M_HoldRuntimeState.bM_HasCompletedInCurrentHold)
	{
		return;
	}

	M_HoldRuntimeState.bM_HasCompletedInCurrentHold = true;
	M_HoldRuntimeState.bM_IsHolding = false;
	OnHoldCompleted.Broadcast();
	ResetProgress();
}

float UW_HoldToSkip::GetNormalizedProgress() const
{
	const float SafeDuration = FMath::Max(M_HoldDuration, HoldToSkipInternal::MinHoldDuration);
	return FMath::Clamp(
		M_HoldRuntimeState.M_CurrentHoldTime / SafeDuration,
		HoldToSkipInternal::EmptyProgress,
		HoldToSkipInternal::FullProgress
	);
}

void UW_HoldToSkip::UpdateIdlePromptBlink(const float InDeltaTime)
{
	M_HoldRuntimeState.M_IdleBlinkElapsedTime += InDeltaTime;
	const float BlinkSineValue = FMath::Sin(M_HoldRuntimeState.M_IdleBlinkElapsedTime * TWO_PI * M_IdleBlinkFrequency);
	const float BlinkNormalized = FMath::Clamp(
		(BlinkSineValue + HoldToSkipInternal::FullProgress) * 0.5f,
		HoldToSkipInternal::BlinkNormalizedMin,
		HoldToSkipInternal::BlinkNormalizedMax
	);

	const float BlinkOpacity = FMath::Lerp(M_IdleBlinkMinOpacity, M_IdleBlinkMaxOpacity, BlinkNormalized);
	SetPromptOpacity(BlinkOpacity);
}

void UW_HoldToSkip::SetPromptOpacity(const float InOpacity) const
{
	if (M_HoldPromptText == nullptr)
	{
		return;
	}

	M_HoldPromptText->SetRenderOpacity(FMath::Clamp(InOpacity, HoldToSkipInternal::EmptyProgress, HoldToSkipInternal::FullProgress));
}
