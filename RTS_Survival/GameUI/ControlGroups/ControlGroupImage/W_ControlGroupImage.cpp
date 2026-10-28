// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ControlGroupImage.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyProviderSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ControlGroupGlass
{
	// Scalars on M_UI_GlassPane (ui_glass_panes.py).
	const FName HoverParameter = TEXT("Hover");
	const FName HoverSweepParameter = TEXT("HoverSweep");
	const FName PressedParameter = TEXT("Pressed");
	// 1 while a unit icon is under the pane: rounds the icon's corners and darkens it towards the rim.
	const FName OccupiedParameter = TEXT("Occupied");
	const FName ScratchSeedParameter = TEXT("ScratchSeed");
	const FName ScratchCoverageParameter = TEXT("ScratchCoverage");
	const FName ScratchLengthParameter = TEXT("ScratchLength");
	const FName DustIntensityParameter = TEXT("DustIntensity");
	const FName StreakOffsetParameter = TEXT("StreakOffset");

	// Per-slot wear: every slot is scratched differently, within a range that still reads as one set.
	constexpr int32 WearSeedPrime = 7919;
	constexpr int32 WearSeedOffset = 13;
	constexpr float ScratchSeedBase = 5.0f;
	constexpr float ScratchSeedStep = 7.3f;
	constexpr float MinScratchCoverage = 0.35f;
	constexpr float MaxScratchCoverage = 0.55f;
	constexpr float MinScratchLength = 0.7f;
	constexpr float MaxScratchLength = 1.1f;
	constexpr float MinDustIntensity = 0.03f;
	constexpr float MaxDustIntensity = 0.08f;
	constexpr float StreakOffsetCentre = 0.22f;
	constexpr float StreakOffsetRange = 0.08f;

	// Larger groups show "99+", so the count never runs into the unit render's type badge.
	constexpr int32 MaxShownUnitCount = 99;

	// Smaller changes are invisible; skips redundant material writes.
	constexpr float MinParameterChange = 0.002f;
}

#if !UE_BUILD_SHIPPING
float UW_ControlGroupImage::Debug_FrozenSweepProgress = -1.0f;
#endif

void UW_ControlGroupImage::InitControlGroupImage(ULocalPlayer* OwningLocalPlayer, const int32 GroupIndex)
{
	if (not IsValid(OwningLocalPlayer))
	{
		RTSFunctionLibrary::ReportError("Control group image received an invalid local player during init.");
		return;
	}

	M_GroupIndex = GroupIndex;

	CacheHotkeyProviderSubsystem(OwningLocalPlayer);
	UpdateControlGroupHotkey();
	BindHotkeyUpdateDelegate();
	BindButtonClick();
	ApplySlotWear();
	ApplyOccupiedState(0);
}

void UW_ControlGroupImage::UpdateControlGroup(const FTrainingOption& MostFrequentUnitInGroup, const int32 UnitCount)
{
	const bool bIsEmptyGroup = MostFrequentUnitInGroup.IsNone() || UnitCount <= 0;
	ApplyOccupiedState(bIsEmptyGroup ? 0 : UnitCount);
	if (bIsEmptyGroup)
	{
		return;
	}
	OnUpdateControlGroup(MostFrequentUnitInGroup);
}

void UW_ControlGroupImage::PlayRecallFlash()
{
	M_GlassAnim.M_RecallFlash = 1.0f;
}

void UW_ControlGroupImage::NativeConstruct()
{
	Super::NativeConstruct();
	// Re-applies the last state when the slot is constructed again; until the first update it is an empty group.
	ApplyOccupiedState(M_UnitCount);
}

void UW_ControlGroupImage::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickGlassAnimation(InDeltaTime);
}

void UW_ControlGroupImage::NativeDestruct()
{
	UnbindHotkeyUpdateDelegate();
	Super::NativeDestruct();
}

void UW_ControlGroupImage::CacheHotkeyProviderSubsystem(ULocalPlayer* OwningLocalPlayer)
{
	URTSHotkeyProviderSubsystem* HotkeyProviderSubsystem = OwningLocalPlayer->GetSubsystem<URTSHotkeyProviderSubsystem>();
	if (not IsValid(HotkeyProviderSubsystem))
	{
		RTSFunctionLibrary::ReportError("Control group image could not resolve the hotkey provider subsystem.");
		return;
	}

	M_HotkeyProviderSubsystem = HotkeyProviderSubsystem;
}

void UW_ControlGroupImage::BindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	M_ControlGroupHotkeyHandle = M_HotkeyProviderSubsystem->OnControlGroupHotkeyUpdated().AddUObject(
		this,
		&UW_ControlGroupImage::HandleControlGroupHotkeyUpdated
	);
}

void UW_ControlGroupImage::UnbindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	if (M_ControlGroupHotkeyHandle.IsValid())
	{
		M_HotkeyProviderSubsystem->OnControlGroupHotkeyUpdated().Remove(M_ControlGroupHotkeyHandle);
		M_ControlGroupHotkeyHandle.Reset();
	}
}

void UW_ControlGroupImage::UpdateControlGroupHotkey()
{
	if (not GetIsValidHotkeyProviderSubsystem() || not GetIsValidControlGroupHotKey())
	{
		return;
	}

	const FText HotkeyText = M_HotkeyProviderSubsystem->GetDisplayKeyForControlGroupSlot(M_GroupIndex);
	M_ControlGroupHotKey->SetKeyText(HotkeyText);
}

void UW_ControlGroupImage::HandleControlGroupHotkeyUpdated(const int32 GroupIndex, const FText& HotkeyText)
{
	if (GroupIndex != M_GroupIndex)
	{
		return;
	}

	if (not GetIsValidControlGroupHotKey())
	{
		return;
	}

	M_ControlGroupHotKey->SetKeyText(HotkeyText);
}

void UW_ControlGroupImage::BindButtonClick()
{
	// Optional: a slot without the button only displays its group.
	if (not IsValid(M_ControlGroupButton))
	{
		return;
	}
	M_ControlGroupButton->OnClicked.AddUniqueDynamic(this, &UW_ControlGroupImage::OnControlGroupButtonClicked);
}

void UW_ControlGroupImage::OnControlGroupButtonClicked()
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetOwningPlayer());
	if (not IsValid(PlayerController))
	{
		return;
	}
	PlayerController->UseControlGroupFromUI(M_GroupIndex);
}

void UW_ControlGroupImage::ApplySlotWear()
{
	UMaterialInstanceDynamic* const GlassMaterial = GetGlassMaterial();
	if (not IsValid(GlassMaterial))
	{
		return;
	}
	using namespace ControlGroupGlass;
	FRandomStream WearStream(M_GroupIndex * WearSeedPrime + WearSeedOffset);
	GlassMaterial->SetScalarParameterValue(ScratchSeedParameter, ScratchSeedBase + M_GroupIndex * ScratchSeedStep);
	GlassMaterial->SetScalarParameterValue(
		ScratchCoverageParameter, WearStream.FRandRange(MinScratchCoverage, MaxScratchCoverage));
	GlassMaterial->SetScalarParameterValue(
		ScratchLengthParameter, WearStream.FRandRange(MinScratchLength, MaxScratchLength));
	GlassMaterial->SetScalarParameterValue(
		DustIntensityParameter, WearStream.FRandRange(MinDustIntensity, MaxDustIntensity));
	GlassMaterial->SetScalarParameterValue(
		StreakOffsetParameter,
		StreakOffsetCentre + WearStream.FRandRange(-StreakOffsetRange, StreakOffsetRange));
}

void UW_ControlGroupImage::ApplyOccupiedState(const int32 UnitCount)
{
	M_UnitCount = FMath::Max(0, UnitCount);
	const bool bIsOccupied = M_UnitCount > 0;
	if (IsValid(M_TrainingItemImage))
	{
		M_TrainingItemImage->SetVisibility(bIsOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (IsValid(M_UnitCountText))
	{
		M_UnitCountText->SetText(M_UnitCount > ControlGroupGlass::MaxShownUnitCount
			                         ? FText::FromString(FString::Printf(TEXT("%d+"), ControlGroupGlass::MaxShownUnitCount))
			                         : FText::AsNumber(M_UnitCount));
		M_UnitCountText->SetVisibility(bIsOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (IsValid(M_GlassPane))
	{
		M_GlassPane->SetRenderOpacity(bIsOccupied ? 1.0f : M_GlassSettings.EmptyPaneOpacity);
	}
	if (UMaterialInstanceDynamic* const GlassMaterial = GetGlassMaterial())
	{
		GlassMaterial->SetScalarParameterValue(ControlGroupGlass::OccupiedParameter, bIsOccupied ? 1.0f : 0.0f);
	}
	if (IsValid(M_HotkeyTab))
	{
		M_HotkeyTab->SetBrushColor(bIsOccupied ? FLinearColor::White : FLinearColor::Transparent);
	}
	if (IsValid(M_ControlGroupHotKey))
	{
		M_ControlGroupHotKey->SetRenderOpacity(
			bIsOccupied ? M_GlassSettings.OccupiedHotkeyOpacity : M_GlassSettings.EmptyHotkeyOpacity);
	}
}

void UW_ControlGroupImage::TickGlassAnimation(const float DeltaSeconds)
{
	// The slot itself is self-hit-test-invisible, so only its button sees the mouse.
	const bool bIsHovered = IsValid(M_ControlGroupButton) ? M_ControlGroupButton->IsHovered() : IsHovered();
	if (bIsHovered && not M_GlassAnim.bM_WasHovered)
	{
		M_GlassAnim.M_SweepProgress = 0.0f;
	}
	M_GlassAnim.bM_WasHovered = bIsHovered;

	const float HoverSeconds = bIsHovered ? M_GlassSettings.HoverInSeconds : M_GlassSettings.HoverOutSeconds;
	M_GlassAnim.M_HoverRamp = FMath::FInterpConstantTo(
		M_GlassAnim.M_HoverRamp, bIsHovered ? 1.0f : 0.0f, DeltaSeconds, 1.0f / HoverSeconds);
	M_GlassAnim.M_SweepProgress = FMath::Min(
		1.0f, M_GlassAnim.M_SweepProgress + DeltaSeconds / M_GlassSettings.SweepSeconds);
	M_GlassAnim.M_RecallFlash = FMath::Max(
		0.0f, M_GlassAnim.M_RecallFlash - DeltaSeconds / M_GlassSettings.RecallFlashSeconds);
	const bool bIsPressed = IsValid(M_ControlGroupButton) && M_ControlGroupButton->IsPressed();
	M_GlassAnim.M_PressRamp = FMath::FInterpConstantTo(
		M_GlassAnim.M_PressRamp, bIsPressed ? 1.0f : 0.0f, DeltaSeconds, 1.0f / M_GlassSettings.PressSeconds);
	SendGlassAnimToMaterial();
}

void UW_ControlGroupImage::SendGlassAnimToMaterial()
{
	if (not IsValid(M_GlassMaterial))
	{
		return;
	}
	using namespace ControlGroupGlass;
	const float HoverStrength = M_UnitCount > 0 ? 1.0f : M_GlassSettings.EmptyHoverStrength;
	const float RecallHover = M_GlassSettings.RecallFlashHover * FMath::Square(M_GlassAnim.M_RecallFlash);
	const float Hover = FMath::Max(
		FMath::SmoothStep(0.0f, 1.0f, M_GlassAnim.M_HoverRamp) * HoverStrength, RecallHover);
	// Ease out: the band enters quickly and settles into the bottom-right corner.
	float SweepProgress = M_GlassAnim.M_SweepProgress;
#if !UE_BUILD_SHIPPING
	if (Debug_FrozenSweepProgress >= 0.0f && M_GlassAnim.bM_WasHovered)
	{
		SweepProgress = Debug_FrozenSweepProgress;
	}
#endif
	const float Sweep = 1.0f - FMath::Square(1.0f - SweepProgress);
	const float Pressed = M_GlassAnim.M_PressRamp;

	if (not FMath::IsNearlyEqual(Hover, M_GlassAnim.M_SentHover, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(HoverParameter, Hover);
		M_GlassAnim.M_SentHover = Hover;
	}
	if (not FMath::IsNearlyEqual(Sweep, M_GlassAnim.M_SentSweep, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(HoverSweepParameter, Sweep);
		M_GlassAnim.M_SentSweep = Sweep;
	}
	if (not FMath::IsNearlyEqual(Pressed, M_GlassAnim.M_SentPressed, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(PressedParameter, Pressed);
		M_GlassAnim.M_SentPressed = Pressed;
	}
}

UMaterialInstanceDynamic* UW_ControlGroupImage::GetGlassMaterial()
{
	if (IsValid(M_GlassMaterial))
	{
		return M_GlassMaterial;
	}
	// Optional: a slot without the glass pane simply has no glass.
	if (not IsValid(M_GlassPane))
	{
		return nullptr;
	}
	M_GlassMaterial = M_GlassPane->GetDynamicMaterial();
	return M_GlassMaterial;
}

bool UW_ControlGroupImage::GetIsValidControlGroupHotKey() const
{
	if (IsValid(M_ControlGroupHotKey))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ControlGroupHotKey"),
		TEXT("UW_ControlGroupImage::GetIsValidControlGroupHotKey"),
		this
	);
	return false;
}

bool UW_ControlGroupImage::GetIsValidHotkeyProviderSubsystem() const
{
	if (M_HotkeyProviderSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_HotkeyProviderSubsystem"),
		TEXT("UW_ControlGroupImage::GetIsValidHotkeyProviderSubsystem"),
		this
	);
	return false;
}
