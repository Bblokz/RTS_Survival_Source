// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

enum class EWeaponName : uint32;
enum class EWeaponFireMode : uint8;

#if RTS_WITH_MAP_TESTS
/**
 * Map tests only: per batched weapon and fire mode, how many rounds were fired and how many muzzle flashes were handed
 * to the batcher, so a bench run shows that every round flashes (a SingleBurst used to flash once per burst).
 */
namespace SmallArmsMuzzleRoundAudit
{
	void NoteRoundFired(const EWeaponName WeaponName, const EWeaponFireMode FireMode);
	void NoteFlashDrawn(const EWeaponName WeaponName, const EWeaponFireMode FireMode);
	// One "weapon/mode rounds=N flashes=M" entry per weapon and fire mode seen.
	FString Describe();
}
#endif
