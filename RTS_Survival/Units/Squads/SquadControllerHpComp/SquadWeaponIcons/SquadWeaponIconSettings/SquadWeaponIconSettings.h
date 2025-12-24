// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Units/Squads/SquadControllerHpComp/SquadWeaponIcons/SquadWeaponIcons.h"
#include "RTS_Survival/Weapons/InfantryWeapon/SecondaryWeaponComp/SecondaryWeapon.h"
#include "SquadWeaponIconSettings.generated.h"
class USlateBrushAsset;

USTRUCT(BlueprintType)
struct FSquadWeaponIconDisplaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USlateBrushAsset> WeaponIconBrush = nullptr;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	FAnchorData CanvasSlotLayoutData;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly)
	FVector2D ImageWidgetSize = FVector2D(480.f, 211.0f);
};

USTRUCT(BlueprintType, Blueprintable)
struct FSquadWeaponIconSettingsData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config)
	TMap<ESquadWeaponIcon, FSquadWeaponIconDisplaySettings> TypeToDisplaySettings;
};

/**
 * 
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Squad Weapon Icons"))
class RTS_SURVIVAL_API USquadWeaponIconSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USquadWeaponIconSettings();

	/** Configure target-type icon brushes and layout overrides globally for the project. */
	UPROPERTY(EditAnywhere, Config, Category="Icons", meta=(ForceInlineRow))
	FSquadWeaponIconSettingsData SquadWeaponIconData;

	/**
	* Convenience accessor.
	*/
	static const USquadWeaponIconSettings* Get()
	{
		return GetDefault<USquadWeaponIconSettings>();
	}

	bool TryGetWeaponIconSettings(const ESquadWeaponIcon WeaponIconType,
	                              FSquadWeaponIconDisplaySettings& OutIconSettings) const;
};

static ESquadWeaponIcon Global_GetWeaponIconForWeapon(const EWeaponName WeaponName)
{
	switch (WeaponName)
	{
	// Rail guns
	case EWeaponName::GerRailGun:
	case EWeaponName::RailGunY:
		return ESquadWeaponIcon::Railgun;
	// Flame weapons
	case EWeaponName::Ashmaker05:
		return ESquadWeaponIcon::AshmakerSmall;
	// Falls through.
	case EWeaponName::Ashmaker11:
	case EWeaponName::Flamm09:
		return ESquadWeaponIcon::AshmakerLarge;

	case EWeaponName::M920_AtSniper:
		return ESquadWeaponIcon::M920Sniper;

	// German small arms
	case EWeaponName::MP46:
		return ESquadWeaponIcon::Mp46SpecialSMG;
	case EWeaponName::STG44_7_92MM:
		return ESquadWeaponIcon::SturmGewehr;
	case EWeaponName::Kar_Sniper:
	case EWeaponName::Mosin_Snip:
		return ESquadWeaponIcon::SniperRifleGer;
	case EWeaponName::FG_42_7_92MM:
		return ESquadWeaponIcon::FG42;

	// Russian small arms
	case EWeaponName::PPSh_41_7_62MM:
		return ESquadWeaponIcon::PPSH;
	case EWeaponName::DP_28_7_62MM:
		return ESquadWeaponIcon::DP28;
	case EWeaponName::Fedrov_Avtomat:
		return ESquadWeaponIcon::Fedrov;

	// Machine guns
	case EWeaponName::MG_34:
		return ESquadWeaponIcon::MG34;
	case EWeaponName::RipperGun_7_62MM:
		return ESquadWeaponIcon::RifleMG;

	// Anti-tank weapons
	case EWeaponName::PanzerFaust:
		return ESquadWeaponIcon::PanzerFaust;
	case EWeaponName::PanzerSchreck:
		return ESquadWeaponIcon::PanzerShrek;
	case EWeaponName::PTRS_41_14_5MM:
		return ESquadWeaponIcon::PTRS;
	case EWeaponName::PTRS_50MM:
		return ESquadWeaponIcon::PTRS50MM;	
	case EWeaponName::PanzerRifle_50mm:
		return ESquadWeaponIcon::PanzerRifle50mm;


	// Laser weapons
	case EWeaponName::SPEKTR_V:
		return ESquadWeaponIcon::SpekterLaser;
	case EWeaponName::LB14:
		return ESquadWeaponIcon::Bm14Laser;

	// Automated weapons
	case EWeaponName::PTRS_X_Tishina:
		return ESquadWeaponIcon::AutomatedPTRS;

	// Default case for weapons without specific icons
	default:
		return ESquadWeaponIcon::None;
	}
}
