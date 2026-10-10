// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfantryStressBehaviour.h"

#include "Engine/World.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace InfantryStressBehaviourConstants
{
	// A stressed soldier empties a magazine three times the size, four times as fast, reloads four times as
	// fast and hits a quarter as often.
	constexpr float MagSizeMultiplier = 3.0f;
	constexpr float CooldownMultiplier = 0.25f;
	constexpr float ReloadTimeMultiplier = 0.25f;
	constexpr float AccuracyMultiplier = 0.25f;
}

UInfantryStressBehaviour::UInfantryStressBehaviour()
{
	using namespace InfantryStressBehaviourConstants;
	BehaviourWeaponMultipliers.MagSizeMlt = MagSizeMultiplier;
	BehaviourWeaponMultipliers.BaseCooldownMlt = CooldownMultiplier;
	BehaviourWeaponMultipliers.ReloadSpeedMlt = ReloadTimeMultiplier;
	BehaviourWeaponMultipliers.AccuracyMlt = AccuracyMultiplier;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Stressed");
	M_DisplayText = TEXT("Sprays blind fire, then runs for cover further from the enemy.");
}

void UInfantryStressBehaviour::StartOnSoldier(ASquadUnit& Soldier)
{
	M_Soldier = &Soldier;
	UWeaponState* SoldierWeapon = Soldier.GetWeaponState();
	if (IsValid(SoldierWeapon) && CheckRequirement(SoldierWeapon))
	{
		ApplyBehaviourToWeapon(SoldierWeapon);
		M_StressedWeapon = SoldierWeapon;
		// The larger magazine would otherwise only be there after the next reload.
		SoldierWeapon->ForceInstantReload();
	}
	StartSoldierFeedback(Soldier);
	BP_OnSoldierStressed(&Soldier);
}

bool UInfantryStressBehaviour::GetAppliesToWeapon(UWeaponState* Weapon) const
{
	return IsValid(Weapon) && CheckRequirement(Weapon);
}

void UInfantryStressBehaviour::StartSoldierFeedback(const ASquadUnit& Soldier)
{
	// The same rules as the squad's behaviour component: icons take precedence over text.
	const bool bUsesIcons = AnimatedIconSettings.IconSettings.bUseIcons;
	const bool bRepeatsWithoutEnd = bUsesIcons
		? AnimatedIconSettings.RepeatStrategy == EBehaviourRepeatedVerticalIconStrategy::InfiniteRepeats
		: AnimatedTextSettings.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats;
	const int32 AmountRepeats = bUsesIcons ? AnimatedIconSettings.AmountRepeats : AnimatedTextSettings.AmountRepeats;
	const UWorld* World = Soldier.GetWorld();
	M_FeedbackRepeatSeconds = bUsesIcons ? AnimatedIconSettings.RepeatInterval : AnimatedTextSettings.RepeatInterval;
	M_NextFeedbackWorldSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.0f;
	M_RemainingFeedbackShowings = bRepeatsWithoutEnd ? INDEX_NONE : FMath::Max(1, AmountRepeats);
	UpdateSoldierFeedback(M_NextFeedbackWorldSeconds);
}

void UInfantryStressBehaviour::UpdateSoldierFeedback(const float WorldSeconds)
{
	ASquadUnit* Soldier = M_Soldier.Get();
	if (M_RemainingFeedbackShowings == 0 || WorldSeconds < M_NextFeedbackWorldSeconds || not IsValid(Soldier))
	{
		return;
	}
	// Nothing to show, or no interval to repeat it on: this was the last time.
	if (not ShowFeedbackOverSoldier(*Soldier) || M_FeedbackRepeatSeconds <= 0.0f)
	{
		M_RemainingFeedbackShowings = 0;
		return;
	}
	M_NextFeedbackWorldSeconds = WorldSeconds + M_FeedbackRepeatSeconds;
	if (M_RemainingFeedbackShowings > 0)
	{
		--M_RemainingFeedbackShowings;
	}
}

bool UInfantryStressBehaviour::ShowFeedbackOverSoldier(ASquadUnit& Soldier) const
{
	const FBehaviourIconSettings& IconSettings = AnimatedIconSettings.IconSettings;
	if (IconSettings.bUseIcons)
	{
		UAnimatedIconWidgetPoolManager* IconPoolManager = FRTS_Statics::GetVerticalAnimatedIconWidgetPoolManager(&Soldier);
		if (not IsValid(IconPoolManager) || IconSettings.IconType == ERTSVerticalAnimatedIcon::None)
		{
			return false;
		}
		return IconSettings.bOverrideAnimationSettings
			? IconPoolManager->ShowAnimatedIconAttachedToActorWithSettings(
				IconSettings.IconType, &Soldier, IconSettings.LocalOffset, IconSettings.AnimationSettings)
			: IconPoolManager->ShowAnimatedIconAttachedToActor(
				IconSettings.IconType, &Soldier, IconSettings.LocalOffset);
	}
	const FBehaviourTextSettings& TextSettings = AnimatedTextSettings.TextSettings;
	UAnimatedTextWidgetPoolManager* TextPoolManager = TextSettings.bUseText
		? FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(&Soldier)
		: nullptr;
	if (not IsValid(TextPoolManager))
	{
		return false;
	}
	return TextPoolManager->ShowAnimatedTextAttachedToActor(
		TextSettings.TextOnSubjects, &Soldier, TextSettings.TextOffset, TextSettings.bAutoWrap,
		TextSettings.InWrapAt, TextSettings.InJustification, TextSettings.InSettings);
}

void UInfantryStressBehaviour::EndOnSoldier()
{
	if (UWeaponState* StressedWeapon = M_StressedWeapon.Get())
	{
		RemoveBehaviourFromWeapon(StressedWeapon);
	}
	M_StressedWeapon.Reset();
	M_RemainingFeedbackShowings = 0;
	BP_OnSoldierCalmedDown(M_Soldier.Get());
	M_Soldier.Reset();
}

void UInfantryStressBehaviour::NotifySoldierRelocates()
{
	if (ASquadUnit* Soldier = M_Soldier.Get())
	{
		BP_OnSoldierRelocates(Soldier);
	}
}
