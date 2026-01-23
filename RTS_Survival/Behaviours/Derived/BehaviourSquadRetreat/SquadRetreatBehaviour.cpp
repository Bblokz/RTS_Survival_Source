// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadRetreatBehaviour.h"

#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

namespace SquadRetreatBehaviourConstants
{
	constexpr float RetreatMovementMultiplier = 1.5f;
}

USquadRetreatBehaviour::USquadRetreatBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourMovementMultipliers.MaxWalkSpeedMultiplier = SquadRetreatBehaviourConstants::RetreatMovementMultiplier;
	BehaviourMovementMultipliers.MaxAccelerationMultiplier = SquadRetreatBehaviourConstants::RetreatMovementMultiplier;
	BehaviourIcon = EBehaviourIcon::FallbackToHQ;

	bM_UsesTick = true;
	AnimatedTextSettings.RepeatInterval = 5.f;
	AnimatedTextSettings.RepeatStrategy = EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats;
	AnimatedTextSettings.TextSettings.bUseText = true;
	AnimatedTextSettings.TextSettings.TextOnSubjects = FRTSRichTextConverter::MakeRTSRich(
		"Falling Back", ERTSRichText::Text_Bad14);
	AnimatedTextSettings.TextSettings.bAutoWrap = false;
	AnimatedTextSettings.TextSettings.InJustification = ETextJustify::Center;
	AnimatedTextSettings.TextSettings.TextOffset = FVector(0.f, 0.f, 350.f);
	AnimatedTextSettings.TextSettings.InSettings.VisibleDuration = 1.5f;
	AnimatedTextSettings.TextSettings.InSettings.FadeOutDuration = 0.5f;
	AnimatedTextSettings.TextSettings.InSettings.DeltaZ = 75.f;
}
