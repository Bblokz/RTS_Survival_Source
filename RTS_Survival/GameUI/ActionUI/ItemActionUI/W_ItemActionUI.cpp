// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_ItemActionUI.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/LocalPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyProviderSubsystem.h"

namespace ItemActionUICooldownFill
{
	// Name of the scalar on M_UI_GlassPane: 0 = ready, 1 = cooldown just started.
	const FName CooldownRemainingParameter = TEXT("CooldownRemaining");

	// UCommandData counts cooldowns down once per second, so estimates from one cooldown differ by at most this.
	constexpr double EstimateWindowSeconds = 1.05;

	// Smaller changes of the fill are not visible; skips redundant material writes.
	constexpr float MinFractionChange = 0.0005f;
}

void UW_ItemActionUI::UpdateItemActionUI(const EAbilityID NewAbility, const int32 CustomType, const int32 CoolDownRemaining, const int32 CooldownTotalDuration)
{
	const bool bIsSameAbility = M_Ability == NewAbility && M_CustomType == CustomType;
	M_Ability = NewAbility;
	M_CustomType = CustomType;
	OnUpdateActionUI(NewAbility, CustomType, CoolDownRemaining, CooldownTotalDuration);
	// Abilities without an icon (IdNoAbility) leave a texture-less brush that Slate draws as a solid white quad.
	HideActionImageWithoutIcon();
	UpdateCooldownFill(bIsSameAbility, CoolDownRemaining, CooldownTotalDuration);
}

void UW_ItemActionUI::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (not M_CooldownFill.bIsCoolingDown)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	ApplyCooldownFillAtTime(World->GetTimeSeconds());
}

void UW_ItemActionUI::UpdateCooldownFill(const bool bIsSameAbility, const int32 CooldownRemaining,
                                         const int32 CooldownDuration)
{
	const UWorld* World = GetWorld();
	if (CooldownRemaining <= 0 || CooldownDuration <= 0 || not IsValid(World))
	{
		StopCooldownFill();
		return;
	}

	const double NowSeconds = World->GetTimeSeconds();
	const double EstimatedEndTime = NowSeconds + CooldownRemaining;
	// An update between two cooldown ticks overestimates the end by up to a second; the earliest estimate is the
	// truest. A larger difference means the cooldown restarted or the slot now shows another unit's cooldown.
	const bool bKeepAnchor = bIsSameAbility && M_CooldownFill.bIsCoolingDown
		&& FMath::IsNearlyEqual(M_CooldownFill.CooldownDuration, static_cast<float>(CooldownDuration))
		&& FMath::Abs(EstimatedEndTime - M_CooldownFill.CooldownEndWorldTime)
		<= ItemActionUICooldownFill::EstimateWindowSeconds;

	M_CooldownFill.CooldownEndWorldTime = bKeepAnchor
		                                      ? FMath::Min(M_CooldownFill.CooldownEndWorldTime, EstimatedEndTime)
		                                      : EstimatedEndTime;
	M_CooldownFill.CooldownDuration = static_cast<float>(CooldownDuration);
	M_CooldownFill.bIsCoolingDown = true;
	ApplyCooldownFillAtTime(NowSeconds);
}

void UW_ItemActionUI::StopCooldownFill()
{
	M_CooldownFill.bIsCoolingDown = false;
	M_CooldownFill.CooldownEndWorldTime = 0.0;
	M_CooldownFill.CooldownDuration = 0.f;
	SetGlassPaneCooldownRemaining(0.f);
}

void UW_ItemActionUI::ApplyCooldownFillAtTime(const double WorldTimeSeconds)
{
	if (M_CooldownFill.CooldownDuration <= 0.f)
	{
		return;
	}
	const double SecondsLeft = M_CooldownFill.CooldownEndWorldTime - WorldTimeSeconds;
	// Hold at full until the command data reports the ability ready; the next UI update stops the fill.
	const float RemainingFraction = FMath::Clamp(
		static_cast<float>(SecondsLeft / M_CooldownFill.CooldownDuration), 0.f, 1.f);
	SetGlassPaneCooldownRemaining(RemainingFraction);
}

void UW_ItemActionUI::SetGlassPaneCooldownRemaining(const float RemainingFraction)
{
	// Reaching exactly 0 is always written so a finished cooldown never leaves a sliver of shade.
	const bool bIsUnchanged = RemainingFraction == M_CooldownFill.AppliedRemainingFraction
		|| (RemainingFraction > 0.f && FMath::Abs(RemainingFraction - M_CooldownFill.AppliedRemainingFraction)
			< ItemActionUICooldownFill::MinFractionChange);
	if (bIsUnchanged)
	{
		return;
	}
	// Ready panes without a dynamic instance already show no cooldown.
	if (RemainingFraction <= 0.f && not IsValid(M_GlassPaneMaterial))
	{
		M_CooldownFill.AppliedRemainingFraction = 0.f;
		return;
	}
	UMaterialInstanceDynamic* GlassPaneMaterial = GetGlassPaneMaterial();
	if (not IsValid(GlassPaneMaterial))
	{
		return;
	}
	GlassPaneMaterial->SetScalarParameterValue(ItemActionUICooldownFill::CooldownRemainingParameter, RemainingFraction);
	M_CooldownFill.AppliedRemainingFraction = RemainingFraction;
}

UMaterialInstanceDynamic* UW_ItemActionUI::GetGlassPaneMaterial()
{
	if (IsValid(M_GlassPaneMaterial))
	{
		return M_GlassPaneMaterial;
	}
	// Optional: widgets without the glass pane simply show no cooldown fill.
	if (not IsValid(M_GlassPane))
	{
		return nullptr;
	}
	M_GlassPaneMaterial = M_GlassPane->GetDynamicMaterial();
	return M_GlassPaneMaterial;
}

void UW_ItemActionUI::HideActionImageWithoutIcon() const
{
	if (not GetIsValidActionItemImage())
	{
		return;
	}

	FSlateBrush Brush = M_ActionItemImage->GetBrush();
	const bool bHasIcon = Brush.GetResourceObject() != nullptr;
	if (bHasIcon)
	{
		if (Brush.DrawAs != ESlateBrushDrawType::NoDrawType)
		{
			return;
		}
		Brush.DrawAs = ESlateBrushDrawType::Image;
		M_ActionItemImage->SetBrush(Brush);
		return;
	}

	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
	{
		return;
	}
	Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
	M_ActionItemImage->SetBrush(Brush);
}

bool UW_ItemActionUI::GetIsValidActionItemImage() const
{
	if (IsValid(M_ActionItemImage))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionItemImage"),
		TEXT("UW_ItemActionUI::GetIsValidActionItemImage"),
		this
	);
	return false;
}

void UW_ItemActionUI::InitActionUIElement(
	ACPPController* PlayerController,
	const int32 IndexActionUIElm, UActionUIManager* ActionUIManager, UW_ActionUIDescription* ActionUIDescription)
{	
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI received an invalid player controller during init.");
		return;
	}

	M_Index = IndexActionUIElm;
	M_PlayerController = PlayerController;
	M_ActionUIManager = ActionUIManager;
	M_ActionUIDescription = ActionUIDescription;

	CacheHotkeyProviderSubsystem();
	UpdateHotkeyFromProvider();
	BindHotkeyUpdateDelegate();
}

void UW_ItemActionUI::SetActionButtonHotkeyHidden(const bool bHideActionButtonHotkey) const
{
	if (not GetIsValidActionItemHotKey())
	{
		return;
	}

	const ESlateVisibility NewVisibility = bHideActionButtonHotkey ? ESlateVisibility::Hidden : ESlateVisibility::Visible;
	M_ActionItemHotKey->SetVisibility(NewVisibility);
}

void UW_ItemActionUI::OnActionUIClicked()
{
	if (GetIsValidPlayerController())
	{
		M_PlayerController->ActivateActionButton(M_Index);
	}
}

void UW_ItemActionUI::OnActionUIHover(const bool bIsHover) const
{
	if (GetIsValidActionUIManager())
	{
		M_ActionUIManager->OnHoverActionUIItem(bIsHover, M_Ability, M_CustomType);
	}
}

bool UW_ItemActionUI::GetIsValidPlayerController()
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	if (APlayerController* NewController = UGameplayStatics::GetPlayerController(this, 0);
		IsValid(NewController))
	{
		if (ACPPController* Pc = Cast<ACPPController>(NewController))
		{
			if (IsValid(Pc))
			{
				M_PlayerController = Pc;
				return true;
			}
		}
	}
	RTSFunctionLibrary::ReportError("Not able to find valid controller for UW_ItemActionUI");
	return false;
}

bool UW_ItemActionUI::GetIsValidActionUIManager() const
{
	if (not M_ActionUIManager.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_ActionUIManager"),
			TEXT("UW_ItemActionUI::GetIsValidActionUIManager"),
			this
		);
		return false;
	}
	return true;
}

void UW_ItemActionUI::NativeDestruct()
{
	UnbindHotkeyUpdateDelegate();
	Super::NativeDestruct();
}

UW_ActionUIDescription* UW_ItemActionUI::GetActionUIDescription() const
{
	if (M_ActionUIDescription.IsValid())
	{
		return M_ActionUIDescription.Get();
	}
	RTSFunctionLibrary::ReportError("No valid action Ui description for item: " + GetName());
	return nullptr;
}

void UW_ItemActionUI::UpdateButtonWithGlobalSlateStyle()
{
	if (ButtonStyleAsset)
	{
		const FButtonStyle* ButtonStyle = ButtonStyleAsset->GetStyle<FButtonStyle>();
		if (ButtonStyle && M_ActionItemButton)
		{
			M_ActionItemButton->SetStyle(*ButtonStyle);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("ButtonStyle null."
			"\n at widget: " + GetName() +
			"\n Forgot to set style reference in UW_ItemActionUI::UpdateButtonWithGlobalSlateStyle?");
	}
}

void UW_ItemActionUI::CacheHotkeyProviderSubsystem()
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = M_PlayerController->GetLocalPlayer();
	if (not IsValid(LocalPlayer))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI could not resolve a local player for hotkey lookup.");
		return;
	}

	URTSHotkeyProviderSubsystem* HotkeyProviderSubsystem = LocalPlayer->GetSubsystem<URTSHotkeyProviderSubsystem>();
	if (not IsValid(HotkeyProviderSubsystem))
	{
		RTSFunctionLibrary::ReportError("UW_ItemActionUI could not resolve the hotkey provider subsystem.");
		return;
	}

	M_HotkeyProviderSubsystem = HotkeyProviderSubsystem;
}

void UW_ItemActionUI::BindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	M_ActionSlotHotkeyHandle = M_HotkeyProviderSubsystem->OnActionSlotHotkeyUpdated().AddUObject(
		this,
		&UW_ItemActionUI::HandleActionSlotHotkeyUpdated
	);
}

void UW_ItemActionUI::UnbindHotkeyUpdateDelegate()
{
	if (not GetIsValidHotkeyProviderSubsystem())
	{
		return;
	}

	if (M_ActionSlotHotkeyHandle.IsValid())
	{
		M_HotkeyProviderSubsystem->OnActionSlotHotkeyUpdated().Remove(M_ActionSlotHotkeyHandle);
		M_ActionSlotHotkeyHandle.Reset();
	}
}

void UW_ItemActionUI::UpdateHotkeyFromProvider()
{
	if (not GetIsValidHotkeyProviderSubsystem() || not GetIsValidActionItemHotKey())
	{
		return;
	}

	const FText HotkeyText = M_HotkeyProviderSubsystem->GetDisplayKeyForActionSlot(M_Index);
	M_ActionItemHotKey->SetKeyText(HotkeyText);
}

void UW_ItemActionUI::HandleActionSlotHotkeyUpdated(const int32 ActionSlotIndex, const FText& HotkeyText)
{
	if (ActionSlotIndex != M_Index)
	{
		return;
	}

	if (not GetIsValidActionItemHotKey())
	{
		return;
	}

	M_ActionItemHotKey->SetKeyText(HotkeyText);
}

bool UW_ItemActionUI::GetIsValidActionItemHotKey() const
{
	if (IsValid(M_ActionItemHotKey))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ActionItemHotKey"),
		TEXT("UW_ItemActionUI::GetIsValidActionItemHotKey"),
		this
	);
	return false;
}

bool UW_ItemActionUI::GetIsValidHotkeyProviderSubsystem() const
{
	if (M_HotkeyProviderSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_HotkeyProviderSubsystem"),
		TEXT("UW_ItemActionUI::GetIsValidHotkeyProviderSubsystem"),
		this
	);
	return false;
}
