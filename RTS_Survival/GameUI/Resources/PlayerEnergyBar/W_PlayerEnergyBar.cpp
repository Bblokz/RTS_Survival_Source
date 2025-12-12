// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

// W_PlayerEnergyBar.cpp

#include "W_PlayerEnergyBar.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "RTS_Survival/GameUI/Resources/PlayerEnergyBar/EnergyBarInfo/W_EnergyBarInfo.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

void UW_PlayerEnergyBar::InitPlayerEnergyBar(UPlayerResourceManager* NewPlayerResourceManager,
                                             UW_EnergyBarInfo* NewEnergyInfoWidget)
{
	if (not IsValid(NewPlayerResourceManager))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar::InitPlayerEnergyBar: PlayerResourceManager is null."));
		return;
	}
	if (not IsValid(NewEnergyInfoWidget))
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar::InitPlayerEnergyBar: EnergyInfoWidget is null."));
		return;
	}

	M_PlayerResourceManager = NewPlayerResourceManager;
	EnergyBarInfoWidget = NewEnergyInfoWidget;
	SetEnergyInfoBarVisibility(false);
}

void UW_PlayerEnergyBar::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache base Y from the level's canvas slot so we can add offsets around center.
	if (GetIsValidEnergyBarLevel())
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EnergyBarLevel->Slot))
		{
			M_EnergyLevelBasePosY = CanvasSlot->GetPosition().Y;
			bM_HasBasePosY = true;
		}
	}

	// Timer: update every 0.5s
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			EnergyBarUpdateTimerHandle,
			this,
			&UW_PlayerEnergyBar::UpdateEnergyBar,
			0.5f,
			true);
	}
}

void UW_PlayerEnergyBar::BeginDestroy()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnergyBarUpdateTimerHandle);
	}
	Super::BeginDestroy();
}

void UW_PlayerEnergyBar::OnHoverEnergyBar(const bool bIsHover)
{
	if (!GetIsValidEnergyInfoWidget())
	{
		return;
	}
	SetEnergyInfoBarVisibility(bIsHover);
}

void UW_PlayerEnergyBar::UpdateEnergyBar()
{
	if (!GetIsValidPlayerResourceManager())
	{
		return;
	}

	const int32 EnergySupply = M_PlayerResourceManager->GetPlayerEnergySupply();
	const int32 EnergyDemand = M_PlayerResourceManager->GetPlayerEnergyDemand();

	// Non-linear window: compute supply's active window and relative positions.
	int64 WindowStart = 0;
	int32 WindowSize = 1; // safe default to avoid div-by-zero
	int32 SupplyWithin = 0;
	DecomposeIntoWindow(FMath::Max(0, EnergySupply), WindowStart, WindowSize, SupplyWithin);

	const float SupplyFill01 = FMath::Clamp(static_cast<float>(SupplyWithin) / static_cast<float>(WindowSize), 0.f, 1.f);

	// Demand marker sits within the *same* active window as supply (clamped).
	const int32 DemandWithin = FMath::Clamp(FMath::Max(0, EnergyDemand) - static_cast<int32>(WindowStart), 0, WindowSize);
	const float DemandLevel01 = FMath::Clamp(static_cast<float>(DemandWithin) / static_cast<float>(WindowSize), 0.f, 1.f);

	const bool bDeficit = EnergySupply < EnergyDemand;

	UpdateProgressBarUI(SupplyFill01, bDeficit);
	UpdateEnergyLevelUI(DemandLevel01);

	// Forward numbers to the info widget (if present).
	if (GetIsValidEnergyInfoWidget())
	{
		EnergyBarInfoWidget->SetSupplyDemand(EnergySupply, EnergyDemand);
	}
}

void UW_PlayerEnergyBar::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!GetIsValidEnergyProgressBar())
	{
		return;
	}

	const bool bIsHoveredNow = EnergyProgressBar->IsHovered();
	if (bIsHoveredNow == bM_WasProgressBarHovered)
	{
		return;
	}

	bM_WasProgressBarHovered = bIsHoveredNow;
	OnHoverEnergyBar(bIsHoveredNow);
}

// ----------------- Validators -----------------

bool UW_PlayerEnergyBar::GetIsValidPlayerResourceManager() const
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar: PlayerResourceManager is not valid."));
	return false;
}

bool UW_PlayerEnergyBar::GetIsValidEnergyProgressBar() const
{
	if (IsValid(EnergyProgressBar))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar: EnergyProgressBar is not bound."));
	return false;
}

bool UW_PlayerEnergyBar::GetIsValidEnergyBarLevel() const
{
	if (IsValid(EnergyBarLevel))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar: EnergyBarLevel is not bound."));
	return false;
}

bool UW_PlayerEnergyBar::GetIsValidEnergyInfoWidget() const
{
	if (IsValid(EnergyBarInfoWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar: EnergyBarInfoWidget is not valid."));
	return false;
}

// ----------------- Window helpers -----------------

int32 UW_PlayerEnergyBar::GetWindowSizeForIndex(const int64 WindowIndex) const
{
	if (EnergyWindowSizes.Num() <= 0)
	{
		// Safe fallback; report once.
		RTSFunctionLibrary::ReportError(TEXT("UW_PlayerEnergyBar: EnergyWindowSizes is empty. Using fallback size = 300."));
		return 300;
	}

	if (WindowIndex < EnergyWindowSizes.Num())
	{
		const int32 Size = EnergyWindowSizes[static_cast<int32>(WindowIndex)];
		return FMath::Max(1, Size);
	}

	// Repeat the last size indefinitely.
	const int32 LastSize = EnergyWindowSizes.Last();
	return FMath::Max(1, LastSize);
}

void UW_PlayerEnergyBar::DecomposeIntoWindow(const int32 Value,
                                             int64& OutWindowStart,
                                             int32& OutWindowSize,
                                             int32& OutWithin) const
{
	const int32 ClampedValue = FMath::Max(0, Value);

	// Sum all windows except the last. If still larger, use arithmetic with the last window.
	int64 Accumulated = 0;
	const int32 Count = EnergyWindowSizes.Num();

	if (Count <= 0)
	{
		OutWindowSize = 300;
		OutWindowStart = 0;
		OutWithin = ClampedValue % OutWindowSize;
		return;
	}

	// Walk fixed part (all but the last)
	for (int32 i = 0; i < Count - 1; ++i)
	{
		const int32 Size = FMath::Max(1, EnergyWindowSizes[i]);
		if (ClampedValue < Accumulated + Size)
		{
			OutWindowStart = Accumulated;
			OutWindowSize = Size;
			OutWithin = ClampedValue - static_cast<int32>(Accumulated);
			return;
		}
		Accumulated += Size;
	}

	// Tail (repeat last)
	const int32 LastSize = FMath::Max(1, EnergyWindowSizes.Last());
	if (ClampedValue < Accumulated + LastSize)
	{
		OutWindowStart = Accumulated;
		OutWindowSize = LastSize;
		OutWithin = ClampedValue - static_cast<int32>(Accumulated);
		return;
	}

	const int64 TailValue = ClampedValue - Accumulated;
	const int64 ExtraWindows = TailValue / LastSize;
	const int32 Remainder = static_cast<int32>(TailValue % LastSize);

	OutWindowStart = Accumulated + (ExtraWindows * LastSize);
	OutWindowSize = LastSize;
	OutWithin = Remainder;
}

// ----------------- UI helpers -----------------

void UW_PlayerEnergyBar::SetEnergyInfoBarVisibility(const bool bVisible) const
{
	if (!GetIsValidEnergyInfoWidget())
	{
		return;
	}
	EnergyBarInfoWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_PlayerEnergyBar::UpdateProgressBarUI(const float Fill01, const bool bInDeficit) const
{
	if (!GetIsValidEnergyProgressBar())
	{
		return;
	}

	EnergyProgressBar->SetPercent(FMath::Clamp(Fill01, 0.f, 1.f));
	EnergyProgressBar->SetFillColorAndOpacity(bInDeficit ? DeficitColor : OkColor);
}

void UW_PlayerEnergyBar::UpdateEnergyLevelUI(const float Level01) const
{
	if (!GetIsValidEnergyBarLevel())
	{
		return;
	}
	if (!bM_HasBasePosY)
	{
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EnergyBarLevel->Slot))
	{
		const FVector2D CurrentPos = CanvasSlot->GetPosition();
		const float YOffset = MapPercentToYOffset(FMath::Clamp(Level01, 0.f, 1.f));
		CanvasSlot->SetPosition(FVector2D(CurrentPos.X, M_EnergyLevelBasePosY + YOffset));
	}
}

float UW_PlayerEnergyBar::MapPercentToYOffset(const float Percent01) const
{
	// 0 => Highest (top), 1 => Lowest (bottom)
	const float Y = FMath::Lerp(EnergyLevelHighestYOffset, EnergyLevelLowestYOffset, Percent01);
	return -FMath::Clamp(Y, EnergyLevelHighestYOffset, EnergyLevelLowestYOffset);
}
