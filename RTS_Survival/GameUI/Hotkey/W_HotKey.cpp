// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_HotKey.h"

#include "Components/RichTextBlock.h"
#include "Engine/LocalPlayer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyProviderSubsystem.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_HotKey::SetKeyText(const FText& NewKeyText)
{
	if (not GetIsValidKeyText())
	{
		return;
	}

	M_KeyText->SetText(NewKeyText);
}

void UW_HotKey::ShowChordedAction(const FName& ActionName, const FString& ModifierStyle, const FString& KeyStyle)
{
	M_ChordedActionName = ActionName;
	M_ChordedModifierStyle = ModifierStyle;
	M_ChordedKeyStyle = KeyStyle;
	BindChordedActionUpdates();
	RefreshChordedActionText();
}

void UW_HotKey::NativeConstruct()
{
	Super::NativeConstruct();
	if (M_ChordedActionName.IsNone())
	{
		return;
	}

	BindChordedActionUpdates();
	RefreshChordedActionText();
}

void UW_HotKey::NativeDestruct()
{
	UnbindChordedActionUpdates();
	Super::NativeDestruct();
}

bool UW_HotKey::GetIsValidKeyText() const
{
	if (IsValid(M_KeyText))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_KeyText"),
		TEXT("UW_HotKey::GetIsValidKeyText"),
		this
	);

	return false;
}

void UW_HotKey::BindChordedActionUpdates()
{
	if (M_ChordedActionHotkeyHandle.IsValid())
	{
		return;
	}

	// Widgets built for tests or the commander preview have no local player: they simply show no hotkey.
	const ULocalPlayer* OwningLocalPlayer = GetOwningLocalPlayer();
	if (not IsValid(OwningLocalPlayer))
	{
		return;
	}

	URTSHotkeyProviderSubsystem* HotkeyProviderSubsystem = OwningLocalPlayer->GetSubsystem<URTSHotkeyProviderSubsystem>();
	if (not IsValid(HotkeyProviderSubsystem))
	{
		return;
	}

	M_HotkeyProviderSubsystem = HotkeyProviderSubsystem;
	M_ChordedActionHotkeyHandle = HotkeyProviderSubsystem->OnChordedActionHotkeyUpdated().AddUObject(
		this,
		&UW_HotKey::HandleChordedActionHotkeyUpdated);
}

void UW_HotKey::UnbindChordedActionUpdates()
{
	if (not M_ChordedActionHotkeyHandle.IsValid())
	{
		return;
	}

	if (M_HotkeyProviderSubsystem.IsValid())
	{
		M_HotkeyProviderSubsystem->OnChordedActionHotkeyUpdated().Remove(M_ChordedActionHotkeyHandle);
	}
	M_ChordedActionHotkeyHandle.Reset();
}

void UW_HotKey::HandleChordedActionHotkeyUpdated(const FName ActionName, const FText& HotkeyText)
{
	if (ActionName != M_ChordedActionName)
	{
		return;
	}

	RefreshChordedActionText();
}

void UW_HotKey::RefreshChordedActionText()
{
	if (M_ChordedActionName.IsNone() || not GetIsValidKeyText())
	{
		return;
	}

	FRTSModifierHotkey Hotkey;
	const ACPPController* RTSController = Cast<ACPPController>(GetOwningPlayer());
	if (IsValid(RTSController) && not RTSController->GetIsMainMenuController())
	{
		Hotkey = RTSController->GetChordedHotkeyForAction(M_ChordedActionName);
	}

	if (not Hotkey.GetIsValid())
	{
		// Unbound in the key bindings menu (or no RTS controller): no empty badge over the button.
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	M_KeyText->SetText(RTSHotkeyTypes::GetChordedHotkeyRichText(Hotkey, M_ChordedModifierStyle, M_ChordedKeyStyle));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
