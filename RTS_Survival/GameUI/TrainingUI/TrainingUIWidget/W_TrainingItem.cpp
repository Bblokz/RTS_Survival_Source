// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_TrainingItem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/GameUI/CooldownItem/W_CoolDownItem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"

UW_TrainingItem::UW_TrainingItem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, M_TrainingItemSizeBox(nullptr)
	, M_TrainingItemButton(nullptr)
	, M_TrainingUIManager(nullptr)
	, M_Index(0)
	, M_CooldownIconTexture(nullptr)
	, M_ClockSecondsLeft(0)
	, M_ClockTotalTrainingTime(0)
	, bM_HasActiveClockState(false)
	, bM_IsClockPaused(false)
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
	ResetCooldownItemForMissingBrush();
	// propagate to blueprint
	OnUpdateTrainingItem(TrainingItemState);
}

void UW_TrainingItem::SetTrainingButtonRenderOpacity(const float NewRenderOpacity)
{
	if (M_TrainingItemButton)
	{
		M_TrainingItemButton->SetRenderOpacity(NewRenderOpacity);
	}
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

	M_ClockSecondsLeft = TimeRemaining;
	M_ClockTotalTrainingTime = TotalTrainingTime;
	bM_HasActiveClockState = TimeRemaining > 0;
	bM_IsClockPaused = bIsPaused;

	ApplyCooldownItemFromCachedState();
}

void UW_TrainingItem::StopClock()
{
	ResetCachedCooldownState();

	if (not GetIsValidCoolDownItem())
	{
		return;
	}

	if (not M_CoolDownItem->GetWasInitialized(false))
	{
		return;
	}

	M_CoolDownItem->PauseClock(false);
	M_CoolDownItem->InstantlyResetCooldown();
}

void UW_TrainingItem::SetClockPaused(const bool bPause)
{
	bM_IsClockPaused = bPause;

	if (not bM_HasActiveClockState)
	{
		return;
	}

	if (not GetIsValidCoolDownItem())
	{
		return;
	}

	if (not M_CoolDownItem->GetWasInitialized(false))
	{
		return;
	}

	M_CoolDownItem->PauseClock(bPause);
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

void UW_TrainingItem::OnObtainedSlateBrushFromTrainingTable(USlateBrushAsset* SlateBrushAsset)
{
	// Note; no error report as not all training items have brushes set.
	if (not IsValid(SlateBrushAsset))
	{
		ResetCooldownItemForMissingBrush();
		return;
	}

	UTexture2D* const IconTexture = GetIconTextureFromSlateBrushAsset(SlateBrushAsset);

	if (not IsValid(IconTexture))
	{
		ResetCooldownItemForMissingBrush();
		return;
	}

	M_CooldownIconTexture = IconTexture;
	ApplyCooldownItemFromCachedState();
}

void UW_TrainingItem::ResetCachedCooldownState()
{
	M_ClockSecondsLeft = 0;
	M_ClockTotalTrainingTime = 0;
	bM_HasActiveClockState = false;
	bM_IsClockPaused = false;
}

void UW_TrainingItem::ResetCooldownItemForMissingBrush()
{
	M_CooldownIconTexture = nullptr;
	ResetCachedCooldownState();

	if (not GetIsValidCoolDownItem())
	{
		return;
	}

	M_CoolDownItem->ClearCooldownMaterial();
}

void UW_TrainingItem::ApplyCooldownItemFromCachedState()
{
	if (not GetIsValidCoolDownItem())
	{
		return;
	}

	if (not IsValid(M_CooldownIconTexture))
	{
		return;
	}

	const int32 CooldownDurationSeconds = bM_HasActiveClockState
		                                      ? M_ClockTotalTrainingTime
		                                      : M_TrainingItemState.TotalTrainingTime;

	if (CooldownDurationSeconds <= 0)
	{
		M_CoolDownItem->ClearCooldownMaterial();
		return;
	}

	const float SecondsLeft = bM_HasActiveClockState
		                          ? static_cast<float>(M_ClockSecondsLeft)
		                          : 0.0f;

	M_CoolDownItem->Init(
		TWeakObjectPtr<UObject>(this),
		M_CooldownIconTexture.Get(),
		static_cast<float>(CooldownDurationSeconds),
		false,
		bM_HasActiveClockState && bM_IsClockPaused,
		SecondsLeft);
}

UTexture2D* UW_TrainingItem::GetIconTextureFromSlateBrushAsset(USlateBrushAsset* SlateBrushAsset) const
{
	if (not IsValid(SlateBrushAsset))
	{
		return nullptr;
	}

	UObject* const BrushResource = SlateBrushAsset->Brush.GetResourceObject();

	if (not IsValid(BrushResource))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("No valid brush resource on slate brush obtained from data table."));
		return nullptr;
	}

	UTexture2D* const IconTexture = Cast<UTexture2D>(BrushResource);

	if (not IsValid(IconTexture))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Could not cast slate brush resource to UTexture2D in Training item."));
		return nullptr;
	}

	return IconTexture;
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

bool UW_TrainingItem::GetIsValidCoolDownItem() const
{
	if (IsValid(M_CoolDownItem))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_CoolDownItem",
		"GetIsValidCoolDownItem",
		this
	);

	return false;
}
