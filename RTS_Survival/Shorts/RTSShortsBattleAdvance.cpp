// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsAdvance, Log, All);

namespace RTSShortsAdvancePrivate
{
	// A vehicle slower than this, this long, at least this long after its order, has finished its move.
	constexpr float StationarySpeedCm = 40.f;
	constexpr float StoppedCheckAfterSeconds = 1.5f;
	constexpr float StoppedForSeconds = 0.6f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}
}

void ARTSShortsDirector::BeginBattleAdvance(const FRTSShortsSegment& Segment)
{
	const FRTSShortsBattleAdvance& Settings = Segment.BattleAdvance;
	if (not Settings.bEnabled || not GetIsValidStage())
	{
		return;
	}
	const FVector Forward = M_Stage->GetActorForwardVector().GetSafeNormal2D();
	auto AddLine = [&](const TArray<TWeakObjectPtr<AActor>>& Line, const bool bEnemy)
	{
		int32 LineIndex = 0;
		for (const TWeakObjectPtr<AActor>& Unit : Line)
		{
			// Vehicles only: squads and their soldiers move through their own orders. Reinforcements keep their own drive.
			if (not Unit.IsValid() || Cast<ASquadController>(Unit.Get()) != nullptr || Cast<ASquadUnit>(Unit.Get()) != nullptr
				|| M_Battle.Reinforcements.Contains(Unit))
			{
				continue;
			}
			FRTSShortsAdvanceState Advance;
			Advance.Unit = Unit;
			Advance.bEnemy = bEnemy;
			const float Distance = bEnemy ? -Settings.EnemyAdvanceCm : Settings.PlayerAdvanceCm;
			Advance.Destination = GetBattleGroundPoint(Unit->GetActorLocation() + Forward * Distance);
			Advance.OrderAtSeconds = Settings.OrderAtSeconds + Settings.OrderStaggerSeconds * LineIndex;
			M_Battle.Advances.Add(Advance);
			M_Battle.HeldUnits.Add(Unit);
			++LineIndex;
		}
	};
	AddLine(M_Battle.PlayerUnits, false);
	AddLine(M_Battle.EnemyUnits, true);
	if (GetIsValidCamera())
	{
		M_Battle.AdvanceCameraStart = M_Camera->GetActorLocation();
	}
	GetBattleAdvanceAnchor(M_Battle.AdvanceAnchorStart);
	M_Battle.AdvanceAnchor = M_Battle.AdvanceAnchorStart;
	UE_LOG(LogRTSShortsAdvance, Display, TEXT("RTS_SHORTS_ADVANCE_BEGIN vehicles=%d playerCm=%.0f enemyCm=%.0f orderAt=%.1f"),
	       M_Battle.Advances.Num(), Settings.PlayerAdvanceCm, Settings.EnemyAdvanceCm, Settings.OrderAtSeconds);
}

void ARTSShortsDirector::TickBattleAdvance(const int32 Frame)
{
	using namespace RTSShortsAdvancePrivate;
	const FRTSShortsBattleAdvance& Settings = M_Job.Segments[M_CurrentSegment].BattleAdvance;
	if (not Settings.bEnabled)
	{
		return;
	}
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	for (FRTSShortsAdvanceState& Advance : M_Battle.Advances)
	{
		if (Advance.bReleased)
		{
			continue;
		}
		if (not GetIsAlive(Advance.Unit))
		{
			Advance.bReleased = true;
			M_Battle.HeldUnits.Remove(Advance.Unit);
			continue;
		}
		if (not Advance.bOrdered)
		{
			if (SegmentSeconds >= Advance.OrderAtSeconds)
			{
				OrderBattleAdvanceMove(Advance, Frame);
			}
			continue;
		}
		if (GetHasBattleAdvanceFinished(Advance, Settings, SegmentSeconds))
		{
			ReleaseBattleAdvanceUnit(Advance, Frame, SegmentSeconds);
		}
	}
	UpdateAdvanceFollowCamera(Settings);
}

void ARTSShortsDirector::OrderBattleAdvanceMove(FRTSShortsAdvanceState& Advance, const int32 Frame)
{
	Advance.bOrdered = true;
	AActor* Unit = Advance.Unit.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	const FRotator Facing = (Advance.Destination - Unit->GetActorLocation()).GetSafeNormal2D().Rotation();
	const ECommandQueueError Result = Commands != nullptr
		? Commands->MoveToLocation(Advance.Destination, false, Facing, false) : ECommandQueueError::NoError;
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), Unit->GetActorLocation(),
	                                                                                   Advance.Destination, Unit);
	UE_LOG(LogRTSShortsAdvance, Display,
	       TEXT("RTS_SHORTS_ADVANCE_ORDER frame=%d unit=%s enemy=%d commands=%d result=%d pathValid=%d length=%.0f"), Frame,
	       *Unit->GetName(), Advance.bEnemy ? 1 : 0, Commands != nullptr ? 1 : 0, static_cast<int32>(Result),
	       Path != nullptr && Path->IsValid() ? 1 : 0, Path != nullptr ? Path->GetPathLength() : 0.f);
}

bool ARTSShortsDirector::GetHasBattleAdvanceFinished(FRTSShortsAdvanceState& Advance, const FRTSShortsBattleAdvance& Settings,
                                                     const float SegmentSeconds) const
{
	using namespace RTSShortsAdvancePrivate;
	const AActor* Unit = Advance.Unit.Get();
	if (FVector::Dist2D(Unit->GetActorLocation(), Advance.Destination) < Settings.ArrivedCm)
	{
		return true;
	}
	// A tank that pulled up a little short of its point has finished its move too: it must not sit out the timeout
	// unable to attack or be attacked.
	const bool bStopped = SegmentSeconds >= Advance.OrderAtSeconds + StoppedCheckAfterSeconds
		&& Unit->GetVelocity().Size2D() < StationarySpeedCm;
	if (not bStopped)
	{
		Advance.StoppedSinceSeconds = -1.f;
	}
	else if (Advance.StoppedSinceSeconds < 0.f)
	{
		Advance.StoppedSinceSeconds = SegmentSeconds;
	}
	const bool bFinishedShort = Advance.StoppedSinceSeconds >= 0.f && SegmentSeconds - Advance.StoppedSinceSeconds >= StoppedForSeconds;
	return bFinishedShort || SegmentSeconds >= Advance.OrderAtSeconds + Settings.TimeoutSeconds;
}

void ARTSShortsDirector::ReleaseBattleAdvanceUnit(FRTSShortsAdvanceState& Advance, const int32 Frame, const float SegmentSeconds)
{
	Advance.bReleased = true;
	M_Battle.HeldUnits.Remove(Advance.Unit);
	const AActor* Unit = Advance.Unit.Get();
	UE_LOG(LogRTSShortsAdvance, Display, TEXT("RTS_SHORTS_ADVANCE_ARRIVED frame=%d unit=%s left=%.0f stoppedShort=%d seconds=%.1f at=%s"),
	       Frame, *Unit->GetName(), FVector::Dist2D(Unit->GetActorLocation(), Advance.Destination),
	       Advance.StoppedSinceSeconds >= 0.f ? 1 : 0, SegmentSeconds, *Unit->GetActorLocation().ToCompactString());
}

bool ARTSShortsDirector::GetIsBattleAdvanceUnit(const AActor* LineUnit) const
{
	return M_Battle.Advances.ContainsByPredicate([LineUnit](const FRTSShortsAdvanceState& Advance)
	{
		return Advance.Unit.Get() == LineUnit;
	});
}

bool ARTSShortsDirector::GetBattleAdvanceAnchor(FVector& OutAnchor) const
{
	// The German line: the camera stands behind it, and it keeps driving forward into its fight after the advance.
	FVector Sum = FVector::ZeroVector;
	int32 Alive = 0;
	for (const FRTSShortsAdvanceState& Advance : M_Battle.Advances)
	{
		if (not Advance.bEnemy && RTSShortsAdvancePrivate::GetIsAlive(Advance.Unit))
		{
			Sum += Advance.Unit->GetActorLocation();
			++Alive;
		}
	}
	if (Alive == 0)
	{
		return false;
	}
	OutAnchor = Sum / Alive;
	return true;
}

void ARTSShortsDirector::UpdateAdvanceFollowCamera(const FRTSShortsBattleAdvance& Settings)
{
	// Battle cuts own the camera from their first cut on; an orbit moves it itself.
	if (Settings.CameraFollowSpeed <= 0.f || not GetIsValidCamera() || M_BattleCuts.ShotIndex > 0
		|| not FMath::IsNearlyZero(M_Job.Segments[M_CurrentSegment].Camera.OrbitDegrees))
	{
		return;
	}
	FVector Anchor;
	if (not GetBattleAdvanceAnchor(Anchor))
	{
		// The German line is gone: the camera settles where it is, on its wrecks and the Soviets who beat it.
		return;
	}
	const float Blend = 1.f - FMath::Exp(-Settings.CameraFollowSpeed / static_cast<float>(M_Job.Fps));
	M_Battle.AdvanceAnchor = FMath::Lerp(M_Battle.AdvanceAnchor, Anchor, Blend);
	FVector Offset = M_Battle.AdvanceAnchor - M_Battle.AdvanceAnchorStart;
	Offset.Z = 0.f;
	M_Camera->SetActorLocation(M_Battle.AdvanceCameraStart + Offset);
}
