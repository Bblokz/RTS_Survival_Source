#pragma once

#include "CoreMinimal.h"

#include "WeaponSystems.generated.h"

/** @brief system used on weapon, can be a direct hit (no collision checks), a trace (instant hit with collision)
* or projectile based (collision checks, no instant hit) */
UENUM(BlueprintType)
enum class EWeaponSystemType : uint8
{
	DirectHit UMETA(DisplayName = "Direct Hit"),
	Trace UMETA(DisplayName = "Trace"),
	Projectile UMETA(DisplayName = "Projectile")
};


UENUM(BlueprintType)
enum class EWeaponFireMode : uint8
{
	// Single shot weapon; cooldown after each shot using PC weaponcooldown.
	Single UMETA(DisplayName = "Single Fire"),
	// Amount of bullets in one burst set with Max burst amount. Creates only one launch effect
	// Cooldown between shots set with burst cooldown.
	SingleBurst UMETA(DisplayName = "Burst Fire"),
	// Fires a random amount of multiple shots in one burst depending on min and max burst amount.
	// Creates a different launch effect for each shot.
	// Cooldown between shots set with burst cooldown.
	RandomBurst UMETA(DisplayName = "RandomBurst Fire")
};

// also fill in Global_GetWeaponDisplayName and Global_GetWeaponEnumAsString (see below)
UENUM(BlueprintType)
enum class
	EWeaponName : uint8
{
	DEFAULT_WEAPON,
	T26_Mg,
	// German small arms
	STG44_7_92MM UMETA(DisplayName = "STG44 7.92MM"),
	MP40_9MM UMETA(DisplayName = "MP40 9MM"),
	Kar_98k UMETA(DisplayName = "Kar98k 7.92MM"),
	Kar_Sniper UMETA(DisplayName = "Kar98k Sniper 7.92MM"),
	M920_AtSniper UMETA(DisplayName = "M920 AT Sniper 20MM"),
	Mauser UMETA(DisplayName = "Mauser 7.92MM"),
	MG_34 UMETA(DisplayName = "MG34 7.92MM"),
	MP40 UMETA(DisplayName = "MP40 9MM"),
	MP46 UMETA(DisplayName = "MP46 9MM"),
	FG_42_7_92MM UMETA(DisplayName = "FG42 7.92MM"),
	Ger_TankMG_7_6MM,
	RipperGun_7_62MM UMETA(DisplayName = "Ripper Gun 7.62MM"),

	// Aircraft weapons
	MG_15 UMETA(DisplayName = "MG15 7.92MM"),
	MG_151 UMETA(DisplayName = "MG151 20MM"),
	BK_5_50MM UMETA(DisplayName = "BK5 50MM"),

	// Aircraft bombs
	Bomb_500Gr UMETA(DisplayName = "500Gr Bomb"),
	Bomb_250Gr UMETA(DisplayName = "250Gr Bomb"),
	Bomb_400Gr UMETA(DisplayName = "400Gr Bomb"),
	Bomb_1000Gr UMETA(DisplayName = "1000Gr Bomb"),
	BombRocket_700Gr UMETA(DisplayName = "700Gr Bomb Rocket"),
	BombRocket_800Gr UMETA(DisplayName = "800Gr Bomb Rocket"),
	BombRocket_1500Gr UMETA(DisplayName = "1500Gr Bomb Rocket"),
	BombRocket_2000Gr UMETA(DisplayName = "2000Gr Bomb Rocket"),
	BombRocket_3000Gr UMETA(DisplayName = "3000Gr Bomb Rocket"),
	BombRocket_5000Gr UMETA(DisplayName = "5000Gr Bomb Rocket"),


	// German At
	PanzerSchreck UMETA(DisplayName = "PanzerSchreck 88MM"),
	PanzerFaust UMETA(DisplayName = "PanzerFaust 44MM"),
	PanzerRifle_50mm UMETA(DisplayName = "PanzerRifle 50MM (rifle grenade launcher)"),

	// German Laser Weapons
	LB14 UMETA(DisplayName = "LB 14 30MM (handheld LaserWeapon"),
	LightStorm UMETA(DisplayName = "LightStorm 30MM (Light Laser MG handheld)"),
	Strahlkanone39 UMETA(DisplayName = "Strahlkanone 39 50MM PZ V/III"),

	// Railgun Weapons
	GerRailGun UMETA(DisplayName = "Ger Railgun 30MM"),

	// German Flame Weapons
	Ashmaker05 UMETA(DisplayName = "ashmaker 05 (Very Light Flame Thrower)"),
	Flamm09 UMETA(DisplayName = "Flamm 09 (Light Flame Thrower)"),
	Ashmaker11 UMETA(DisplayName = "ashmaker 11 (Light-Medium Flame Thrower)"),
	Flamm15 UMETA(DisplayName = "Flamm 15 (Medium Flame Thrower)"),


	// German small calibre
	KwK38_T_37MM UMETA(DisplayName = "KwK38 t 37MM (PZ 38(t))"),
	KwK39_1_50MM UMETA(DisplayName = "KwK39/1 50MM (PZ III M)"),
	KwK39_1_50MM_Puma UMETA(DisplayName = "KwK39/1 50MM (Puma)"),
	KwK42_L_50MM UMETA(DisplayName = "KwK L/42 L 50MM (PZ III J)"),
	KwK37_F_50MM UMETA(DisplayName = "KwK37 75MM (PZ II FLAMM)"),
	Flak38_20MM,
	KwK30_20MM UMETA(DisplayName = "KwK30 20MM (PZ II F)"),
	Kwk30_20MM_sdkfz231 UMETA(DisplayName = "KwK30 20MM (Sdkfz 231)"),
	Kwk31_30MM UMETA(DisplayName = "KwK31 30MM (skdfz 140 3cm)"),
	Kwk32_35MM UMETA(DisplayName = "Kwk32 30MM (Jaguar)"),
	Kwk34_Twin30MM UMETA(DisplayName = "Kwk34 Twin 30MM (PZ III AA)"),
	Kwk37_20MM UMETA(DisplayName = "KwK 37 20MM (Sdkfz 140 2cm)"),
	Flak36_37MM UMETA(DisplayName = "Flak36 37MM (Flak 36)"),


	// German Medium calibre
	Pak_t_L_43_47MM UMETA(DisplayName = "Pak(t) L/43 47MM (PanzerJager I)"),
	KwK40_L48_75MM UMETA(DisplayName = "KwK40 L48 75MM (PZ IV)"),
	Pak40_3_L46_75MM_Sdkfz251 UMETA(DisplayName = "Pak40 3/L46 75MM (Sdkfz 251/22)"),
	Pak39_L_48_75MM UMETA(DisplayName = "Pak39 L/48 75MM (hetzer)"),
	StuK40_L48_75MM UMETA(DisplayName = "StuK40 L48 75MM (Stug III F)"),
	KwK37_75MM UMETA(DisplayName = "KwK37 75MM (PZ IV Ausf F)"),
	KwK42_75MM UMETA(DisplayName = "KwK42 75MM (Panther)"),
	KWK42_75MM_PantherD UMETA(DisplayName = "KwK42 75MM PantherD"),
	Pak38_50MM UMETA(DisplayName = "Pak38 50MM (Pak 38 BXP)"),
	Pak40_3_L46_75MM UMETA(DisplayName = "Pak40 3/L46 75MM (Marder)"),
	KwK44_L_36_5_75MM UMETA(DisplayName = "KwK44 L/36.5 75MM Mause Secondary"),
	KwK44_L_36_5_75MM_E100 UMETA(DisplayName = "KwK44 L/36.5 75MM E100 Secondary"),
	// German Heavy calibre
	KwK36_88MM UMETA(DisplayName = "KwK36 88MM (Tiger)"),
	Kwk36_88MM_TigerH1 UMETA(DisplayName = "KwK36 88MM TigerH1"),
	Flak37_88MM UMETA(DisplaynName = "Flak37_88MM (Flak 88 BXP)"),
	KwK43_88MM UMETA(DisplayName = "Kwk43 88MM (KingTiger)"),
	Pak43_88MM UMETA(DisplayName = "Pak43 88MM (Jagdpanther)"),
	KwK43_88MM_PantherII UMETA(DisplayName = "Kwk43 88MM PantherII"),
	LeFH_18_105MM UMETA(DisplayName = "LeFH 18 105MM (Howitzer)"),
	// German Super Heavy calibre
	KwKL_68_105MM UMETA(DisplayName = "Kwkl/68 105MM KT 105"),
	KwK44_128MM UMETA(DisplayName = "KwK44 128MM (Maus)"),
	KwK44_128MM_E100 UMETA(DisplayName = "KwK44 128MM (E100)"),
	sIG_33_150MM UMETA(DisplayName = "sIG 33 150MM (Sturmpanzer)"),
	Stu_H_43_L_12_150MM UMETA(DisplayName = "StuH 43 L/12 150MM (Brumbar)"),


	// Russian Small Arms
	Mosin UMETA(DisplayName = "Mosin 7.62MM"),
	Mosin_Snip UMETA(DisplayName = "Mosin Sniper 7.62MM"),
	SKS UMETA(DisplayName = "SKS 7.62MM"),
	PPSh_41_7_62MM UMETA(DisplayName = "PPSh-41 7.62MM"),
	PPS_43_7_62MM UMETA(DisplayName = "PPS-43 7.62MM"),
	DP_28_7_62MM UMETA(DisplayName = "DP-28 7.62MM"),
	SVT_40_7_62MM UMETA(DisplayName = "SVT-40 7.62MM"),
	M1895_Nagant UMETA(DisplayName = "M1895 Nagant 7.62MM"),
	Fedrov_Avtomat UMETA(DisplayName = "Fedrov Avtomat 7.62MM"),
	// Heavy mg on top of tanks.
	DShk_12_7MM UMETA(DisplayName = "DShK 12.7MM"),
	// Russian At for infantry
	PTRS_41_14_5MM UMETA(DisplayName = "PTRS-41 14.5MM"),
	PTRS_X_Tishina UMETA(DisplayName = "PTRS X Tishina 14.5MM Automated"),
	Bazooka_50MM UMETA(DisplayName = "Bazooka 50MM (RPG-1)"),


	// Russian Small calibre
	shVAK_20MM UMETA(DisplayName = "ShVAK 20MM"),
	T26_45MM UMETA(DisplayName = "20-K 45MM (T-26)"),
	PTRS_50MM UMETA(DisplayName = "PTRS 50MM AT RIFLE"),
	BT_7_20K_45MM UMETA(DisplayName = "20-K 45MM BT-7"),
	T_70_20k_45MM UMETA(DisplayName = "20-K 45MM T-70"),
	Bofors_40MM UMETA(DisplayName = "Bofors 40MM (Bofors)"),
	NS_37MM UMETA(DisplayName = "NS 37MM (NS-37)"),

	// Russian laser weapons
	// infantry held
	SPEKTR_V UMETA(DisplayName = "Spektr V 30MM (Infantry Laser Weapon)"),

	// Russian Railgun weapons
	RailGunY UMETA(DisplayName = "Double sided railgun 30MM"),
	

	// Russian Medium calibre
	ZIS_S_53_85MM UMETA(DisplayName = "ZIS 85mm (T34/85)"),
	ZIS_3_76MM UMETA(DisplayName = "ZIS-3 76MM (ZIS-3)"),
	DT_5_85MM UMETA(DisplayName = "DT-5 85mm (IS-1)"),
	L_11_76MM UMETA(DisplayName = "L_11 76mm (KV-1 L-11)"),
	F_34_76MM UMETA(DisplayName = "F-34 76MM (T-34)"),
	F_34_T34E UMETA(DisplayName = "F-34 76MM (T-34E)"),
	L_10_76MM UMETA(DisplayName = "L_10 76MM (T-28)"),
	// Faster reload than L_11.
	F_35_76MM UMETA(DisplayName = "F-35 76MM (KV-1E)"),
	ZIS_5_76MM UMETA(DisplayName = "ZIS-5 76MM (KV-IS)"),
	KT_28_76MM UMETA(DisplayName = "Short Barrel 76MM (T-28T)"),
	KT_28_76MM_BT UMETA(DisplayName = "KT-28 76MM BT-7-4"),
	KT_28_76MM_T24 UMETA(DisplayName = "KT-28 76MM T-24-4"),
	// Russian Heavy calibre
	LB_1_100MM UMETA(DisplayName = "LB-1 100mm (T34/100)"),
	D_25T_122MM UMETA(DisplayName = "DT-25T 122MM (IS-2)"),
	ZIS_6_107MM UMETA(DisplayName = "ZIS-6 107MM (KV-5)"),
	QF_37In_94MM UMETA(DisplayName = "QF 37in 94MM"),
	D_25T_122MM_IS3 UMETA(Displayname = "DT-25T 122MM (IS-3)"),
	M_10T_152MM UMETA(DisplayName = "M-10T 152MM (KV-2)")
};

static FString Global_GetWeaponDisplayName(const EWeaponName WeaponName)
{
	switch (WeaponName)
	{
	case EWeaponName::T26_Mg: return "7.62 DT";
	case EWeaponName::MP46: return "MP46";
	case EWeaponName::F_35_76MM: return "F-35";
	case EWeaponName::BK_5_50MM: return "BK 5";
	case EWeaponName::F_34_T34E: return "F-34";
	case EWeaponName::M_10T_152MM: return "M 10T";
	case EWeaponName::Flak37_88MM: return "Flak 37";
	case EWeaponName::Flak38_20MM: return "Flak 38";
		case EWeaponName::SPEKTR_V: return "Spektr-V";
	case EWeaponName::KwK30_20MM: return "KwK 30";
	case EWeaponName::L_11_76MM: return "L/11";
	case EWeaponName::shVAK_20MM: return "shVAK";
	case EWeaponName::NS_37MM: return "NS-37";
	case EWeaponName::T26_45MM: return "20-K";
	case EWeaponName::BT_7_20K_45MM: return "20-K";
	case EWeaponName::T_70_20k_45MM: return "20-K";
	case EWeaponName::KT_28_76MM: return "KT-28";
	case EWeaponName::KT_28_76MM_T24: return "KT-28";
	case EWeaponName::KT_28_76MM_BT: return "KT-28";
	case EWeaponName::KwK36_88MM: return "KwK 36";
	case EWeaponName::KWK42_75MM_PantherD: return "KwK 42";
	case EWeaponName::Pak38_50MM: return "Pak 38";
	case EWeaponName::Kwk30_20MM_sdkfz231: return "KwK 30";
	case EWeaponName::KwK39_1_50MM: return "KwK 39/1";
	case EWeaponName::KwK39_1_50MM_Puma: return "Kwk 39/1";
	case EWeaponName::KwK42_75MM: return "KwK 42";
	case EWeaponName::KwK43_88MM: return "KwK 43";
	case EWeaponName::KwK43_88MM_PantherII: return "KwK 43";
	case EWeaponName::LB_1_100MM: return "LB-1";
	case EWeaponName::PTRS_X_Tishina: return "PTRS X Tishina";
	case EWeaponName::KwK38_T_37MM: return "KwK 38(t)";
	case EWeaponName::KwK40_L48_75MM: return "KwK L/48";
	case EWeaponName::KwK42_L_50MM: return "KwK L/42";
	case EWeaponName::KwKL_68_105MM: return "KwK L/68";
	case EWeaponName::PTRS_50MM: return "PTRS 50MM";
	case EWeaponName::Ger_TankMG_7_6MM: return "MG 34";
	case EWeaponName::ZIS_S_53_85MM: return "ZiS-S-53";
	case EWeaponName::ZIS_3_76MM: return "ZiS-3";
	case EWeaponName::DT_5_85MM: return "DT-5";
	case EWeaponName::ZIS_5_76MM: return "ZiS-5";
	case EWeaponName::F_34_76MM: return "F-34";
	case EWeaponName::Kwk34_Twin30MM: return "Kwk 34 Twin";
	case EWeaponName::Strahlkanone39: return "Strahlkn. 39";
	case EWeaponName::L_10_76MM: return "L-10";
	case EWeaponName::D_25T_122MM: return "D-25T";
	case EWeaponName::D_25T_122MM_IS3: return "D-25T";
	case EWeaponName::KwK44_128MM: return "KwK 44";
	case EWeaponName::KwK44_L_36_5_75MM: return "KwK44 L/36";
	case EWeaponName::StuK40_L48_75MM: return "StuK 40";
	case EWeaponName::Pak40_3_L46_75MM: return "Pak40/3";
	case EWeaponName::Pak43_88MM: return "Pak43";
	case EWeaponName::Pak40_3_L46_75MM_Sdkfz251: return "Pak40/3";
	case EWeaponName::Pak_t_L_43_47MM: return "Pak(t)L/43";
	case EWeaponName::QF_37In_94MM: return "QF 37in";
	case EWeaponName::Kwk31_30MM: return "KwK 31";
	case EWeaponName::Kwk36_88MM_TigerH1: return "KwK 36";
	case EWeaponName::Kwk37_20MM: return "Kwk 37";
	case EWeaponName::Kwk32_35MM: return "Kwk 32";
	case EWeaponName::sIG_33_150MM: return "s.I.G. 33";
	case EWeaponName::STG44_7_92MM: return "STG 44";
	case EWeaponName::MP40_9MM: return "MP40";
	case EWeaponName::Kar_98k: return "Kar 98k";
	case EWeaponName::Bomb_500Gr: return "500Gr Bomb";
	case EWeaponName::Mauser: return "Mauser";
	case EWeaponName::MG_34: return "MG 34";
	case EWeaponName::MP40: return "MP40";
	case EWeaponName::Kar_Sniper: return "Kar 98/2";
	case EWeaponName::FG_42_7_92MM: return "FG 42";
	case EWeaponName::PanzerSchreck: return "Pzr Schreck";
	case EWeaponName::Mosin: return "Mosin";
	case EWeaponName::Mosin_Snip: return "Mosin";
	case EWeaponName::PPSh_41_7_62MM: return "PPSh-41";
	case EWeaponName::DP_28_7_62MM: return "DP-28";
	case EWeaponName::SVT_40_7_62MM: return "SVT-40";
	case EWeaponName::M1895_Nagant: return "M1895 Nagant";
	case EWeaponName::Fedrov_Avtomat: return "Fedrov";
	case EWeaponName::PTRS_41_14_5MM: return "PTRS";
	case EWeaponName::Pak39_L_48_75MM: return "Pak 39";
	case EWeaponName::PanzerFaust: return "Pzr Faust";
	case EWeaponName::Flak36_37MM: return "Flak 36";
	case EWeaponName::LeFH_18_105MM: return "LeFH 18";
	case EWeaponName::Stu_H_43_L_12_150MM: return "Stu.H 43 L/12";
	case EWeaponName::DShk_12_7MM: return "DShk";
	case EWeaponName::Bofors_40MM: return "Bofors 40MM";
	case EWeaponName::MG_15: return "MG 15";
	case EWeaponName::MG_151: return "MG 151";
	case EWeaponName::KwK37_75MM: return "KwK 37";
	case EWeaponName::Flamm09: return "Flamm 09";
	case EWeaponName::Flamm15: return "Flamm 15";
	case EWeaponName::Ashmaker05: return "Ashmaker 05";
	case EWeaponName::Ashmaker11: return "Ashmaker 11";
	case EWeaponName::RipperGun_7_62MM: return "Ripper Gun";
	case EWeaponName::PanzerRifle_50mm: return "Pzr Rifle 50mm";
	case EWeaponName::LB14: return "LB 14";
	case EWeaponName::KwK37_F_50MM: return "KwK 37 FLm";
	case EWeaponName::LightStorm: return "LightStorm";
	case EWeaponName::Bomb_250Gr: return "250Gr Bomb";
	case EWeaponName::Bomb_400Gr: return "400Gr Bomb";
	case EWeaponName::BombRocket_700Gr: return "700Gr Bomb Rocket";
	case EWeaponName::BombRocket_800Gr: return "800Gr Bomb Rocket";
	case EWeaponName::Bomb_1000Gr: return "1000Gr Bomb";
	case EWeaponName::BombRocket_1500Gr: return "1500Gr Bomb Rocket";
	case EWeaponName::BombRocket_2000Gr: return "2000Gr Bomb Rocket";
	case EWeaponName::BombRocket_3000Gr: return "3000Gr Bomb Rocket";
	case EWeaponName::BombRocket_5000Gr: return "5000Gr Bomb Rocket";
	case EWeaponName::KwK44_128MM_E100: return "KwK 44";
		case EWeaponName::KwK44_L_36_5_75MM_E100: return "KwK44 L/36";
	case EWeaponName::ZIS_6_107MM: return "ZIS-6";
	case EWeaponName::Bazooka_50MM:
		return "Bazooka";
	case EWeaponName::GerRailGun:
		return "RailGun";
	case EWeaponName::M920_AtSniper:
		return "M920";
	case EWeaponName::RailGunY:
		return "RailGun Y";
	case EWeaponName::SKS:
		return "SKS";
	case EWeaponName::PPS_43_7_62MM:
		return "PPS-43";

	case EWeaponName::DEFAULT_WEAPON: return "Default Weapon";
	default:
		return "NoWpTranslation";
	}
}

static int32 Global_GetWeaponValue(const EWeaponName WeaponType)
{
    switch (WeaponType)
    {
    	
        // ---------------------------------------------------------------------
        // Handheld laser weapons (Highest  tier)
        // ---------------------------------------------------------------------
    case EWeaponName::GerRailGun:
    	return 1100;
    case EWeaponName::RailGunY:
    	return 1095;
        // ---------------------------------------------------------------------
        // Handheld laser weapons (top tier, category ~1000)
        // ---------------------------------------------------------------------
        case EWeaponName::LightStorm:          // Light Laser MG handheld
            return 1000;
        case EWeaponName::LB14:               // Handheld laser
            return 998;
        case EWeaponName::SPEKTR_V:           // Infantry Laser Weapon
            return 996;
    	

        // ---------------------------------------------------------------------
        // Automated / special AT above flame (category ~900)
        // ---------------------------------------------------------------------
        case EWeaponName::PTRS_X_Tishina:     // Automated PTRS
            return 900;
    case EWeaponName::M920_AtSniper: // sniper AT.
    	return 898;
        case EWeaponName::PanzerRifle_50mm:   // Rifle grenade launcher
            return 895;
    	

        // ---------------------------------------------------------------------
        // Flame weapons (infantry, category ~800)
        // ---------------------------------------------------------------------
        case EWeaponName::Flamm15:            // Medium flamethrower
            return 800;
        case EWeaponName::Ashmaker11:         // Light-medium flamethrower
            return 798;
        case EWeaponName::Flamm09:            // Light flamethrower
            return 796;
        case EWeaponName::Ashmaker05:         // Very light flamethrower
            return 794;

        // ---------------------------------------------------------------------
        // Sniper rifles (category ~700)
        // ---------------------------------------------------------------------
        case EWeaponName::Kar_Sniper:
            return 700;
        case EWeaponName::Mosin_Snip:
            return 698;

        // ---------------------------------------------------------------------
        // Infantry AT weapons (category ~600)
        // ---------------------------------------------------------------------
        case EWeaponName::PTRS_50MM:
            return 598;
        case EWeaponName::PanzerSchreck:
            return 596;
    case EWeaponName::Bazooka_50MM:
    	return 595;
        case EWeaponName::PanzerFaust:
            return 594;
        case EWeaponName::PTRS_41_14_5MM:
            return 590;
    	

        // ---------------------------------------------------------------------
        // HMG / LMG – RipperGun top HMG, MP46 lowest HMG (category ~500)
        // ---------------------------------------------------------------------
        case EWeaponName::RipperGun_7_62MM:
            return 500;
    	// Special case of very powerful rifle.
        case EWeaponName::SKS:
            return 499;
        case EWeaponName::MG_34:
            return 498;
        case EWeaponName::DP_28_7_62MM:
            return 496;
        case EWeaponName::MP46:               // Very powerful SMG, treated as low HMG tier
            return 494;

        // ---------------------------------------------------------------------
        // Automatic / assault rifles (SVT > Fedrov > FG42 > STG) (category ~400)
        // ---------------------------------------------------------------------
        case EWeaponName::SVT_40_7_62MM:
            return 400;
        case EWeaponName::Fedrov_Avtomat:
            return 398;
        case EWeaponName::FG_42_7_92MM:
            return 396;
        case EWeaponName::STG44_7_92MM:
            return 394;

        // ---------------------------------------------------------------------
        // SMGs (category ~300)
        // ---------------------------------------------------------------------
    case EWeaponName::PPS_43_7_62MM:
    	return 310;
        case EWeaponName::PPSh_41_7_62MM:
            return 300;
        case EWeaponName::MP40_9MM:
            return 298;
        case EWeaponName::MP40:
            return 296;

        // ---------------------------------------------------------------------
        // Rifles (category ~200)
        // ---------------------------------------------------------------------
        case EWeaponName::Kar_98k:
            return 200;
        case EWeaponName::Mosin:
            return 198;
        case EWeaponName::Mauser:
            return 196;

        // ---------------------------------------------------------------------
		
        // ---------------------------------------------------------------------
        case EWeaponName::M1895_Nagant:
            return 100;
        case EWeaponName::DEFAULT_WEAPON:
            return 98;

        // ---------------------------------------------------------------------
        // Everything else: not handheld / not infantry.
        // This function is only meaningful in that context → return 1.
        // ---------------------------------------------------------------------
        default:
            return 1;
    }
}
static FString Global_GetWeaponEnumAsString(const EWeaponName WeaponName)
{
	switch (WeaponName)
	{
	case EWeaponName::DEFAULT_WEAPON:
		return "DEFAULT_WEAPON";
	case EWeaponName::BK_5_50MM:
		return "BK_5_50MM";
	case EWeaponName::T26_Mg:
		return "T26_Mg";
	case EWeaponName::KwK38_T_37MM:
		return "KwK38_T_37MM";
	case EWeaponName::KwK39_1_50MM:
		return "KwK39_1_50MM";
	case EWeaponName::KwK39_1_50MM_Puma:
		return "KwK39_1_50MM_Puma";
	case EWeaponName::KwK42_L_50MM:
		return "KwK42_L_50MM";
	case EWeaponName::Flak38_20MM:
		return "Flak38_20MM";
	case EWeaponName::KwK30_20MM:
		return "Kwk30_20MM";
	case EWeaponName::KwK40_L48_75MM:
		return "KwK40_L48_75MM";
	case EWeaponName::Ger_TankMG_7_6MM:
		return "Ger_TankMG_7_6MM";
	case EWeaponName::DShk_12_7MM:
		return "DShk_12_7MM";
	case EWeaponName::KwK42_75MM:
		return "KwK42_75MM";
	case EWeaponName::KwK36_88MM:
		return "KwK36_88MM";
	case EWeaponName::Flak37_88MM:
		return "Flak37_88MM";
	case EWeaponName::KwK43_88MM:
		return "KwK43_88MM";
	case EWeaponName::KwK43_88MM_PantherII:
		return "KwK43_88MM_PantherII";
	case EWeaponName::KwKL_68_105MM:
		return "KwKL_68_105MM";
	case EWeaponName::T26_45MM:
		return "T26_45MM";
	case EWeaponName::BT_7_20K_45MM:
		return "BT_7_20K_45MM";
	case EWeaponName::T_70_20k_45MM:
		return "T_70_20k_45MM";
	case EWeaponName::ZIS_S_53_85MM:
		return "ZIS_S_53_85MM";
	case EWeaponName::ZIS_3_76MM:
		return "ZIS_3_76MM";
	case EWeaponName::DT_5_85MM:
		return "DT_5_85MM";
	case EWeaponName::L_11_76MM:
		return "L_11_76MM";
	case EWeaponName::F_35_76MM:
		return "F_35_76MM";
	case EWeaponName::ZIS_5_76MM:
		return "ZIS_5_76MM";
	case EWeaponName::KT_28_76MM:
		return "KT_28_76MM";
	case EWeaponName::F_34_76MM:
		return "F_34_76MM";
	case EWeaponName::L_10_76MM:
		return "L_10_76MM";
	case EWeaponName::KT_28_76MM_BT:
		return "KT_28_76MM_BT";
	case EWeaponName::KT_28_76MM_T24:
		return "KT_28_76MM_T24";
	case EWeaponName::LB_1_100MM:
		return "LB_1_100MM";
	case EWeaponName::D_25T_122MM:
		return "D_25T_122MM";
	case EWeaponName::D_25T_122MM_IS3:
		return "D_25T_122MM";
	case EWeaponName::KwK44_L_36_5_75MM:
		return "KwK44_L_36_5_75MM";
	case EWeaponName::KwK44_128MM:
		return "KwK44_128MM";
	case EWeaponName::StuK40_L48_75MM:
		return "Stuk40_L48_75MM";
	case EWeaponName::Pak40_3_L46_75MM:
		return "Pak40_3_L46_75MM";
	case EWeaponName::Pak43_88MM:
		return "Pak43_88MM";
	case EWeaponName::Pak_t_L_43_47MM:
		return "Pak_t_L_43_47MM";
	case EWeaponName::Kwk31_30MM:
		return "Kwk31_30MM";
	case EWeaponName::Kwk37_20MM:
		return "Kwk37_20MM";
	case EWeaponName::sIG_33_150MM:
		return "sIG_33_150MM";
	case EWeaponName::STG44_7_92MM:
		return "STG44_7_92MM";
	case EWeaponName::MP40_9MM:
		return "MP40_9MM";
	case EWeaponName::Kar_98k:
		return "Kar_98k";
	case EWeaponName::Mauser:
		return "Mauser";
	case EWeaponName::MG_34:
		return "MG_34";
	case EWeaponName::MP40:
		return "MP40";
	case EWeaponName::Kar_Sniper:
		return "Kar_Sniper";
	case EWeaponName::FG_42_7_92MM:
		return "FG_42_7_92MM";
	case EWeaponName::PanzerSchreck:
		return "PanzerSchreck";
	case EWeaponName::Mosin:
		return "Mosin";
	case EWeaponName::Mosin_Snip:
		return "Mosin_Snip";
	case EWeaponName::PPSh_41_7_62MM:
		return "PPSh_41_7_62MM";
	case EWeaponName::DP_28_7_62MM:
		return "DP_28_7_62MM";
	case EWeaponName::SVT_40_7_62MM:
		return "SVT_40_7_62MM";
	case EWeaponName::M1895_Nagant:
		return "M1895_Nagant";
	case EWeaponName::Fedrov_Avtomat:
		return "Fedrov_Avtomat";
	case EWeaponName::PTRS_41_14_5MM:
		return "PTRS_41_14_5MM";
	case EWeaponName::Pak39_L_48_75MM:
		return "Pak39_L_48_75MM";
	case EWeaponName::PanzerFaust:
		return "PanzerFaust";
	case EWeaponName::Flak36_37MM:
		return "Flak36_37MM";
	case EWeaponName::LeFH_18_105MM:
		return "LeFH_18_105MM";

	default:
		{
			const auto WeaponEnum = StaticEnum<EWeaponName>();
			if(WeaponEnum)
			{
			const FName Name = WeaponEnum->GetNameByValue(static_cast<int64>(WeaponName));
			const FName CleanName = FName(*Name.ToString().RightChop(FString("EWeaponName::").Len()));
			return CleanName.ToString();
				
			}
			return "INVALID WEAPON ENUM";
		}
	}
}
