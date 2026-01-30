// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "W_ControlGroupImage.generated.h"

class UW_HotKey;
class URTSHotkeyProviderSubsystem;
class ULocalPlayer;
struct FTrainingOption;
enum class EAllUnitType : uint8;
/**
 * @brief Widget used by the control group bar to show the group icon and hotkey.
 * @note SetupControlGroupIndex: implement in blueprint to update slot-specific visuals.
 * @note OnUpdateControlGroup: implement in blueprint to refresh the displayed unit data.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ControlGroupImage : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Initializes the control group image and hotkey subscription for this slot.
	 * @param OwningLocalPlayer Local player used to access the hotkey provider subsystem.
	 * @param GroupIndex Index of this control group widget in the UI.
	 */
	void InitControlGroupImage(ULocalPlayer* OwningLocalPlayer, const int32 GroupIndex);


	UFUNCTION(BlueprintImplementableEvent)
	void OnUpdateControlGroup(const FTrainingOption MostFrequentUnitInGroup);

protected:
	virtual void NativeDestruct() override;

private:
	int32 M_GroupIndex = 0;

	UPROPERTY()
	TWeakObjectPtr<URTSHotkeyProviderSubsystem> M_HotkeyProviderSubsystem;

	FDelegateHandle M_ControlGroupHotkeyHandle;

	void CacheHotkeyProviderSubsystem(ULocalPlayer* OwningLocalPlayer);
	void BindHotkeyUpdateDelegate();
	void UnbindHotkeyUpdateDelegate();
	void UpdateControlGroupHotkey();
	void HandleControlGroupHotkeyUpdated(const int32 GroupIndex, const FText& HotkeyText);

	bool GetIsValidControlGroupHotKey() const;
	bool GetIsValidHotkeyProviderSubsystem() const;

public:
	// The hotkey of this control group ui item.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UW_HotKey* M_ControlGroupHotKey;
};
