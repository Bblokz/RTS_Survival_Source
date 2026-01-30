// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "RTSHotkeyProviderSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSHotkeyProviderConstants
{
	constexpr int32 ActionButtonIndexOffset = 1;
	constexpr int32 ControlGroupIndexOffset = 1;
	constexpr int32 MaxActionSlots = 15;
	constexpr int32 MaxControlGroupSlots = 10;
	const TCHAR* ActionButtonPrefix = TEXT("IA_ActionButton");
	const TCHAR* ControlGroupPrefix = TEXT("IA_ControlGroup");
}

namespace RTSHotkeyProviderStatics
{
	FName BuildActionName(const TCHAR* Prefix, const int32 IndexValue)
	{
		return FName(*FString::Printf(TEXT("%s%d"), Prefix, IndexValue));
	}
}

FText URTSHotkeyProviderSubsystem::GetDisplayKeyForActionSlot(const int32 ActionSlotIndex) const
{
	const UInputAction* Action = GetInputActionForActionSlot(ActionSlotIndex);
	return GetDisplayKeyForAction(Action);
}

FText URTSHotkeyProviderSubsystem::GetDisplayKeyForControlGroupSlot(const int32 ControlGroupIndex) const
{
	const UInputAction* Action = GetInputActionForControlGroupSlot(ControlGroupIndex);
	return GetDisplayKeyForAction(Action);
}

FOnActionSlotHotkeyUpdated& URTSHotkeyProviderSubsystem::OnActionSlotHotkeyUpdated()
{
	return M_OnActionSlotHotkeyUpdated;
}

FOnControlGroupHotkeyUpdated& URTSHotkeyProviderSubsystem::OnControlGroupHotkeyUpdated()
{
	return M_OnControlGroupHotkeyUpdated;
}

void URTSHotkeyProviderSubsystem::HandleKeyBindingChanged(UInputAction* UpdatedAction)
{
	if (not IsValid(UpdatedAction))
	{
		RTSFunctionLibrary::ReportError(TEXT("Hotkey provider received an invalid input action update."));
		return;
	}

	BroadcastActionSlotHotkeysForAction(UpdatedAction);
	BroadcastControlGroupHotkeysForAction(UpdatedAction);
}

void URTSHotkeyProviderSubsystem::RefreshAllHotkeys()
{
	for (int32 ActionSlotIndex = 0; ActionSlotIndex < RTSHotkeyProviderConstants::MaxActionSlots; ++ActionSlotIndex)
	{
		BroadcastActionSlotHotkey(ActionSlotIndex);
	}

	for (int32 ControlGroupIndex = 0;
	     ControlGroupIndex < RTSHotkeyProviderConstants::MaxControlGroupSlots;
	     ++ControlGroupIndex)
	{
		BroadcastControlGroupHotkey(ControlGroupIndex);
	}
}

UInputMappingContext* URTSHotkeyProviderSubsystem::GetDefaultMappingContext() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (not IsValid(LocalPlayer))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Hotkey provider failed to resolve local player for mapping context."));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Hotkey provider failed to resolve world for mapping context."));
		return nullptr;
	}

	APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Hotkey provider failed to resolve player controller for mappings."));
		return nullptr;
	}

	const ACPPController* RtsController = Cast<ACPPController>(PlayerController);
	if (not IsValid(RtsController))
	{
		RTSFunctionLibrary::ReportError(TEXT("Hotkey provider failed to resolve RTS controller for mappings."));
		return nullptr;
	}

	return RtsController->GetDefaultInputMappingContext();
}

UInputAction* URTSHotkeyProviderSubsystem::FindActionByName(const FName& ActionName) const
{
	UInputMappingContext* MappingContext = GetDefaultMappingContext();
	if (MappingContext == nullptr)
	{
		return nullptr;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (not IsValid(Mapping.Action))
		{
			continue;
		}

		if (Mapping.Action->GetFName() == ActionName)
		{
			return const_cast<UInputAction*>(Mapping.Action.Get());
		}
	}

	return nullptr;
}

UInputAction* URTSHotkeyProviderSubsystem::GetInputActionForActionSlot(const int32 ActionSlotIndex) const
{
	const bool bUsesZeroBasedIndexing = GetUsesZeroBasedActionButtonIndexing();
	const int32 ActionNameIndex = bUsesZeroBasedIndexing
		? ActionSlotIndex
		: ActionSlotIndex + RTSHotkeyProviderConstants::ActionButtonIndexOffset;

	const FName ActionName = RTSHotkeyProviderStatics::BuildActionName(
		RTSHotkeyProviderConstants::ActionButtonPrefix,
		ActionNameIndex);
	return FindActionByName(ActionName);
}

UInputAction* URTSHotkeyProviderSubsystem::GetInputActionForControlGroupSlot(const int32 ControlGroupIndex) const
{
	const bool bUsesZeroBasedIndexing = GetUsesZeroBasedControlGroupIndexing();
	const int32 InputActionIndex = bUsesZeroBasedIndexing
		? ControlGroupIndex
		: ControlGroupIndex + RTSHotkeyProviderConstants::ControlGroupIndexOffset;
	const FName ActionName = RTSHotkeyProviderStatics::BuildActionName(
		RTSHotkeyProviderConstants::ControlGroupPrefix,
		InputActionIndex);
	return FindActionByName(ActionName);
}

bool URTSHotkeyProviderSubsystem::GetUsesZeroBasedActionButtonIndexing() const
{
	const FName ZeroBasedName = RTSHotkeyProviderStatics::BuildActionName(
		RTSHotkeyProviderConstants::ActionButtonPrefix,
		0);
	return FindActionByName(ZeroBasedName) != nullptr;
}

bool URTSHotkeyProviderSubsystem::GetUsesZeroBasedControlGroupIndexing() const
{
	const FName ZeroBasedName = RTSHotkeyProviderStatics::BuildActionName(
		RTSHotkeyProviderConstants::ControlGroupPrefix,
		0);
	return FindActionByName(ZeroBasedName) != nullptr;
}

FText URTSHotkeyProviderSubsystem::GetDisplayKeyForAction(const UInputAction* Action) const
{
	if (not IsValid(Action))
	{
		return FText::GetEmpty();
	}

	UInputMappingContext* MappingContext = GetDefaultMappingContext();
	if (MappingContext == nullptr)
	{
		return FText::GetEmpty();
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	FKey PrimaryKey;
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (Mapping.Action.Get() != Action || not Mapping.Key.IsValid())
		{
			continue;
		}

		if (not PrimaryKey.IsValid())
		{
			PrimaryKey = Mapping.Key;
			continue;
		}

		if (PrimaryKey.IsGamepadKey() && not Mapping.Key.IsGamepadKey())
		{
			PrimaryKey = Mapping.Key;
		}
	}

	return PrimaryKey.IsValid() ? PrimaryKey.GetDisplayName() : FText::GetEmpty();
}

void URTSHotkeyProviderSubsystem::BroadcastActionSlotHotkey(const int32 ActionSlotIndex)
{
	const FText HotkeyText = GetDisplayKeyForActionSlot(ActionSlotIndex);
	M_OnActionSlotHotkeyUpdated.Broadcast(ActionSlotIndex, HotkeyText);
}

void URTSHotkeyProviderSubsystem::BroadcastControlGroupHotkey(const int32 ControlGroupIndex)
{
	const FText HotkeyText = GetDisplayKeyForControlGroupSlot(ControlGroupIndex);
	M_OnControlGroupHotkeyUpdated.Broadcast(ControlGroupIndex, HotkeyText);
}

void URTSHotkeyProviderSubsystem::BroadcastActionSlotHotkeysForAction(const UInputAction* UpdatedAction)
{
	for (int32 ActionSlotIndex = 0; ActionSlotIndex < RTSHotkeyProviderConstants::MaxActionSlots; ++ActionSlotIndex)
	{
		if (GetInputActionForActionSlot(ActionSlotIndex) == UpdatedAction)
		{
			BroadcastActionSlotHotkey(ActionSlotIndex);
		}
	}
}

void URTSHotkeyProviderSubsystem::BroadcastControlGroupHotkeysForAction(const UInputAction* UpdatedAction)
{
	for (int32 ControlGroupIndex = 0;
	     ControlGroupIndex < RTSHotkeyProviderConstants::MaxControlGroupSlots;
	     ++ControlGroupIndex)
	{
		if (GetInputActionForControlGroupSlot(ControlGroupIndex) == UpdatedAction)
		{
			BroadcastControlGroupHotkey(ControlGroupIndex);
		}
	}
}
