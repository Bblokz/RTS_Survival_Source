// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "RTS_Survival/Subsystems/HotkeyProviderSubsystem/RTSHotkeyTypes.h"
#include "RTSHotkeyProviderSubsystem.generated.h"

class UInputAction;
class UInputMappingContext;
struct FEnhancedActionKeyMapping;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActionSlotHotkeyUpdated, int32, const FText&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnControlGroupHotkeyUpdated, int32, const FText&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnChordedActionHotkeyUpdated, FName, const FText&);

/**
 * @brief Provides UI hotkey display strings per local player and broadcasts updates after binding changes.
 */
UCLASS()
class RTS_SURVIVAL_API URTSHotkeyProviderSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	FText GetDisplayKeyForActionSlot(const int32 ActionSlotIndex) const;
	FText GetDisplayKeyForControlGroupSlot(const int32 ControlGroupIndex) const;
	FText GetDisplayKeyForChordedAction(const FName& ActionName) const;

	FOnActionSlotHotkeyUpdated& OnActionSlotHotkeyUpdated();
	FOnControlGroupHotkeyUpdated& OnControlGroupHotkeyUpdated();
	FOnChordedActionHotkeyUpdated& OnChordedActionHotkeyUpdated();

	void HandleKeyBindingChanged(UInputAction* UpdatedAction);
	void HandleChordedKeyBindingChanged(UInputAction* UpdatedAction);
	void RefreshAllHotkeys();

private:
	FOnActionSlotHotkeyUpdated M_OnActionSlotHotkeyUpdated;
	FOnControlGroupHotkeyUpdated M_OnControlGroupHotkeyUpdated;
	FOnChordedActionHotkeyUpdated M_OnChordedActionHotkeyUpdated;

	UInputMappingContext* GetDefaultMappingContext() const;
	UInputMappingContext* GetChordedActionMappingContext() const;
	UInputAction* FindActionByName(const FName& ActionName) const;
	UInputAction* GetInputActionForActionSlot(const int32 ActionSlotIndex) const;
	UInputAction* GetInputActionForControlGroupSlot(const int32 ControlGroupIndex) const;
	bool GetUsesZeroBasedActionButtonIndexing() const;
	bool GetUsesZeroBasedControlGroupIndexing() const;
	FText GetDisplayKeyForAction(const UInputAction* Action) const;
	FText GetDisplayKeyForChordedAction(const UInputAction* Action) const;
	bool TryGetChordedHotkeyFromMapping(const FEnhancedActionKeyMapping& Mapping, FRTSModifierHotkey& OutHotkey) const;

	void BroadcastActionSlotHotkey(const int32 ActionSlotIndex);
	void BroadcastControlGroupHotkey(const int32 ControlGroupIndex);
	void BroadcastActionSlotHotkeysForAction(const UInputAction* UpdatedAction);
	void BroadcastControlGroupHotkeysForAction(const UInputAction* UpdatedAction);
};
