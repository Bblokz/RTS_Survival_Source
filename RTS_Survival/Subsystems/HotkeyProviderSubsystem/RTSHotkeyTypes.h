// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSHotkeyTypes.generated.h"

class UInputAction;
class UInputMappingContext;

UENUM(BlueprintType)
enum class ERTSHotkeyModifier : uint8
{
	Control,
	Shift,
	Alt,
	Invalid
};

USTRUCT(BlueprintType)
struct FRTSModifierHotkey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSHotkeyModifier Modifier = ERTSHotkeyModifier::Invalid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey Key;

	bool GetIsValid() const;
	bool operator==(const FRTSModifierHotkey& Other) const;
};

USTRUCT(BlueprintType)
struct FRTSSavedKeyBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName ContextName = NAME_None;

	UPROPERTY()
	FName ActionName = NAME_None;

	UPROPERTY()
	FKey Key;
};

USTRUCT(BlueprintType)
struct FRTSSavedChordedKeyBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FName ContextName = NAME_None;

	UPROPERTY()
	FName ActionName = NAME_None;

	UPROPERTY()
	FRTSModifierHotkey Hotkey;
};

namespace RTSHotkeyTypes
{
	const FName ModifierControlActionName = TEXT("IA_Modifier_Control");
	const FName ModifierShiftActionName = TEXT("IA_Modifier_Shift");
	const FName ModifierAltActionName = TEXT("IA_Modifier_Alt");
	const FName NomadicExpansionActionName = TEXT("IA_NomadicExpansion");
	// Chorded actions IA_GlobalAbility1..8 (default Ctrl+A/S/D/F/Z/X/C/V) fire the matching global ability panel slot.
	constexpr int32 GlobalAbilityHotkeySlotCount = 8;
	// Chorded actions IA_BuildingExpansion1..10 (default Alt+Q/W/E/R/T/A/S/D/F/G) click the matching bxp item.
	constexpr int32 BuildingExpansionHotkeySlotCount = 10;

	bool GetIsModifierActionName(const FName& ActionName);
	bool GetIsControlGroupActionName(const FName& ActionName);
	bool GetIsActionButtonActionName(const FName& ActionName);
	bool GetIsNumberKey(const FKey& Key);
	FName GetModifierActionName(const ERTSHotkeyModifier Modifier);
	ERTSHotkeyModifier GetModifierFromActionName(const FName& ModifierActionName);
	FString GetModifierDisplayString(const ERTSHotkeyModifier Modifier);
	FText GetChordedHotkeyDisplayText(const FRTSModifierHotkey& Hotkey);
	FString GetActionDisplayName(const FName& ActionName);
	/** @return IA_GlobalAbility<SlotIndex + 1> for a zero-based panel slot, NAME_None when out of range. */
	FName GetGlobalAbilityActionName(const int32 SlotIndex);
	/** @return The zero-based panel slot of an IA_GlobalAbility<N> action, INDEX_NONE for any other action. */
	int32 GetGlobalAbilitySlotFromActionName(const FName& ActionName);
	/** @return IA_BuildingExpansion<SlotIndex + 1> for a zero-based bxp item slot, NAME_None when out of range. */
	FName GetBuildingExpansionActionName(const int32 SlotIndex);
	/** @return The zero-based bxp item slot of an IA_BuildingExpansion<N> action, INDEX_NONE for any other action. */
	int32 GetBuildingExpansionSlotFromActionName(const FName& ActionName);
	/**
	 * @brief Badge text for a chorded hotkey: the modifier and the key each in their own rich text style ("CTRL A").
	 * @param Hotkey The modifier + key pair; an invalid (unbound) hotkey gives empty text.
	 * @param ModifierStyle Rich text row name for the modifier part.
	 * @param KeyStyle Rich text row name for the key part.
	 * @return Rich text markup, or empty text when the hotkey is unbound.
	 */
	FText GetChordedHotkeyRichText(const FRTSModifierHotkey& Hotkey, const FString& ModifierStyle, const FString& KeyStyle);
}
