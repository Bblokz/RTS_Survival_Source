// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_ControlGroupImage.h"

#include "Engine/LocalPlayer.h"
#include "RTS_Survival/GameUI/Hotkey/W_HotKey.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyProviderSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

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
