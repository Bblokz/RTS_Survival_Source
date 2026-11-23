// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Owner ids stored on world divisions (AWorldDivisionBase::GetOwningPlayer) and in their save data.
 */
namespace WorldDivisionOwners
{
	constexpr int32 PlayerOwner = 1;
	constexpr int32 EnemyOwner = 2;
}
