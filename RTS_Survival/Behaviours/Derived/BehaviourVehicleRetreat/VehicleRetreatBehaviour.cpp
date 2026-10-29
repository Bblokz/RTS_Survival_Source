// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleRetreatBehaviour.h"

#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

namespace VehicleRetreatBehaviourConstants
{
	constexpr float TextRepeatInterval = 8.f;
	constexpr float TextOffsetZ = 350.f;
	constexpr float TextVisibleDuration = 1.5f;
	constexpr float TextFadeOutDuration = 0.5f;
	constexpr float TextDeltaZ = 75.f;
}

UVehicleRetreatBehaviour::UVehicleRetreatBehaviour()
{
	using namespace VehicleRetreatBehaviourConstants;
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourIcon = EBehaviourIcon::FallbackToHQ;
	M_TitleText = TEXT("Retreating");
	M_DisplayText = TEXT("Badly damaged; reversing to a base for repairs.");
	M_BuffType = EBuffDebuffType::Debuff;

	AnimatedTextSettings.RepeatInterval = TextRepeatInterval;
	AnimatedTextSettings.RepeatStrategy = EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats;
	AnimatedTextSettings.TextSettings.bUseText = true;
	AnimatedTextSettings.TextSettings.TextOnSubjects = FRTSRichTextConverter::MakeRTSRich(
		"Retreating", ERTSRichText::Text_Bad14);
	AnimatedTextSettings.TextSettings.bAutoWrap = false;
	AnimatedTextSettings.TextSettings.InJustification = ETextJustify::Center;
	AnimatedTextSettings.TextSettings.TextOffset = FVector(0.f, 0.f, TextOffsetZ);
	AnimatedTextSettings.TextSettings.InSettings.VisibleDuration = TextVisibleDuration;
	AnimatedTextSettings.TextSettings.InSettings.FadeOutDuration = TextFadeOutDuration;
	AnimatedTextSettings.TextSettings.InSettings.DeltaZ = TextDeltaZ;
}
