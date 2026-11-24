// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SmallArmsMuzzleRoundAudit.h"

#if RTS_WITH_MAP_TESTS
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

namespace SmallArmsMuzzleRoundAudit
{
	namespace
	{
		struct FRoundAuditCounts
		{
			int64 RoundsFired = 0;
			int64 FlashesDrawn = 0;
		};

		// Game thread only; weapons fire from timers.
		TMap<FString, FRoundAuditCounts>& GetCounts()
		{
			static TMap<FString, FRoundAuditCounts> Counts;
			return Counts;
		}

		FString MakeKey(const EWeaponName WeaponName, const EWeaponFireMode FireMode)
		{
			return Global_GetWeaponEnumAsString(WeaponName) + TEXT("/") + UEnum::GetDisplayValueAsText(FireMode).ToString();
		}
	}

	void NoteRoundFired(const EWeaponName WeaponName, const EWeaponFireMode FireMode)
	{
		GetCounts().FindOrAdd(MakeKey(WeaponName, FireMode)).RoundsFired++;
	}

	void NoteFlashDrawn(const EWeaponName WeaponName, const EWeaponFireMode FireMode)
	{
		GetCounts().FindOrAdd(MakeKey(WeaponName, FireMode)).FlashesDrawn++;
	}

	FString Describe()
	{
		FString Description;
		for (const TPair<FString, FRoundAuditCounts>& Entry : GetCounts())
		{
			Description += FString::Printf(
				TEXT("%s rounds=%lld flashes=%lld; "),
				*Entry.Key,
				Entry.Value.RoundsFired,
				Entry.Value.FlashesDrawn);
		}
		return Description;
	}
}
#endif
