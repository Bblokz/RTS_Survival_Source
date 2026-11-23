// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionObjectivePlateSettings.h"

#include "Engine/Font.h"
#include "Materials/MaterialInterface.h"

namespace MissionObjectivePlateDefaults
{
	const TCHAR* const PlateMaterialPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/"
		"MI9_RTS_T_Sidebar_TopPanel_128x128_u_n.MI9_RTS_T_Sidebar_TopPanel_128x128_u_n");
	const TCHAR* const MajorShieldPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates/MI_ObjectiveShield_Major.MI_ObjectiveShield_Major");
	const TCHAR* const BonusShieldPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates/MI_ObjectiveShield_Bonus.MI_ObjectiveShield_Bonus");
	const TCHAR* const MajorIconPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates/MI_ObjectiveGlassIcon_Star.MI_ObjectiveGlassIcon_Star");
	const TCHAR* const BonusIconPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates/MI_ObjectiveGlassIcon_Diamond.MI_ObjectiveGlassIcon_Diamond");
	const TCHAR* const KnobPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Missions/MissionUI/ObjectivePlates/M_UI_ObjectiveKnob.M_UI_ObjectiveKnob");
	const TCHAR* const ButtonNormalPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/"
		"MI9_Theme01_T_Btn06_n_368x138_f_n.MI9_Theme01_T_Btn06_n_368x138_f_n");
	const TCHAR* const ButtonHoveredPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/"
		"MI9_Theme01_T_Btn06_p_368x138_f_h.MI9_Theme01_T_Btn06_p_368x138_f_h");
	const TCHAR* const ButtonPressedPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/Skins/13_Frontstahl_Gunmetal/Slice/Instances/"
		"MI9_Theme01_T_Btn06_p_368x138_f_p.MI9_Theme01_T_Btn06_p_368x138_f_p");
	const TCHAR* const LabelFontPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");

	// sRGB hex from the A1-c sketch (steel blue major, signal green bonus, bone text).
	const TCHAR* const MajorLabelHex = TEXT("92BCE6");
	const TCHAR* const BonusLabelHex = TEXT("86CF9A");
	const TCHAR* const ButtonTextHex = TEXT("DEDACD");
}

UMissionObjectivePlateSettings::UMissionObjectivePlateSettings()
{
	using namespace MissionObjectivePlateDefaults;
	PlateMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(PlateMaterialPath));
	MajorShieldMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MajorShieldPath));
	BonusShieldMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(BonusShieldPath));
	MajorIconMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MajorIconPath));
	BonusIconMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(BonusIconPath));
	KnobMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(KnobPath));
	ConfirmButtonNormal = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ButtonNormalPath));
	ConfirmButtonHovered = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ButtonHoveredPath));
	ConfirmButtonPressed = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ButtonPressedPath));
	LabelFont = TSoftObjectPtr<UFont>(FSoftObjectPath(LabelFontPath));
	MajorLabelColor = FLinearColor::FromSRGBColor(FColor::FromHex(MajorLabelHex));
	BonusLabelColor = FLinearColor::FromSRGBColor(FColor::FromHex(BonusLabelHex));
	ButtonTextColor = FLinearColor::FromSRGBColor(FColor::FromHex(ButtonTextHex));
	TitleColor = FLinearColor::FromSRGBColor(FColor::FromHex(ButtonTextHex));
}
