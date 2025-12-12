#pragma once

#include "CoreMinimal.h"

UENUM(Blueprintable)
enum class ECardSound : uint8
{
	Sound_None,
	Sound_Hover,
	Sound_StartDrag,
	Sound_Drop,
	Sound_DropFailed,
	Sound_SwitchLayout,
};