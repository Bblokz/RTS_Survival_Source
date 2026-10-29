// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMenuBattleArena.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"

namespace RTSMenuBattleArenaPrivate
{
	constexpr float ArrowLengthCm = 400.f;
	constexpr float ArrowScale = 8.f;
}

ARTSMenuBattleArena::ARTSMenuBattleArena()
{
	PrimaryActorTick.bCanEverTick = false;
	M_ArenaRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaRoot"));
	SetRootComponent(M_ArenaRoot);
#if WITH_EDITORONLY_DATA
	M_BattleDirectionArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("BattleDirection"));
	if (M_BattleDirectionArrow)
	{
		M_BattleDirectionArrow->SetupAttachment(M_ArenaRoot);
		M_BattleDirectionArrow->ArrowLength = RTSMenuBattleArenaPrivate::ArrowLengthCm;
		M_BattleDirectionArrow->ArrowSize = RTSMenuBattleArenaPrivate::ArrowScale;
	}
	M_Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (M_Billboard)
	{
		M_Billboard->SetupAttachment(M_ArenaRoot);
	}
#endif
}

FVector ARTSMenuBattleArena::GetBattleForward() const
{
	return GetActorForwardVector().GetSafeNormal2D();
}

FVector ARTSMenuBattleArena::GetLineCenter(const ERTSMenuBattleSide Side) const
{
	const float HalfSeparation = M_Settings.LineSeparation * 0.5f;
	const float Sign = Side == ERTSMenuBattleSide::German ? -1.f : 1.f;
	return GetActorLocation() + GetBattleForward() * HalfSeparation * Sign;
}

float ARTSMenuBattleArena::GetFacingYaw(const ERTSMenuBattleSide Side) const
{
	const float ForwardYaw = GetBattleForward().Rotation().Yaw;
	return Side == ERTSMenuBattleSide::German ? ForwardYaw : ForwardYaw + 180.f;
}

FVector ARTSMenuBattleArena::GetSlotLocation(const FRTSMenuBattleRosterEntry& Entry) const
{
	// 0 in the middle, then +1, -1, +2, -2 spacings to the side.
	const int32 SlotIndex = Entry.SlotIndex;
	const float LateralSlot = SlotIndex == 0
		                          ? 0.f
		                          : (SlotIndex % 2 == 1 ? 1.f : -1.f) * static_cast<float>((SlotIndex + 1) / 2);
	const FVector Forward = GetBattleForward();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	// Each side's ranks stack away from the enemy; reinforcements come in from further back.
	const FVector AwayFromEnemy = Entry.Side == ERTSMenuBattleSide::German ? -Forward : Forward;
	const float Depth = GetRankDistance(Entry.Kind) + (Entry.Wave > 0 ? M_Settings.ReinforcementDepth : 0.f)
		+ (Entry.bDrivesIn ? M_Settings.ApproachDistance : 0.f) + Entry.SlotJitter.Y;
	// Pits are spread wider and sit between the vehicle slots, so the line's tanks do not park on them.
	constexpr float PitSpacingScale = 1.7f;
	constexpr float PitSlotOffset = 0.5f;
	const float Lateral = Entry.Kind == ERTSMenuBattleUnitKind::Pit
		                      ? (LateralSlot * PitSpacingScale + PitSlotOffset) * M_Settings.SlotSpacing + Entry.SlotJitter.X
		                      : LateralSlot * M_Settings.SlotSpacing + Entry.SlotJitter.X;
	return GetLineCenter(Entry.Side) + AwayFromEnemy * Depth + Right * Lateral;
}

FVector ARTSMenuBattleArena::GetFormUpLocation(const FRTSMenuBattleRosterEntry& Entry) const
{
	FRTSMenuBattleRosterEntry OnTheLine = Entry;
	OnTheLine.bDrivesIn = false;
	return GetSlotLocation(OnTheLine);
}

float ARTSMenuBattleArena::GetRankDistance(const ERTSMenuBattleUnitKind Kind) const
{
	switch (Kind)
	{
	case ERTSMenuBattleUnitKind::Vehicle:
		return 0.f;
	case ERTSMenuBattleUnitKind::Squad:
		return M_Settings.RankSpacing;
	case ERTSMenuBattleUnitKind::Aircraft:
		return M_Settings.RankSpacing * 2.f;
	case ERTSMenuBattleUnitKind::Pit:
		// Dug in a little ahead of the line the vehicles form up on.
		return -M_Settings.RankSpacing * 0.4f;
	}
	return 0.f;
}
