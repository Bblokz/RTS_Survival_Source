#pragma once


#include "CoreMinimal.h"


UENUM(Blueprintable)
enum class EShowBottomRightUIPanel : uint8
{
	Show_None,
	Show_ActionUI,
	Show_TrainingDescription,
	Show_BxpDescription
};
