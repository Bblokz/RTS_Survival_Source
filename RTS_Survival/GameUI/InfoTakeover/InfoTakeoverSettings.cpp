// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfoTakeoverSettings.h"

#include "Engine/DataTable.h"
#include "W_CameraControlsTakeover.h"
#include "W_CoverTakeover.h"
#include "W_NomadicConversionTakeover.h"
#include "W_BxpPackUpTakeover.h"
#include "W_CapturingTrucksTakeover.h"
#include "W_DocumentTakeover.h"
#include "W_SpacedArmourTakeover.h"
#include "W_VehicleInfoTakeover.h"
#include "W_RattleFeedTakeover.h"
#include "W_WespeBarrageTakeover.h"
#include "W_DigInTakeover.h"
#include "W_DirectionFindingTakeover.h"
#include "W_WallBreachTakeover.h"
#include "W_AdrenalineShotTakeover.h"
#include "W_VulturesAtTakeover.h"

UInfoTakeoverSettings::UInfoTakeoverSettings()
{
	VehicleTakeoverWidgetClass = TSoftClassPtr<UW_VehicleInfoTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WBP_VehicleInfoTakeover.WBP_VehicleInfoTakeover_C")));
	VehicleLibrary = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_VehicleInfoTakeovers.DT_VehicleInfoTakeovers")));
	SquadLibrary = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_SquadInfoTakeovers.DT_SquadInfoTakeovers")));
	CameraControlsTakeoverWidgetClass = TSoftClassPtr<UW_CameraControlsTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WBP_CameraControlsTakeover.WBP_CameraControlsTakeover_C")));
	CoverTakeoverWidgetClass = TSoftClassPtr<UW_CoverTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Cover/WBP_CoverTakeover.WBP_CoverTakeover_C")));
	SpacedArmourTakeoverWidgetClass = TSoftClassPtr<UW_SpacedArmourTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/SpacedArmour/WBP_SpacedArmourTakeover."
		"WBP_SpacedArmourTakeover_C")));
	NomadicConversionTakeoverWidgetClass = TSoftClassPtr<UW_NomadicConversionTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/NomadicConversion/WBP_NomadicConversionTakeover."
		"WBP_NomadicConversionTakeover_C")));
	BxpPackUpTakeoverWidgetClass = TSoftClassPtr<UW_BxpPackUpTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/BxpPackUp/WBP_BxpPackUpTakeover.WBP_BxpPackUpTakeover_C")));
	VulturesAtTakeoverWidgetClass = TSoftClassPtr<UW_VulturesAtTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/VulturesAt/WBP_VulturesAtTakeover.WBP_VulturesAtTakeover_C")));
	RattleFeedTakeoverWidgetClass = TSoftClassPtr<UW_RattleFeedTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/RattleFeed/WBP_RattleFeedTakeover.WBP_RattleFeedTakeover_C")));
	WespeBarrageTakeoverWidgetClass = TSoftClassPtr<UW_WespeBarrageTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WespeBarrage/WBP_WespeBarrageTakeover.WBP_WespeBarrageTakeover_C")));
	DigInTakeoverWidgetClass = TSoftClassPtr<UW_DigInTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DigIn/WBP_DigInTakeover.WBP_DigInTakeover_C")));
	WallBreachTakeoverWidgetClass = TSoftClassPtr<UW_WallBreachTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/WallBreach/WBP_WallBreachTakeover."
		"WBP_WallBreachTakeover_C")));
	AdrenalineShotTakeoverWidgetClass = TSoftClassPtr<UW_AdrenalineShotTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/AdrenalineShot/WBP_AdrenalineShotTakeover.WBP_AdrenalineShotTakeover_C")));
	CapturingTrucksTakeoverWidgetClass = TSoftClassPtr<UW_CapturingTrucksTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/CapturingTrucks/WBP_CapturingTrucksTakeover."
		"WBP_CapturingTrucksTakeover_C")));
	DirectionFindingTakeoverWidgetClass = TSoftClassPtr<UW_DirectionFindingTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DirectionFinding/WBP_DirectionFindingTakeover."
		"WBP_DirectionFindingTakeover_C")));
	DocumentTakeoverWidgetClass = TSoftClassPtr<UW_DocumentTakeover>(FSoftObjectPath(TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Document/WBP_DocumentTakeover.WBP_DocumentTakeover_C")));
}
