#pragma once

#include "CoreMinimal.h"

#include "BuffDebuffType.generated.h"

UENUM()
enum class EBuffDebuffType : uint8
{
	None,
	Neutral,
	Buff,
	Debuff
	
};
