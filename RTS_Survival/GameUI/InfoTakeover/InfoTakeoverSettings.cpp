// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfoTakeoverSettings.h"

#include "Engine/DataTable.h"
#include "W_CameraControlsTakeover.h"
#include "W_CoverTakeover.h"
#include "W_SpacedArmourTakeover.h"
#include "W_VehicleInfoTakeover.h"

UInfoTakeoverSettings::UInfoTakeoverSettings()
{
	VehicleTakeoverWidgetClass = TSoftClassPtr<UW_VehicleInfoTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WBP_VehicleInfoTakeover.WBP_VehicleInfoTakeover_C")));
	VehicleLibrary = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_VehicleInfoTakeovers.DT_VehicleInfoTakeovers")));
	CameraControlsTakeoverWidgetClass = TSoftClassPtr<UW_CameraControlsTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WBP_CameraControlsTakeover.WBP_CameraControlsTakeover_C")));
	CoverTakeoverWidgetClass = TSoftClassPtr<UW_CoverTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Cover/WBP_CoverTakeover.WBP_CoverTakeover_C")));
	SpacedArmourTakeoverWidgetClass = TSoftClassPtr<UW_SpacedArmourTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/SpacedArmour/WBP_SpacedArmourTakeover."
		"WBP_SpacedArmourTakeover_C")));
}
