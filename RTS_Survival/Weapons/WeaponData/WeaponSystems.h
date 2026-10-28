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
// uint32 so we can grow past 256 weapons; Blueprints therefore use FName weapon names, see Global_GetWeaponNameFromFName.
// Append only: never reorder or remove an entry (hide it with UMETA(Hidden) instead).
UENUM()
enum class
	EWeaponName : uint32
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
	Ger_TankMG_34B UMETA(DisplayName = "MG 34 Ausf. B"),
	Ger_TankMG_42 UMETA(DisplayName = "Ger Tank MG 42 (Faster rate of fire)"),
	Ger_TankMG_13 UMETA(DisplayName = "Ger Tank MG 13 (slower rate of fire)"),
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
	PzJagerLaser35 UMETA(DisplayName = "Jagerstrahl 35"),
	Strahlkanone39 UMETA(DisplayName = "Strahlkanone 39 50MM PZ V/III"),
	JagdtigerSonnensturm128L UMETA(DisplayName = "Sonnensturm 128L (Jagdtiger Siege Laser)"),

	// Railgun Weapons
	GerRailGun30MM UMETA(DisplayName = "Ger Railgun 30MM"),
	GerRailgun20MM UMETA(DisplayName = "20 Rail"),
	RailGun50MM UMETA(DisplayName = "50MM RailGun"),
	RailGun80MM UMETA(DisplayName = "80MM RailGun"),

	// German Flame Weapons
	Ashmaker05 UMETA(DisplayName = "ashmaker 05 (Very Light Flame Thrower)"),
	Flamm09 UMETA(DisplayName = "Flamm 09 (Light Flame Thrower)"),
	Ashmaker11 UMETA(DisplayName = "ashmaker 11 (Light-Medium Flame Thrower)"),
	Flamm15 UMETA(DisplayName = "Flamm 15 (Medium Flame Thrower)"),
	FlammIS3 UMETA(DisplayName = "Flamm IS-3 (Heavy Flame Thrower)"),

	// German small calibre
	KwK38_T_37MM UMETA(DisplayName = "KwK38 t 37MM (PZ 38(t))"),
	KwK39_1_50MM UMETA(DisplayName = "KwK39/1 50MM (PZ III M)"),
	KwK39_Rockets UMETA(DisplayName = "20mm Rockets"),
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
	Flak36_37MM_TW UMETA(DisplayName = "Flak36 37MM TW"),

	// Ger vertical Rocket Weapons
	Rocket_30mm UMETA(DisplayName = "30MM Vertical Rocket (Sdkfz140 Rocket)"),
	Rocket_50mm UMETA(DisplayName = "50MM Vertical Rocket (PZ IV Rocket)"),


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
	Pak38_50MM_TW UMETA(DisplayName = "Pak38 50MM TW"),
	Pak40_3_L46_75MM UMETA(DisplayName = "Pak40 3/L46 75MM (Marder)"),
	Pak40_3_L46_75MM_TW UMETA(DisplayName = "Pak40 3/L46 75MM TW"),
	KwK44_L_36_5_75MM UMETA(DisplayName = "KwK44 L/36.5 75MM Mause Secondary"),
	KwK44_L_36_5_75MM_E100 UMETA(DisplayName = "KwK44 L/36.5 75MM E100 Secondary"),
	// German Heavy calibre
	KwK36_88MM UMETA(DisplayName = "KwK36 88MM (Tiger)"),
	Kwk36_88MM_TigerH1 UMETA(DisplayName = "KwK36 88MM TigerH1"),
	Flak37_88MM UMETA(DisplayName = "Flak37_88MM (Flak 88 BXP)"),
	Flak37_88MM_TW UMETA(DisplayName = "Flak37 88MM TW"),
	KwK43_88MM UMETA(DisplayName = "Kwk43 88MM (KingTiger)"),
	Pak43_88MM UMETA(DisplayName = "Pak43 88MM (Jagdpanther)"),
	KwK43_88MM_PantherII UMETA(DisplayName = "Kwk43 88MM PantherII"),
	LeFH_18_105MM UMETA(DisplayName = "LeFH 18 105MM (Howitzer)"),
	LeFH_18_105MM_TW UMETA(DisplayName = "LeFH 18 105MM TW"),
	Morser_18_210MM_TW UMETA(DisplayName = "Morser 18 210MM TW"),
	// German Super Heavy calibre
	KwKL_68_105MM UMETA(DisplayName = "Kwkl/68 105MM KT 105"),
	KwK44_128MM UMETA(DisplayName = "KwK44 128MM (Maus)"),
	KwK44_128MM_E100 UMETA(DisplayName = "KwK44 128MM (E100)"),
	sIG_33_150MM UMETA(DisplayName = "sIG 33 150MM (Sturmpanzer)"),
	sFH18_150MM_TW UMETA(DisplayName = "sFH18 150MM TW"),
	Stu_H_43_L_12_150MM UMETA(DisplayName = "StuH 43 L/12 150MM (Brumbar)"),
	Panzerwerfer UMETA(DisplayName = "Panzerwerfer 150MM"),
	Nebelwerfer_150MM_TW UMETA(DisplayName = "Nebelwerfer 150MM TW"),
	RW61_Mortar_380MM UMETA(DisplayName = "RW61 Mortar 380MM (SturmTiger)"),
	Mortar_120MM UMETA(DisplayName = "120MM Mortar"),
	Mortar_120MM_TW UMETA(DisplayName = "120MM Mortar TW"),
	Mortar_80MM UMETA(DisplayName = "80MM Mortar"),
	Mortar_80MM_TW UMETA(DisplayName = "80MM Mortar TW"),
	Mortar_40MM UMETA(DisplayName = "40MM Mortar"),
	Panzerwerfer_Small UMETA(DisplayName = "Panzerwerfer 40MM"),


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
	Ba12_23MM UMETA(DisplayName = "Ba-12 23MM"),
	AM_23 UMETA(DisplayName = "AM-23 23MM"),
	T26_45MM UMETA(DisplayName = "20-K 45MM (T-26)"),
	PTRS_50MM UMETA(DisplayName = "PTRS 50MM AT RIFLE"),
	BT_7_20K_45MM UMETA(DisplayName = "20-K 45MM BT-7"),
	T_70_20k_45MM UMETA(DisplayName = "20-K 45MM T-70"),
	Bofors_40MM UMETA(DisplayName = "Bofors 40MM (Bofors)"),
	Bofors_40MM_TW UMETA(DisplayName = "Bofors 40MM TW"),
	NS_37MM UMETA(DisplayName = "NS 37MM (NS-37)"),
	RusTwin35MM UMETA(DisplayName = "Rus Twin 35MM (T-34-AA)"),

	// Russian laser weapons
	// infantry held
	SPEKTR_V UMETA(DisplayName = "Spektr V 30MM (Infantry Laser Weapon)"),
	Luch_50L UMETA(DisplayName = "Luch 50L (T-34/76L Laser)"),
	Luch_85L UMETA(DisplayName = "Luch 85L (SU-85 Laser)"),
	Zarya_100L UMETA(DisplayName = "Zarya 100L (T-44 Laser)"),

	// Russian Railgun weapons
	RailGunY UMETA(DisplayName = "Double sided railgun 30MM"),


	// Russian Medium calibre
	ZIS_S_53_85MM UMETA(DisplayName = "ZIS 85mm (T34/85)"),
	D_5S_85MM_SU85 UMETA(DisplayName = "D-5S 85MM (SU-85)"),
	ZIS_3_76MM UMETA(DisplayName = "ZIS-3 76MM (ZIS-3)"),
	ZIS_3_76MM_TW UMETA(DisplayName = "ZIS-3 76MM TW"),
	ZIS_3_76MM_SU76 UMETA(DisplayName = "ZIS-3 76MM (SU-76)"),
	DT_5_85MM UMETA(DisplayName = "DT-5 85mm (IS-1)"),
	L_11_76MM UMETA(DisplayName = "L_11 76mm (KV-1 L-11)"),
	L_14_122MM_Arc UMETA(DisplayName = "L-11 76MM Arc (KV-1 Arc)"),
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
	D_10S_100MM_SU100 UMETA(DisplayName = "D-10S 100MM (SU-100)"),
	D_25T_122MM UMETA(DisplayName = "DT-25T 122MM (IS-2)"),
	M_30S_122MM_SU122 UMETA(DisplayName = "M-30S 122MM (SU-122)"),
	M1938_122MM UMETA(DisplayName = "M-1938 122MM (Howitzer)"),
	ZIS_6_107MM UMETA(DisplayName = "ZIS-6 107MM (KV-5)"),
	QF_37In_94MM UMETA(DisplayName = "QF 37in 94MM"),
	D_25T_122MM_IS3 UMETA(DisplayName = "DT-25T 122MM (IS-3)"),
	M_10T_152MM UMETA(DisplayName = "M-10T 152MM (KV-2)"),
	ML_20S_152MM_SU152 UMETA(DisplayName = "ML-20S 152MM (SU-152)"),
	KS30_130MM_TW UMETA(DisplayName = "KS-30 130MM TW"),
	TM_3_12_305MM UMETA(DisplayName = "TM-3-12 305MM"),
	// Appended so serialized weapon names keep their values.
	B4_203MM_TW UMETA(DisplayName = "B-4 203MM TW"),
	// The fortified gun pit version: same gun, longer crane-loading cycle.
	B4_203MM_Pit UMETA(DisplayName = "B-4 203MM Gun Pit"),
	// Soviet 45mm anti-tank gun M1937 (53-K): the team weapon and its sandbag pit fire the same gun.
	M53K_45MM_TW UMETA(DisplayName = "53-K 45MM TW"),
	// ML-20 152mm gun-howitzer: the towed team weapon and the gun pit that loads it with the B-4's crane drill.
	ML20_152MM_TW UMETA(DisplayName = "ML-20 152MM TW"),
	ML20_152MM_Pit UMETA(DisplayName = "ML-20 152MM Gun Pit"),
	// Soviet 100mm field gun M1944 (BS-3): the team weapon and its square sandbag pit fire the same gun.
	BS3_100MM_TW UMETA(DisplayName = "BS-3 100MM TW"),
	// The Flak 36/37 in its pit: 6-round clips handed to the loader, so the clip change takes longer than the team weapon's.
	Flak36_37MM_Pit UMETA(DisplayName = "Flak36 37MM Gun Pit"),
	// Soviet infantry weapons (2nd batch).
	// 125mm ampoule launcher: lobs incendiary glass ampoules.
	Ampulomet_125MM UMETA(DisplayName = "Ampulomet M1941 125MM (incendiary ampoule launcher)"),
	ROKS3_Flamethrower UMETA(DisplayName = "ROKS-3 (Medium Flame Thrower)"),
	Korovin_M1941_7_62MM UMETA(DisplayName = "Korovin M1941 7.62MM"),
	DS39_7_62MM UMETA(DisplayName = "DS-39 7.62MM"),
	// Cup discharger on a Mosin: high-explosive rifle grenades.
	Dyakonov_RGL_41MM UMETA(DisplayName = "Dyakonov 41MM (rifle grenade launcher)"),
	// Electromagnetic rifle that charges its coils before every shot.
	ProtoCoilgun_20MM UMETA(DisplayName = "Prototype Coilgun 20MM"),
	// Raketenwerfer 43 "Puppchen": 88mm HEAT rockets from a small wheeled launcher loaded through a breech.
	Raketenwerfer43_88MM UMETA(DisplayName = "Raketenwerfer 43 88MM"),
	// The Panzerschreck foxhole's weapon: the infantry Panzerschreck, balanced on its own for a light emplacement.
	PanzerSchreck_Pit UMETA(DisplayName = "PanzerSchreck 88MM Foxhole"),
	// The ISU-152's casemate gun: the SU-152's ML-20S balanced as a direct-fire heavy tank destroyer gun.
	ML_20S_152MM_ISU152 UMETA(DisplayName = "ML-20S 152MM (ISU-152)"),
	// The Maxim team weapon's water-cooled 7.62mm machine gun; it used to borrow the DShK's 12.7mm entry.
	Maxim_M1910_7_62MM_TW UMETA(DisplayName = "Maxim M1910 7.62MM TW"),
	// The Pz II Luchs' 20mm: the Pz II F's KwK 30 stats under its own name, so it can get its own icon.
	KwK38_20MM_Luchs UMETA(DisplayName = "KwK38 20MM (PZ II Luchs)"),
	// The Sd.Kfz. 7/1's quadruple 20mm: the Flak 38 stats under its own name, so it can get its own icon.
	Flakvierling38_20MM UMETA(DisplayName = "Flakvierling 38 20MM (Sdkfz 7/1)"),
	// The Sd.Kfz. 221's 20mm: the Sd.Kfz. 231's KwK 30 stats under its own name, so it can get its own icon.
	Kwk30_20MM_sdkfz221 UMETA(DisplayName = "KwK30 20MM (Sdkfz 221)"),
	// The Jagdpanzer IV/70's long 75mm: the Panther's gun in a medium tank destroyer casemate.
	Pak42_L70_75MM UMETA(DisplayName = "Pak42 L/70 75MM (Jagdpanzer IV/70)"),
	// Unit variant batch: appended so serialized weapon names keep their values.
	// Pz 38(t) AT Ammo Stock: 25mm autocannon firing 3-round APCR clips.
	KwK_25MM_APCR_Pz38t UMETA(DisplayName = "2.5 cm APCR Autocannon"),
	// Pz 38(t) Railgun Sniper: APCR-only rail rifle with little pen falloff at range.
	GerRailSniper20MM UMETA(DisplayName = "2 cm Rail Sniper"),
	// Sd.Kfz. 250 Howitzer: the short 75mm firing HE only.
	KwK37_75MM_HE_Sdkfz250 UMETA(DisplayName = "7.5 cm KwK 37 L/24 (HE)"),
	// Flak Luchs: twin 15mm; stats are per projectile, one shot fires both barrels.
	MG151_15MM_Twin UMETA(DisplayName = "2x MG 151/15"),
	// Pz I Bison Direct Fire: long 50mm with a faster reload, more range and pen than the KwK 39/1.
	PzI_Bison_L60_50MM UMETA(DisplayName = "5 cm L/60 (Bison)"),
	// Marder Dual Barrel: twin 40mm; stats are per projectile, one shot fires both barrels.
	Ger_Twin40MM_Marder UMETA(DisplayName = "Twin 4 cm (Marder)"),
	// Pz IV G Double Turret: the rear turret's arcing 50mm artillery gun.
	PzIVG_DT_50MM_Artillery UMETA(DisplayName = "5 cm Artillery"),
	// Jagdpanzer IV/70 Siege: 100mm HE siege mortar.
	Mortar_100MM_JPzIVSiege UMETA(DisplayName = "10 cm Siege Mortar"),
	// Jagdpanzer IV/70 Ambush: the Pak 42 locked to APCR (the only shell in its list, so the ammo picker can't switch).
	Pak42_L70_75MM_APCR UMETA(DisplayName = "7.5 cm PaK 42 L/70 (APCR)"),
	// Unit variant batch 2: appended so serialized weapon names keep their values.
	// Brummbar Direct Siege: a short 20 cm direct-fire siege gun; slower reload than the StuH 43.
	StuH_200MM_DirectSiege UMETA(DisplayName = "20 cm StuH (Direct Siege)"),
	// Pz IV H Ostwind: Flak 43 with 8-round clips and a quick clip change.
	Flak43_37MM_Ostwind UMETA(DisplayName = "3.7 cm Flak 43 (Ostwind)"),
	// Pz IV Rockets Incendiary Salvo: three large incendiary rockets per salvo, long reload.
	Rocket_320mm_Incendiary UMETA(DisplayName = "32 cm Incendiary Rocket (Pz IV Rockets)"),
	// Pz IV Rockets Radixite Hail: a ripple of many small radixite rockets.
	RadixiteHail_Rocket_76MM UMETA(DisplayName = "7.6 cm Radixite Hail Rocket"),
	// E25 Hunter: long 60mm firing APCR only.
	E25_Hunter_60MM_APCR UMETA(DisplayName = "6 cm L/100 (E25 Hunter)"),
	// E25 Assault: short 90mm firing HE only.
	E25_Assault_90MM_HE UMETA(DisplayName = "9 cm Sturmkanone (HE)"),
	// Tiger H1 Twin Gun: each of the two 75mm guns; HEAT by default, can switch to HE.
	KwK40_L48_75MM_HEAT_TigerTwin UMETA(DisplayName = "7.5 cm KwK 40 L/48 (HEAT)"),
	// Tiger Rail Bunker: 100mm rail cannon in the casemate.
	TigerRail_100MM_Railgun UMETA(DisplayName = "10 cm Rail Cannon (Rail Bunker)"),
	// Tiger II Rocketeer: two twin 35mm mounts; stats are per projectile, one shot fires all four barrels.
	Ger_2xTwin35MM_TigerII UMETA(DisplayName = "2x Twin 3.5 cm (Tiger II)"),
	// Maus AA: Flak 41 with a 3-round magazine.
	Maus_AA_88MM_Flak UMETA(DisplayName = "8.8 cm Flak 41 (Maus AA)"),
	// Unit variant batch 3: appended so serialized weapon names keep their values.
	// Sd.Kfz. 140 AA: one MG 151/15 of the triple Drilling mount (three separate weapons of this type).
	MG151_15MM_Drilling UMETA(DisplayName = "MG 151/15 (Drilling)"),
	// Sd.Kfz. 140 Bunker: the casemate's paired 5 cm mortar (HE only, autocannon range).
	Mortar_50MM_140Bunker UMETA(DisplayName = "5 cm Casemate Mortar"),
	// Puma Dual Barrel: each of the two MK 103 30mm guns (3-round magazine).
	MK103_30MM_PumaDual UMETA(DisplayName = "MK 103 30MM (Puma Dual)"),
	// Puma Radixite Sniper: one huge 50mm laser shot per reload (damage assumes BeamIterations 1).
	RadixiteSniper_50MM_Laser UMETA(DisplayName = "Radixite Sniper 50MM Laser (Puma)"),
	// Pz II F Radixite Autocannon: 20mm radixite rounds with their own damage over time.
	KwK30_Radixite_20MM UMETA(DisplayName = "Radixite Auto-Blaster 20mm (Pz II F)"),
	// Pz III J Radixite Cannon: radixite rounds by default (own damage over time), HE selectable.
	KwK42_Radixite_50MM UMETA(DisplayName = "Radixite Cannon 50mm (Pz III J)"),
	// Pz III J Rocket Launcher: the 8-rocket rack beside the gun (HEAT only).
	Rocket_80mm_PzIIIRack UMETA(DisplayName = "8 cm Rocket Rack (Pz III J)"),
	// Sd.Kfz. 234/3 Incendiary Support: the Stummel firing napalm HE that leaves burning ground.
	KwK37_75MM_Napalm UMETA(DisplayName = "7.5 cm KwK 37 Napalm"),
	// Sd.Kfz. 234/3 Incendiary Support: each of the two rear flame projectors (burning ground on impact).
	FlameCharge_50MM_234 UMETA(DisplayName = "5 cm Flame Charge"),
	// Pz III AA Drilling: three Flak 38 barrels; stats are per projectile, one shot fires all three.
	Flak38_20MM_Drilling UMETA(DisplayName = "3x 2 cm Flak 38 (Drilling)"),
	// Pz III AA FlaK 41: 5 cm flak with 5-round clips.
	Flak41_50MM UMETA(DisplayName = "5 cm FlaK 41"),
	// Pz 38(t) Rockets Incendiary Salvo: six 15 cm incendiary rockets on a fixed rack.
	Rocket_150mm_Incendiary UMETA(DisplayName = "15 cm Incendiary Rocket (Pz 38(t))"),
	// Pz 38(t) Rockets Precision Missile: two 20 cm vertical missiles.
	Rocket_200mm_PrecisionMissile UMETA(DisplayName = "20 cm Precision Missile (Pz 38(t))"),
	// Unit variant batch 4: appended so serialized weapon names keep their values.
	// Wespe Radixite Howitzer: 20 cm radixite shells with their own damage over time; HE can be loaded too.
	LeFH_Radixite_200MM UMETA(DisplayName = "20 cm Radixite Howitzer"),
	// Pz III M Overcharged: one charged 60mm laser shot per (long) reload.
	Laser_60MM_PzIIIM UMETA(DisplayName = "Overcharged Laser 60mm (Pz III M)"),
	// Pz III M Spaced Armor: the Panzerschreck foxhole rocket with medium tank cannon range.
	PanzerSchreck_PzIIIM UMETA(DisplayName = "PanzerSchreck 88MM (Pz III M)"),
	// Hetzer Flame: the Pz III Flamm's Flamm 15 in the hull, with less damage.
	Flamm15_HetzerHull UMETA(DisplayName = "Flamm 15 (Hetzer hull)"),
	// Hetzer Mortar: HE-only 12 cm mortar with stronger shells and a faster reload.
	Mortar_120MM_Hetzer UMETA(DisplayName = "12 cm Hetzer Mortar"),
	// JagdPanther Pfeil: the PaK 43 firing APCR only (no splash, more pen).
	Pak43_88MM_APCR_Pfeil UMETA(DisplayName = "8.8 cm PaK 43 Pfeil (APCR)"),
	// JagdPanther Bollwerk: the casemate sFH 18 as arcing HE artillery.
	sFH18_150MM_Bollwerk UMETA(DisplayName = "15 cm sFH 18 Bollwerk"),
	// E-50 Ausf. F: the long 8.8 cm KwK 44 L/100 (more pen, range and accuracy than the King Tiger's KwK 43).
	KwK44_88MM_E50 UMETA(DisplayName = "8.8 cm KwK 44 L/100 (E-50)"),
	// T-44/155: a 155mm howitzer in the T-44 turret, firing arcing HE like the towed ML-20.
	T44_155MM_Howitzer UMETA(DisplayName = "155MM Howitzer (T-44/155)"),
	// The MG 42 on the pit field mounts (twin MG42 pit, comm centre MG42 nests): 10-round bursts timed to the tank
	// MG42 burst sound; it used to borrow the DShK's 12.7mm entry.
	Ger_PitMG42_7_92MM UMETA(DisplayName = "MG 42 7.92MM (Pit)"),
	// SMK main turret: the L-11 76mm with a faster reload and a little more range than the KV-1's.
	L_11_76MM_SMK UMETA(DisplayName = "L-11 76MM (SMK)")
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
	case EWeaponName::Luch_50L: return "Luch 50L";
	case EWeaponName::Luch_85L: return "Luch 85L";
	case EWeaponName::Zarya_100L: return "Zarya 100L";
	case EWeaponName::KwK30_20MM: return "KwK 30";
	case EWeaponName::L_11_76MM: return "L/11";
	case EWeaponName::L_14_122MM_Arc: return "L-14 Arc";
	case EWeaponName::shVAK_20MM: return "shVAK";
	case EWeaponName::NS_37MM: return "NS-37";
	case EWeaponName::RusTwin35MM: return "Twin 35mm";
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
	case EWeaponName::KwK39_Rockets: return "20mm Rockets";
	case EWeaponName::KwK39_1_50MM_Puma: return "Kwk 39/1";
	case EWeaponName::KwK42_75MM: return "KwK 42";
	case EWeaponName::KwK43_88MM: return "KwK 43";
	case EWeaponName::KwK43_88MM_PantherII: return "KwK 43";
	case EWeaponName::LB_1_100MM: return "LB-1";
	case EWeaponName::D_10S_100MM_SU100: return "D-10S";
	case EWeaponName::PTRS_X_Tishina: return "PTRS X Tishina";
	case EWeaponName::KwK38_T_37MM: return "KwK 38(t)";
	case EWeaponName::KwK40_L48_75MM: return "KwK L/48";
	case EWeaponName::KwK42_L_50MM: return "KwK L/42";
	case EWeaponName::KwKL_68_105MM: return "KwK L/68";
	case EWeaponName::PTRS_50MM: return "PTRS 50MM";
	case EWeaponName::Ger_TankMG_7_6MM: return "MG 34";
	case EWeaponName::Ger_TankMG_34B: return "MG 34 Ausf. B";
	case EWeaponName::Ger_TankMG_42: return "MG 42";
	case EWeaponName::Ger_TankMG_13: return "MG 13";
	case EWeaponName::Ba12_23MM: return "Ba12 23MM";
	case EWeaponName::AM_23: return "AM-23";
	case EWeaponName::ZIS_S_53_85MM: return "ZiS-S-53";
	case EWeaponName::D_5S_85MM_SU85: return "D-5S";

	case EWeaponName::Flak36_37MM_TW: return "Flak 36 TW";
	case EWeaponName::Pak38_50MM_TW: return "PaK 38 TW";
	case EWeaponName::Pak40_3_L46_75MM_TW: return "PaK 40 TW";
	case EWeaponName::Flak37_88MM_TW: return "Flak 37 TW";
	case EWeaponName::LeFH_18_105MM_TW: return "leFH 18 TW";
	case EWeaponName::Morser_18_210MM_TW: return "Morser 18 TW";
	case EWeaponName::sFH18_150MM_TW: return "sFH 18 TW";
	case EWeaponName::Nebelwerfer_150MM_TW: return "Nebelwerfer TW";
	case EWeaponName::Mortar_120MM_TW: return "120MM Mortar TW";
	case EWeaponName::Mortar_80MM_TW: return "80MM Mortar TW";
	case EWeaponName::Bofors_40MM_TW: return "Bofors TW";
	case EWeaponName::ZIS_3_76MM_TW: return "ZiS-3 TW";
	case EWeaponName::KS30_130MM_TW: return "KS-30 TW";
	case EWeaponName::ZIS_3_76MM: return "ZiS-3";
	case EWeaponName::ZIS_3_76MM_SU76: return "ZiS-3 SU-76";
	case EWeaponName::DT_5_85MM: return "DT-5";
	case EWeaponName::ZIS_5_76MM: return "ZiS-5";
	case EWeaponName::F_34_76MM: return "F-34";
	case EWeaponName::Kwk34_Twin30MM: return "Kwk 34 Twin";
	case EWeaponName::PzJagerLaser35: return "Jagerstrahl 35";
	case EWeaponName::Strahlkanone39: return "Strahlkn. 39";
	case EWeaponName::JagdtigerSonnensturm128L: return "Sonnensturm 128L";
	case EWeaponName::L_10_76MM: return "L-10";
	case EWeaponName::D_25T_122MM: return "D-25T";
	case EWeaponName::D_25T_122MM_IS3: return "D-25T";
	case EWeaponName::M_30S_122MM_SU122: return "M-30S";
	case EWeaponName::M1938_122MM: return "M-1938";
	case EWeaponName::ML_20S_152MM_SU152: return "ML-20S";
	case EWeaponName::ML_20S_152MM_ISU152: return "ML-20S";
	case EWeaponName::TM_3_12_305MM: return "TM-3-12";
	case EWeaponName::B4_203MM_TW: return "B-4";
	case EWeaponName::M53K_45MM_TW: return "53-K";
	case EWeaponName::B4_203MM_Pit: return "B-4";
	case EWeaponName::ML20_152MM_TW: return "ML-20";
	case EWeaponName::ML20_152MM_Pit: return "ML-20";
	case EWeaponName::BS3_100MM_TW: return "BS-3";
	case EWeaponName::Maxim_M1910_7_62MM_TW: return "Maxim";
	case EWeaponName::KwK38_20MM_Luchs: return "KwK 38";
	case EWeaponName::Flakvierling38_20MM: return "Flakvierling 38";
	case EWeaponName::Kwk30_20MM_sdkfz221: return "KwK 30";
	case EWeaponName::Pak42_L70_75MM: return "Pak 42 L/70";
	case EWeaponName::KwK_25MM_APCR_Pz38t: return "2.5 cm APCR";
	case EWeaponName::GerRailSniper20MM: return "Rail Sniper";
	case EWeaponName::KwK37_75MM_HE_Sdkfz250: return "KwK 37 HE";
	case EWeaponName::MG151_15MM_Twin: return "2x MG 151/15";
	case EWeaponName::PzI_Bison_L60_50MM: return "5 cm L/60";
	case EWeaponName::Ger_Twin40MM_Marder: return "Twin 40mm";
	case EWeaponName::PzIVG_DT_50MM_Artillery: return "5 cm Artillery";
	case EWeaponName::Mortar_100MM_JPzIVSiege: return "10 cm Mortar";
	case EWeaponName::Pak42_L70_75MM_APCR: return "Pak 42 APCR";
	case EWeaponName::StuH_200MM_DirectSiege: return "20 cm Siege Gun";
	case EWeaponName::Flak43_37MM_Ostwind: return "Flak 43";
	case EWeaponName::Rocket_320mm_Incendiary: return "Incendiary Rockets";
	case EWeaponName::RadixiteHail_Rocket_76MM: return "Radixite Hail";
	case EWeaponName::E25_Hunter_60MM_APCR: return "6 cm Hunter APCR";
	case EWeaponName::E25_Assault_90MM_HE: return "9 cm HE";
	case EWeaponName::KwK40_L48_75MM_HEAT_TigerTwin: return "KwK 40 HEAT";
	case EWeaponName::TigerRail_100MM_Railgun: return "100MM RailGun";
	case EWeaponName::Ger_2xTwin35MM_TigerII: return "2x Twin 35mm";
	case EWeaponName::Maus_AA_88MM_Flak: return "Flak 41";
	case EWeaponName::MG151_15MM_Drilling: return "MG 151/15";
	case EWeaponName::Mortar_50MM_140Bunker: return "5 cm Mortar";
	case EWeaponName::MK103_30MM_PumaDual: return "MK 103";
	case EWeaponName::RadixiteSniper_50MM_Laser: return "Radixite Sniper";
	case EWeaponName::KwK30_Radixite_20MM: return "Radixite 20mm";
	case EWeaponName::KwK42_Radixite_50MM: return "Radixite 50mm";
	case EWeaponName::Rocket_80mm_PzIIIRack: return "8 cm Rockets";
	case EWeaponName::KwK37_75MM_Napalm: return "KwK 37 Napalm";
	case EWeaponName::FlameCharge_50MM_234: return "Flame Charge";
	case EWeaponName::Flak38_20MM_Drilling: return "3x Flak 38";
	case EWeaponName::Flak41_50MM: return "5 cm Flak 41";
	case EWeaponName::Rocket_150mm_Incendiary: return "Incendiary Rockets";
	case EWeaponName::Rocket_200mm_PrecisionMissile: return "Precision Missile";
	case EWeaponName::LeFH_Radixite_200MM: return "Radixite 200mm";
	case EWeaponName::Laser_60MM_PzIIIM: return "Laser 60mm";
	case EWeaponName::PanzerSchreck_PzIIIM: return "Pzr Schreck";
	case EWeaponName::Flamm15_HetzerHull: return "Hull Flamm";
	case EWeaponName::Mortar_120MM_Hetzer: return "12 cm Mortar";
	case EWeaponName::Pak43_88MM_APCR_Pfeil: return "Pak 43 Pfeil";
	case EWeaponName::sFH18_150MM_Bollwerk: return "sFH 18";
	case EWeaponName::KwK44_88MM_E50: return "KwK 44/2";
	case EWeaponName::T44_155MM_Howitzer: return "155mm How";
	case EWeaponName::Ger_PitMG42_7_92MM: return "MG 42";
	case EWeaponName::L_11_76MM_SMK: return "L/11";
	case EWeaponName::Flak36_37MM_Pit: return "Flak 37";
	case EWeaponName::Raketenwerfer43_88MM: return "RW 43";
	case EWeaponName::PanzerSchreck_Pit: return "Pzr Schreck";
	case EWeaponName::KwK44_128MM: return "KwK 44";
	case EWeaponName::KwK44_L_36_5_75MM: return "KwK44 L/36";
	case EWeaponName::Panzerwerfer: return "Panzerwerfer";
	case EWeaponName::RW61_Mortar_380MM: return "RW61 Mortar";
	case EWeaponName::Mortar_120MM: return "120MM Mortar";
	case EWeaponName::Mortar_80MM: return "80MM Mortar";
	case EWeaponName::Mortar_40MM: return "40MM Mortar";
	case EWeaponName::Panzerwerfer_Small: return "Panzerwerfer 40MM";
	case EWeaponName::StuK40_L48_75MM: return "StuK 40";
	case EWeaponName::Pak40_3_L46_75MM: return "Pak40/3";
	case EWeaponName::Pak43_88MM: return "Pak43";
	case EWeaponName::Pak40_3_L46_75MM_Sdkfz251: return "Pak40/3";
	case EWeaponName::Pak_t_L_43_47MM: return "Pak(t)L/43";
	case EWeaponName::QF_37In_94MM: return "QF 37in";
	case EWeaponName::Kwk31_30MM: return "KwK 31";
	case EWeaponName::Kwk36_88MM_TigerH1: return "KwK 36";
	case EWeaponName::Kwk37_20MM: return "Kwk 37";
		case EWeaponName::Rocket_30mm: return "Rockets";
		case EWeaponName::Rocket_50mm: return "Rockets";
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
	case EWeaponName::FlammIS3: return "Flamm IS-3";
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
	case EWeaponName::GerRailGun30MM:
		return "RailGun";
	case EWeaponName::GerRailgun20MM:
		return "20 Rail";
	case EWeaponName::RailGun50MM:
		return "50MM RailGun";
	case EWeaponName::RailGun80MM:
		return "80MM RailGun";
	case EWeaponName::M920_AtSniper:
		return "M920";
	case EWeaponName::RailGunY:
		return "RailGun Y";
	case EWeaponName::SKS:
		return "SKS";
	case EWeaponName::PPS_43_7_62MM:
		return "PPS-43";
	case EWeaponName::Ampulomet_125MM:
		return "Ampulomet";
	case EWeaponName::ROKS3_Flamethrower:
		return "ROKS-3";
	case EWeaponName::Korovin_M1941_7_62MM:
		return "Korovin";
	case EWeaponName::DS39_7_62MM:
		return "DS-39";
	case EWeaponName::Dyakonov_RGL_41MM:
		return "Dyakonov";
	case EWeaponName::ProtoCoilgun_20MM:
		return "Coilgun";

	case EWeaponName::DEFAULT_WEAPON: return "Default Weapon";
	default:
		return "NoWpTranslation";
	}
}

static bool Global_IsRifle(const EWeaponName WeaponName)
{
	switch (WeaponName)
	{
	case EWeaponName::GerRailGun30MM:
	case EWeaponName::RailGun50MM:
	case EWeaponName::RailGun80MM:
	case EWeaponName::RailGunY:
	case EWeaponName::SVT_40_7_62MM:
	case EWeaponName::Fedrov_Avtomat:
	case EWeaponName::FG_42_7_92MM:
	case EWeaponName::STG44_7_92MM:
	case EWeaponName::Kar_98k:
	case EWeaponName::Mosin:
	case EWeaponName::Mauser:
	case EWeaponName::SKS:
	case EWeaponName::PTRS_50MM:
	case EWeaponName::PTRS_41_14_5MM:
	case EWeaponName::PanzerRifle_50mm:
	case EWeaponName::PTRS_X_Tishina:
	case EWeaponName::Dyakonov_RGL_41MM:
	case EWeaponName::ProtoCoilgun_20MM:
		return true;
	default:
		return false;
	}
}

static int32 Global_GetWeaponValue(const EWeaponName WeaponType)
{
	switch (WeaponType)
	{
	// ---------------------------------------------------------------------
	// Handheld laser weapons (Highest  tier)
	// ---------------------------------------------------------------------
	case EWeaponName::GerRailGun30MM:
		return 1100;
	case EWeaponName::RailGun80MM:
		return 1099;
	case EWeaponName::GerRailgun20MM:
		return 1098;
	case EWeaponName::RailGun50MM:
		return 1097;
	case EWeaponName::RailGunY:
		return 1095;
	case EWeaponName::ProtoCoilgun_20MM:
		return 1090;
	// ---------------------------------------------------------------------
	// Handheld laser weapons (top tier, category ~1000)
	// ---------------------------------------------------------------------
	case EWeaponName::LightStorm: // Light Laser MG handheld
		return 1000;
	case EWeaponName::LB14: // Handheld laser
		return 998;
	case EWeaponName::SPEKTR_V: // Infantry Laser Weapon
		return 996;


	// ---------------------------------------------------------------------
	// Automated / special AT above flame (category ~900)
	// ---------------------------------------------------------------------
	case EWeaponName::PTRS_X_Tishina: // Automated PTRS
		return 900;
	case EWeaponName::M920_AtSniper: // sniper AT.
		return 898;
	case EWeaponName::PanzerRifle_50mm: // Rifle grenade launcher
		return 895;
	case EWeaponName::Dyakonov_RGL_41MM: // Rifle grenade launcher
		return 893;
	case EWeaponName::Ampulomet_125MM: // Incendiary ampoule launcher
		return 890;


	// ---------------------------------------------------------------------
	// Flame weapons (infantry, category ~800)
	// ---------------------------------------------------------------------
	case EWeaponName::Flamm15: // Medium flamethrower
		return 800;
	case EWeaponName::FlammIS3: // IS-3 heavy flamethrower
		return 802;
	case EWeaponName::Ashmaker11: // Light-medium flamethrower
		return 798;
	case EWeaponName::ROKS3_Flamethrower: // Medium flamethrower
		return 799;
	case EWeaponName::Flamm09: // Light flamethrower
		return 796;
	case EWeaponName::Ashmaker05: // Very light flamethrower
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
	case EWeaponName::DS39_7_62MM:
		return 497;
	case EWeaponName::DP_28_7_62MM:
		return 496;
	case EWeaponName::MP46: // Very powerful SMG, treated as low HMG tier
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
	case EWeaponName::Korovin_M1941_7_62MM:
		return 305;
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
	case EWeaponName::KwK39_Rockets:
		return 1;

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
	case EWeaponName::AM_23:
		return "AM_23";
	case EWeaponName::KwK38_T_37MM:
		return "KwK38_T_37MM";
	case EWeaponName::KwK39_1_50MM:
		return "KwK39_1_50MM";
	case EWeaponName::KwK39_Rockets:
		return "KwK39_Rockets";
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
	case EWeaponName::Ger_TankMG_34B:
		return "Ger_TankMG_34B";
	case EWeaponName::Luch_50L:
		return "Luch_50L";
	case EWeaponName::Luch_85L:
		return "Luch_85L";
	case EWeaponName::Zarya_100L:
		return "Zarya_100L";
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
	case EWeaponName::D_5S_85MM_SU85:
		return "D_5S_85MM_SU85";
	case EWeaponName::ZIS_3_76MM:
		return "ZIS_3_76MM";
	case EWeaponName::ZIS_3_76MM_SU76:
		return "ZIS_3_76MM_SU76";
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
	case EWeaponName::D_10S_100MM_SU100:
		return "D_10S_100MM_SU100";
	case EWeaponName::D_25T_122MM:
		return "D_25T_122MM";
	case EWeaponName::D_25T_122MM_IS3:
		return "D_25T_122MM";
	case EWeaponName::FlammIS3:
		return "FlammIS3";
	case EWeaponName::M_30S_122MM_SU122:
		return "M_30S_122MM_SU122";
	case EWeaponName::M1938_122MM:
		return "M1938_122MM";
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
	case EWeaponName::Rocket_30mm:
		return "Rocket_30mm";
	case EWeaponName::Rocket_50mm:
		return "Rocket_50mm";
	case EWeaponName::sIG_33_150MM:
		return "sIG_33_150MM";
	case EWeaponName::Panzerwerfer:
		return "Panzerwerfer";
	case EWeaponName::RW61_Mortar_380MM:
		return "RW61_Mortar_380MM";
	case EWeaponName::Mortar_120MM:
		return "Mortar_120MM";
	case EWeaponName::Mortar_80MM:
		return "Mortar_80MM";
	case EWeaponName::Mortar_40MM:
		return "Mortar_40MM";
	case EWeaponName::Panzerwerfer_Small:
		return "Panzerwerfer_Small";
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
	case EWeaponName::Morser_18_210MM_TW:
		return "Morser_18_210MM_TW";
	case EWeaponName::TM_3_12_305MM:
		return "TM_3_12_305MM";
	case EWeaponName::B4_203MM_TW:
		return "B4_203MM_TW";
	case EWeaponName::M53K_45MM_TW:
		return "M53K_45MM_TW";
	case EWeaponName::ML20_152MM_TW:
		return "ML20_152MM_TW";
	case EWeaponName::ML20_152MM_Pit:
		return "ML20_152MM_Pit";
	case EWeaponName::BS3_100MM_TW:
		return "BS3_100MM_TW";
	case EWeaponName::Maxim_M1910_7_62MM_TW:
		return "Maxim_M1910_7_62MM_TW";
	case EWeaponName::KwK38_20MM_Luchs:
		return "KwK38_20MM_Luchs";
	case EWeaponName::Flakvierling38_20MM:
		return "Flakvierling38_20MM";
	case EWeaponName::Kwk30_20MM_sdkfz221:
		return "Kwk30_20MM_sdkfz221";
	case EWeaponName::Pak42_L70_75MM:
		return "Pak42_L70_75MM";
	case EWeaponName::KwK_25MM_APCR_Pz38t:
		return "KwK_25MM_APCR_Pz38t";
	case EWeaponName::GerRailSniper20MM:
		return "GerRailSniper20MM";
	case EWeaponName::KwK37_75MM_HE_Sdkfz250:
		return "KwK37_75MM_HE_Sdkfz250";
	case EWeaponName::MG151_15MM_Twin:
		return "MG151_15MM_Twin";
	case EWeaponName::PzI_Bison_L60_50MM:
		return "PzI_Bison_L60_50MM";
	case EWeaponName::Ger_Twin40MM_Marder:
		return "Ger_Twin40MM_Marder";
	case EWeaponName::PzIVG_DT_50MM_Artillery:
		return "PzIVG_DT_50MM_Artillery";
	case EWeaponName::Mortar_100MM_JPzIVSiege:
		return "Mortar_100MM_JPzIVSiege";
	case EWeaponName::Pak42_L70_75MM_APCR:
		return "Pak42_L70_75MM_APCR";
	case EWeaponName::StuH_200MM_DirectSiege:
		return "StuH_200MM_DirectSiege";
	case EWeaponName::Flak43_37MM_Ostwind:
		return "Flak43_37MM_Ostwind";
	case EWeaponName::Rocket_320mm_Incendiary:
		return "Rocket_320mm_Incendiary";
	case EWeaponName::RadixiteHail_Rocket_76MM:
		return "RadixiteHail_Rocket_76MM";
	case EWeaponName::E25_Hunter_60MM_APCR:
		return "E25_Hunter_60MM_APCR";
	case EWeaponName::E25_Assault_90MM_HE:
		return "E25_Assault_90MM_HE";
	case EWeaponName::KwK40_L48_75MM_HEAT_TigerTwin:
		return "KwK40_L48_75MM_HEAT_TigerTwin";
	case EWeaponName::TigerRail_100MM_Railgun:
		return "TigerRail_100MM_Railgun";
	case EWeaponName::Ger_2xTwin35MM_TigerII:
		return "Ger_2xTwin35MM_TigerII";
	case EWeaponName::Maus_AA_88MM_Flak:
		return "Maus_AA_88MM_Flak";
	case EWeaponName::MG151_15MM_Drilling:
		return "MG151_15MM_Drilling";
	case EWeaponName::Mortar_50MM_140Bunker:
		return "Mortar_50MM_140Bunker";
	case EWeaponName::MK103_30MM_PumaDual:
		return "MK103_30MM_PumaDual";
	case EWeaponName::RadixiteSniper_50MM_Laser:
		return "RadixiteSniper_50MM_Laser";
	case EWeaponName::KwK30_Radixite_20MM:
		return "KwK30_Radixite_20MM";
	case EWeaponName::KwK42_Radixite_50MM:
		return "KwK42_Radixite_50MM";
	case EWeaponName::Rocket_80mm_PzIIIRack:
		return "Rocket_80mm_PzIIIRack";
	case EWeaponName::KwK37_75MM_Napalm:
		return "KwK37_75MM_Napalm";
	case EWeaponName::FlameCharge_50MM_234:
		return "FlameCharge_50MM_234";
	case EWeaponName::Flak38_20MM_Drilling:
		return "Flak38_20MM_Drilling";
	case EWeaponName::Flak41_50MM:
		return "Flak41_50MM";
	case EWeaponName::Rocket_150mm_Incendiary:
		return "Rocket_150mm_Incendiary";
	case EWeaponName::Rocket_200mm_PrecisionMissile:
		return "Rocket_200mm_PrecisionMissile";
	case EWeaponName::LeFH_Radixite_200MM:
		return "LeFH_Radixite_200MM";
	case EWeaponName::Laser_60MM_PzIIIM:
		return "Laser_60MM_PzIIIM";
	case EWeaponName::PanzerSchreck_PzIIIM:
		return "PanzerSchreck_PzIIIM";
	case EWeaponName::Flamm15_HetzerHull:
		return "Flamm15_HetzerHull";
	case EWeaponName::Mortar_120MM_Hetzer:
		return "Mortar_120MM_Hetzer";
	case EWeaponName::Pak43_88MM_APCR_Pfeil:
		return "Pak43_88MM_APCR_Pfeil";
	case EWeaponName::sFH18_150MM_Bollwerk:
		return "sFH18_150MM_Bollwerk";
	case EWeaponName::KwK44_88MM_E50:
		return "KwK44_88MM_E50";
	case EWeaponName::T44_155MM_Howitzer:
		return "T44_155MM_Howitzer";
	case EWeaponName::Ger_PitMG42_7_92MM:
		return "Ger_PitMG42_7_92MM";
	case EWeaponName::L_11_76MM_SMK:
		return "L_11_76MM_SMK";
	case EWeaponName::Flak36_37MM_Pit:
		return "Flak36_37MM_Pit";
	case EWeaponName::Raketenwerfer43_88MM:
		return "Raketenwerfer43_88MM";
	case EWeaponName::PanzerSchreck_Pit:
		return "PanzerSchreck_Pit";
	case EWeaponName::B4_203MM_Pit:
		return "B4_203MM_Pit";
	case EWeaponName::Ampulomet_125MM:
		return "Ampulomet_125MM";
	case EWeaponName::ROKS3_Flamethrower:
		return "ROKS3_Flamethrower";
	case EWeaponName::Korovin_M1941_7_62MM:
		return "Korovin_M1941_7_62MM";
	case EWeaponName::DS39_7_62MM:
		return "DS39_7_62MM";
	case EWeaponName::Dyakonov_RGL_41MM:
		return "Dyakonov_RGL_41MM";
	case EWeaponName::ProtoCoilgun_20MM:
		return "ProtoCoilgun_20MM";

	default:
		{
			const auto WeaponEnum = StaticEnum<EWeaponName>();
			if (WeaponEnum)
			{
				const FName Name = WeaponEnum->GetNameByValue(static_cast<int64>(WeaponName));
				const FName CleanName = FName(*Name.ToString().RightChop(FString("EWeaponName::").Len()));
				return CleanName.ToString();
			}
			return "INVALID WEAPON ENUM";
		}
	}
}
