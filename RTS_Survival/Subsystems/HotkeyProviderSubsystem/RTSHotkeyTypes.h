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

	bool GetIsModifierActionName(const FName& ActionName);
	bool GetIsControlGroupActionName(const FName& ActionName);
	bool GetIsActionButtonActionName(const FName& ActionName);
	bool GetIsNumberKey(const FKey& Key);
	FName GetModifierActionName(const ERTSHotkeyModifier Modifier);
	ERTSHotkeyModifier GetModifierFromActionName(const FName& ModifierActionName);
	FString GetModifierDisplayString(const ERTSHotkeyModifier Modifier);
	FText GetChordedHotkeyDisplayText(const FRTSModifierHotkey& Hotkey);
	FString GetActionDisplayName(const FName& ActionName);
}
