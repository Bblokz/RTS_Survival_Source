// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "RTSHotkeyTypes.h"

#include "InputCoreTypes.h"

namespace RTSHotkeyTypesPrivate
{
	const TCHAR* ActionButtonPrefix = TEXT("IA_ActionButton");
	const TCHAR* ControlGroupPrefix = TEXT("IA_ControlGroup");
	const TCHAR* GlobalAbilityPrefix = TEXT("IA_GlobalAbility");
	const TCHAR* BuildingExpansionPrefix = TEXT("IA_BuildingExpansion");
}

bool FRTSModifierHotkey::GetIsValid() const
{
	return Modifier != ERTSHotkeyModifier::Invalid && Key.IsValid();
}

bool FRTSModifierHotkey::operator==(const FRTSModifierHotkey& Other) const
{
	return Modifier == Other.Modifier && Key == Other.Key;
}

bool RTSHotkeyTypes::GetIsModifierActionName(const FName& ActionName)
{
	return ActionName == ModifierControlActionName
		|| ActionName == ModifierShiftActionName
		|| ActionName == ModifierAltActionName;
}

bool RTSHotkeyTypes::GetIsControlGroupActionName(const FName& ActionName)
{
	return ActionName.ToString().StartsWith(RTSHotkeyTypesPrivate::ControlGroupPrefix);
}

bool RTSHotkeyTypes::GetIsActionButtonActionName(const FName& ActionName)
{
	return ActionName.ToString().StartsWith(RTSHotkeyTypesPrivate::ActionButtonPrefix);
}

bool RTSHotkeyTypes::GetIsNumberKey(const FKey& Key)
{
	return Key == EKeys::Zero
		|| Key == EKeys::One
		|| Key == EKeys::Two
		|| Key == EKeys::Three
		|| Key == EKeys::Four
		|| Key == EKeys::Five
		|| Key == EKeys::Six
		|| Key == EKeys::Seven
		|| Key == EKeys::Eight
		|| Key == EKeys::Nine
		|| Key == EKeys::NumPadZero
		|| Key == EKeys::NumPadOne
		|| Key == EKeys::NumPadTwo
		|| Key == EKeys::NumPadThree
		|| Key == EKeys::NumPadFour
		|| Key == EKeys::NumPadFive
		|| Key == EKeys::NumPadSix
		|| Key == EKeys::NumPadSeven
		|| Key == EKeys::NumPadEight
		|| Key == EKeys::NumPadNine;
}

FName RTSHotkeyTypes::GetModifierActionName(const ERTSHotkeyModifier Modifier)
{
	switch (Modifier)
	{
	case ERTSHotkeyModifier::Control:
		return ModifierControlActionName;
	case ERTSHotkeyModifier::Shift:
		return ModifierShiftActionName;
	case ERTSHotkeyModifier::Alt:
		return ModifierAltActionName;
	default:
		return NAME_None;
	}
}

ERTSHotkeyModifier RTSHotkeyTypes::GetModifierFromActionName(const FName& ModifierActionName)
{
	if (ModifierActionName == ModifierControlActionName)
	{
		return ERTSHotkeyModifier::Control;
	}

	if (ModifierActionName == ModifierShiftActionName)
	{
		return ERTSHotkeyModifier::Shift;
	}

	if (ModifierActionName == ModifierAltActionName)
	{
		return ERTSHotkeyModifier::Alt;
	}

	return ERTSHotkeyModifier::Invalid;
}

FString RTSHotkeyTypes::GetModifierDisplayString(const ERTSHotkeyModifier Modifier)
{
	switch (Modifier)
	{
	case ERTSHotkeyModifier::Control:
		return TEXT("Ctrl");
	case ERTSHotkeyModifier::Shift:
		return TEXT("Shft");
	case ERTSHotkeyModifier::Alt:
		return TEXT("Alt");
	default:
		return FString();
	}
}

FText RTSHotkeyTypes::GetChordedHotkeyDisplayText(const FRTSModifierHotkey& Hotkey)
{
	if (not Hotkey.GetIsValid())
	{
		return FText::GetEmpty();
	}

	const FString DisplayText = GetModifierDisplayString(Hotkey.Modifier) + Hotkey.Key.GetDisplayName().ToString();
	return FText::FromString(DisplayText);
}

FString RTSHotkeyTypes::GetActionDisplayName(const FName& ActionName)
{
	FString ActionLabel = ActionName.ToString();
	const FString Prefix = TEXT("IA_");
	if (ActionLabel.StartsWith(Prefix))
	{
		ActionLabel.RightChopInline(Prefix.Len());
	}

	return ActionLabel;
}

namespace RTSHotkeyTypesPrivate
{
	FName GetSlotActionName(const TCHAR* Prefix, const int32 SlotIndex, const int32 SlotCount)
	{
		if (SlotIndex < 0 || SlotIndex >= SlotCount)
		{
			return NAME_None;
		}

		return FName(*FString::Printf(TEXT("%s%d"), Prefix, SlotIndex + 1));
	}

	int32 GetSlotFromActionName(const TCHAR* Prefix, const FName& ActionName, const int32 SlotCount)
	{
		const FString ActionLabel = ActionName.ToString();
		const FString PrefixString = Prefix;
		if (not ActionLabel.StartsWith(PrefixString))
		{
			return INDEX_NONE;
		}

		const FString NumberPart = ActionLabel.RightChop(PrefixString.Len());
		if (NumberPart.IsEmpty() || not NumberPart.IsNumeric())
		{
			return INDEX_NONE;
		}

		const int32 SlotIndex = FCString::Atoi(*NumberPart) - 1;
		return SlotIndex >= 0 && SlotIndex < SlotCount ? SlotIndex : INDEX_NONE;
	}
}

FName RTSHotkeyTypes::GetGlobalAbilityActionName(const int32 SlotIndex)
{
	return RTSHotkeyTypesPrivate::GetSlotActionName(
		RTSHotkeyTypesPrivate::GlobalAbilityPrefix, SlotIndex, GlobalAbilityHotkeySlotCount);
}

int32 RTSHotkeyTypes::GetGlobalAbilitySlotFromActionName(const FName& ActionName)
{
	return RTSHotkeyTypesPrivate::GetSlotFromActionName(
		RTSHotkeyTypesPrivate::GlobalAbilityPrefix, ActionName, GlobalAbilityHotkeySlotCount);
}

FName RTSHotkeyTypes::GetBuildingExpansionActionName(const int32 SlotIndex)
{
	return RTSHotkeyTypesPrivate::GetSlotActionName(
		RTSHotkeyTypesPrivate::BuildingExpansionPrefix, SlotIndex, BuildingExpansionHotkeySlotCount);
}

int32 RTSHotkeyTypes::GetBuildingExpansionSlotFromActionName(const FName& ActionName)
{
	return RTSHotkeyTypesPrivate::GetSlotFromActionName(
		RTSHotkeyTypesPrivate::BuildingExpansionPrefix, ActionName, BuildingExpansionHotkeySlotCount);
}

FText RTSHotkeyTypes::GetChordedHotkeyRichText(
	const FRTSModifierHotkey& Hotkey, const FString& ModifierStyle, const FString& KeyStyle)
{
	if (not Hotkey.GetIsValid())
	{
		return FText::GetEmpty();
	}

	const FString ModifierText = GetModifierDisplayString(Hotkey.Modifier).ToUpper();
	const FString KeyText = Hotkey.Key.GetDisplayName().ToString().ToUpper();
	return FText::FromString(FString::Printf(
		TEXT("<%s>%s</> <%s>%s</>"), *ModifierStyle, *ModifierText, *KeyStyle, *KeyText));
}
