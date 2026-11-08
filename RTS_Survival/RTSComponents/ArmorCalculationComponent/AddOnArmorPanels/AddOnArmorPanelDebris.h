// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AddOnArmorPanelLayout.h"

/**
 * Shot-off panels fly as pooled Niagara mesh debris (the winner of Docs/Units/Vehicles/AddOnArmorBreakTest.md).
 * NS_AddOnArmorPanelDebris is built by RTS.AddOnArmor.BuildDebrisSystem; its motion is DebrisHlsl/*.hlsl.
 * Per burst the panels component sets the user parameters below, one array entry per thrown panel.
 */
namespace AddOnArmorPanelDebris
{
	inline const TCHAR* SystemPackage = TEXT("/Game/RTS_Survival/SpecialEffects/AddOnArmor/NS_AddOnArmorPanelDebris");
	inline const TCHAR* SystemPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AddOnArmor/NS_AddOnArmorPanelDebris.NS_AddOnArmorPanelDebris");

	// Static mesh user parameter: the hull's debris panel (UAddOnArmorPanelLayout::GetDebrisMesh).
	inline const FName DebrisMeshParameter(TEXT("DebrisMesh"));
	inline const FName PieceCountParameter(TEXT("PieceCount"));
	// Float4 arrays: start (xyz world position, w uniform scale), velocity (xyz), spin (xyz axis, w rad/s),
	// orientation (quaternion xyzw).
	inline const FName PieceStartParameter(TEXT("PieceStart"));
	inline const FName PieceVelocityParameter(TEXT("PieceVelocity"));
	inline const FName PieceSpinParameter(TEXT("PieceSpin"));
	inline const FName PieceOrientationParameter(TEXT("PieceOrientation"));
	// World Z of the ground plane the pieces bounce on and come to rest on.
	inline const FName GroundHeightParameter(TEXT("GroundHeight"));

	// Pieces fly and lie whole for DebrisSecondsBeforeShrink, then shrink away over DebrisShrinkSeconds.
	constexpr float DebrisSecondsBeforeShrink = 3.f;
	constexpr float DebrisShrinkSeconds = 0.6f;

	// A destroyed zone throws all of its panels in one burst; the burst is bound to PieceCount.
	constexpr int32 MaxPiecesPerBurst = AddOnArmorPanelMask::MaxPanelsPerMesh;
}
