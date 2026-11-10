// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

/** Defaults shared by the portrait subtitle box and the cinematic overlay so both look the same. */
namespace SubtitleStyleDefaults
{
	constexpr int32 SpeakerFontSize = 11;
	constexpr int32 LineFontSize = 14;
	// The game's main font (composite), used for the short speaker name; loaded with ConstructorHelpers.
	inline const TCHAR* MainFontPath = TEXT("/Game/RTS_Survival/Fonts/RR_mainFont.RR_mainFont");
	// Plain sans-serif for the spoken words: the main font is an all-caps display face that is hard to read
	// in full sentences.
	inline const TCHAR* ReadableFontPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");
	inline const TCHAR* ReadableTypeface = TEXT("Regular");

	/** Dark, slightly transparent rounded panel with a thin teal outline. */
	RTS_SURVIVAL_API FSlateBrush MakePanelBrush();
}
