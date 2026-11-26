// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "TacticalIconCatalog.generated.h"

class UTexture2D;

/** One tactical icon as engraved on the unit portraits: the placard badge on its own and a one-line explanation. */
USTRUCT(BlueprintType)
struct FTacticalIconInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TacticalIcon")
	TObjectPtr<UTexture2D> BadgeTexture = nullptr;

	// Text before a ':' is highlighted; a line without ':' is highlighted as a whole.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TacticalIcon")
	FText Description;
};

/** The icons engraved on one portrait texture; NAME_None for an empty corner. */
USTRUCT(BlueprintType)
struct FTacticalPortraitIcons
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TacticalIcon")
	FName LeftIcon = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TacticalIcon")
	FName RightIcon = NAME_None;
};

/**
 * @brief Which tactical icons each unit portrait carries, so UI can show them next to the portrait with an
 * explanation. Generated from the placard plans (Source/Tools/TacticalIconBackgrounds/tactical_icon_catalog.py +
 * fill_tactical_icon_catalog.py); do not edit by hand, rerun those instead.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UTacticalIconCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * @param PortraitResource The resource object of the portrait brush (a texture).
	 * @param OutIcons Left corner first; empty when the portrait has no tactical icons.
	 */
	void GetIconsForPortrait(const UObject* PortraitResource, TArray<const FTacticalIconInfo*>& OutIcons) const;

protected:
	// Key: icon spec from the placard plans (icon stem, or stem@colour when the same glyph has another meaning).
	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	TMap<FName, FTacticalIconInfo> Icons;

	// Key: package path of the portrait texture, e.g. /Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/MediumTanks/T_Ger_MediumTank_PZIIIAA.
	UPROPERTY(EditAnywhere, Category = "TacticalIcon")
	TMap<FName, FTacticalPortraitIcons> Portraits;

private:
	void AddIcon(const FName IconSpec, const FName PortraitName, TArray<const FTacticalIconInfo*>& OutIcons) const;
};
