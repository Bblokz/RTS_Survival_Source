// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TacticalIconCatalog.h"

#include "Engine/Texture2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTacticalIconCatalog::GetIconsForPortrait(const UObject* PortraitResource,
                                               TArray<const FTacticalIconInfo*>& OutIcons) const
{
	OutIcons.Reset();
	if (not IsValid(PortraitResource))
	{
		return;
	}
	const FName PortraitName = PortraitResource->GetPackage()->GetFName();
	const FTacticalPortraitIcons* PortraitIcons = Portraits.Find(PortraitName);
	if (not PortraitIcons)
	{
		return;
	}
	AddIcon(PortraitIcons->LeftIcon, PortraitName, OutIcons);
	AddIcon(PortraitIcons->RightIcon, PortraitName, OutIcons);
}

void UTacticalIconCatalog::AddIcon(const FName IconSpec, const FName PortraitName,
                                   TArray<const FTacticalIconInfo*>& OutIcons) const
{
	if (IconSpec.IsNone())
	{
		return;
	}
	const FTacticalIconInfo* Info = Icons.Find(IconSpec);
	if (not Info || not IsValid(Info->BadgeTexture))
	{
		RTSFunctionLibrary::ReportError("Tactical icon " + IconSpec.ToString() + " of portrait "
			+ PortraitName.ToString() + " has no badge texture in " + GetName()
			+ "\n Rerun Source/Tools/TacticalIconBackgrounds/fill_tactical_icon_catalog.py");
		return;
	}
	OutIcons.Add(Info);
}
