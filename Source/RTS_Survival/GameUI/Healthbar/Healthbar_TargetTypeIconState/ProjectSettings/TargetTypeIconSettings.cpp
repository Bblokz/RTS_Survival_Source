// TargetTypeIconSettings.cpp
#include "TargetTypeIconSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/MessageDialog.h"
#include "RTS_Survival/GameUI/Healthbar/Healthbar_TargetTypeIconState/TargetTypeIconState.h"


UTargetTypeIconSettings::UTargetTypeIconSettings()
{
	// Optional: categorize nicely in Project Settings
	CategoryName = TEXT("UI");
	SectionName  = TEXT("Health Bar Icons");
}

void UTargetTypeIconSettings::ResolveTypeToBrushMap(TMap<ETargetTypeIcon, FTargetTypeIconBrushes>& OutMap) const
{
	OutMap.Reset();

	for (const TPair<ETargetTypeIcon, FTargetTypeIconBrushes_Soft>& Pair : TypeToBrush)
	{
		FTargetTypeIconBrushes Resolved;

		// LoadSynchronous() is fine here because settings are read rarely (e.g., on init)
		Resolved.PlayerBrush = Pair.Value.PlayerBrush.IsNull()
			? nullptr
			: Pair.Value.PlayerBrush.LoadSynchronous();

		Resolved.EnemyBrush = Pair.Value.EnemyBrush.IsNull()
			? nullptr
			: Pair.Value.EnemyBrush.LoadSynchronous();

		OutMap.Add(Pair.Key, MoveTemp(Resolved));
	}
}
