// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsBattleCuts, Log, All);

namespace RTSShortsBattleCutsPrivate
{
	// Shots are never shorter than this, a unit that dies at once included.
	constexpr float MinShotSeconds = 2.f;
	// The camera stays this high above the ground under it.
	constexpr float MinGroundClearanceCm = 130.f;
	// The view counts as clear when the first thing in the way is at most this close to the camera.
	constexpr float ClearViewSlackCm = 150.f;
	// Aim heights above the unit's ground point and above its target's.
	constexpr float SubjectAimHeightCm = 160.f;
	constexpr float TargetAimHeightCm = 120.f;
	// A unit this close to its nearest enemy scores the most (the fight is round it).
	constexpr float CloseFightCm = 8000.f;
	// Vehicles of either line within this radius of a unit count towards its group: a shot of a group shows the fight,
	// a shot of a lone vehicle does not. Each one adds GroupScorePerVehicle, up to GroupScoreMaxVehicles of them.
	constexpr float GroupRadiusCm = 3000.f;
	constexpr float GroupScorePerVehicle = 0.35f;
	constexpr int32 GroupScoreMaxVehicles = 8;
	// Squads are filmed now and then for the infantry and the guns: this much extra score on every third shot.
	constexpr float SquadTurnScore = 1.5f;
	// Reinforcements driving in are filmed on every fourth shot while there are any.
	constexpr float ArrivalTurnScore = 3.f;
	// A shot whose view of its unit stays blocked this many frames (a tank drove between) cuts away early.
	constexpr int32 BlockedFramesBeforeCut = 6;
	constexpr float MinBlockedShotSeconds = 0.5f;
	// No camera stands closer than this to another vehicle or wreck when a shot begins.
	constexpr float CameraVehicleClearanceCm = 750.f;
	// The aim leads from the unit towards its target by at most this many shot distances: a target 100 m off would aim at empty
	// ground. Over the shoulder the lead runs along the view; from the side or the front it swings the unit out of the
	// narrow 9:16 frame, so those shots lead by a third of their distance at most (about 18 degrees).
	constexpr float MaxLookLeadDistances = 1.7f;
	constexpr float MaxSideLookLeadDistances = 0.33f;

	float GetShotYawOffset(const ERTSShortsBattleShot Shot)
	{
		// Bearing of the camera from the unit, relative to the bearing from the unit to its target.
		switch (Shot)
		{
		case ERTSShortsBattleShot::Flank:
			return 90.f;
		case ERTSShortsBattleShot::FrontQuarter:
			return 35.f;
		case ERTSShortsBattleShot::OverShoulder:
		case ERTSShortsBattleShot::Wide:
		default:
			return 155.f;
		}
	}

	const TCHAR* GetShotName(const ERTSShortsBattleShot Shot)
	{
		switch (Shot)
		{
		case ERTSShortsBattleShot::Flank:
			return TEXT("Flank");
		case ERTSShortsBattleShot::FrontQuarter:
			return TEXT("FrontQuarter");
		case ERTSShortsBattleShot::Wide:
			return TEXT("Wide");
		case ERTSShortsBattleShot::OverShoulder:
		default:
			return TEXT("OverShoulder");
		}
	}

	float Smoothstep(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		return Clamped * Clamped * (3.f - 2.f * Clamped);
	}
}

bool ARTSShortsDirector::GetBattleCutUnitPoint(const AActor* LineUnit, FVector& OutPoint) const
{
	if (not IsValid(LineUnit))
	{
		return false;
	}
	ASquadController* Squad = const_cast<ASquadController*>(Cast<ASquadController>(LineUnit));
	if (Squad == nullptr)
	{
		OutPoint = LineUnit->GetActorLocation();
		return true;
	}
	FVector Sum = FVector::ZeroVector;
	int32 Alive = 0;
	for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
	{
		if (IsValid(Soldier) && Soldier->IsUnitAlive())
		{
			Sum += Soldier->GetActorLocation();
			++Alive;
		}
	}
	if (Alive == 0)
	{
		return false;
	}
	OutPoint = Sum / Alive;
	return true;
}

bool ARTSShortsDirector::GetIsBattleCutUnitAlive(const AActor* LineUnit) const
{
	if (Cast<ASquadController>(LineUnit) != nullptr)
	{
		FVector Unused;
		return GetBattleCutUnitPoint(LineUnit, Unused);
	}
	// The engine's AActor::IsUnitAlive is non-const; the unit is only read.
	return IsValid(LineUnit) && const_cast<AActor*>(LineUnit)->IsUnitAlive();
}

bool ARTSShortsDirector::GetIsBattleCutUnitFilmable(const AActor* LineUnit) const
{
	// A reinforcement waits out of sight until its move order; once it drives in it is worth a shot of its own. A
	// Segment.BattleAdvance vehicle is held while it closes in, in plain view.
	if (not M_Battle.HeldUnits.Contains(TWeakObjectPtr<AActor>(const_cast<AActor*>(LineUnit))) || GetIsBattleAdvanceUnit(LineUnit))
	{
		return true;
	}
	const int32 Index = M_Battle.Reinforcements.IndexOfByKey(TWeakObjectPtr<AActor>(const_cast<AActor*>(LineUnit)));
	return M_Battle.ReinforcementOrdered.IsValidIndex(Index) && M_Battle.ReinforcementOrdered[Index];
}

bool ARTSShortsDirector::GetIsBattleCutUnitDrivingIn(const AActor* LineUnit) const
{
	const int32 Index = M_Battle.Reinforcements.IndexOfByKey(TWeakObjectPtr<AActor>(const_cast<AActor*>(LineUnit)));
	return M_Battle.ReinforcementOrdered.IsValidIndex(Index) && M_Battle.ReinforcementJoined.IsValidIndex(Index)
		&& M_Battle.ReinforcementOrdered[Index] && not M_Battle.ReinforcementJoined[Index];
}

void ARTSShortsDirector::BeginBattleCuts(const FRTSShortsSegment& Segment)
{
	M_BattleCuts = FRTSShortsBattleCutState();
	if (not Segment.BattleCuts.bEnabled)
	{
		return;
	}
	M_BattleCuts.bIsActive = true;
	M_BattleCuts.Random.Initialize(Segment.BattleCuts.RandomSeed);
	// The opening shot keeps the segment's own camera until the first cut.
	M_BattleCuts.ShotEndSeconds = Segment.BattleCuts.FirstCutSeconds;
	M_BattleCuts.bSubjectIsPlayer = M_BattleCuts.Random.FRand() < 0.5f;
	UE_LOG(LogRTSShortsBattleCuts, Display, TEXT("RTS_SHORTS_BATTLE_CUTS_BEGIN firstCut=%.1f shot=%.1f"), Segment.BattleCuts.FirstCutSeconds,
	       Segment.BattleCuts.ShotSeconds);
}

void ARTSShortsDirector::TickBattleCuts(const int32 Frame)
{
	using namespace RTSShortsBattleCutsPrivate;
	if (not M_BattleCuts.bIsActive || not M_Battle.bIsActive || not M_Job.Segments.IsValidIndex(M_CurrentSegment) || not GetIsValidCamera())
	{
		return;
	}
	const FRTSShortsBattleCuts& Settings = M_Job.Segments[M_CurrentSegment].BattleCuts;
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	if (M_BattleCuts.Subject.IsValid() && M_BattleCuts.DiedAtSeconds < 0.f && not GetIsBattleCutUnitAlive(M_BattleCuts.Subject.Get()))
	{
		// Stay on the explosion, then move on (never past the planned end, never under the minimum shot length).
		M_BattleCuts.DiedAtSeconds = SegmentSeconds;
		M_BattleCuts.ShotEndSeconds = FMath::Min(M_BattleCuts.ShotEndSeconds,
		                                         FMath::Max(SegmentSeconds + Settings.DeathHoldSeconds,
		                                                    M_BattleCuts.ShotStartSeconds + MinShotSeconds));
		UE_LOG(LogRTSShortsBattleCuts, Display, TEXT("RTS_SHORTS_BATTLE_CUT_SUBJECT_DIED frame=%d endsAt=%.2f"), Frame,
		       M_BattleCuts.ShotEndSeconds);
	}
	const bool bBlockedTooLong = M_BattleCuts.BlockedFrames >= BlockedFramesBeforeCut
		&& SegmentSeconds - M_BattleCuts.ShotStartSeconds >= MinBlockedShotSeconds;
	if (bBlockedTooLong)
	{
		UE_LOG(LogRTSShortsBattleCuts, Display, TEXT("RTS_SHORTS_BATTLE_CUT_BLOCKED frame=%d shot=%d"), Frame, M_BattleCuts.ShotIndex);
	}
	if (SegmentSeconds >= M_BattleCuts.ShotEndSeconds || bBlockedTooLong)
	{
		StartBattleCutShot(Settings, SegmentSeconds, Frame);
		return;
	}
	if (M_BattleCuts.ShotIndex > 0)
	{
		PlaceBattleCutCamera(SegmentSeconds, false);
	}
}

void ARTSShortsDirector::StartBattleCutShot(const FRTSShortsBattleCuts& Settings, const float SegmentSeconds, const int32 Frame)
{
	using namespace RTSShortsBattleCutsPrivate;
	FRTSShortsBattleCutState& State = M_BattleCuts;
	// Alternate the lines; a line with nobody left to film hands the shot to the other.
	const bool bWantPlayer = not State.bSubjectIsPlayer;
	const bool bSquadTurn = State.ShotIndex % 3 == 2;
	const bool bArrivalTurn = State.ShotIndex % 4 == 1;
	AActor* Best = nullptr;
	bool bBestIsPlayer = bWantPlayer;
	float BestScore = TNumericLimits<float>::Lowest();
	for (const bool bPlayerSide : {bWantPlayer, not bWantPlayer})
	{
		const TArray<TWeakObjectPtr<AActor>>& Line = bPlayerSide ? M_Battle.PlayerUnits : M_Battle.EnemyUnits;
		const TArray<TWeakObjectPtr<AActor>>& Squads = bPlayerSide ? M_Battle.PlayerSquads : M_Battle.EnemySquads;
		const TArray<TWeakObjectPtr<AActor>>& Enemies = bPlayerSide ? M_Battle.EnemyUnits : M_Battle.PlayerUnits;
		TArray<AActor*> Candidates;
		for (const TWeakObjectPtr<AActor>& Unit : Line)
		{
			// Soldiers are filmed through their squad.
			if (Unit.IsValid() && Cast<ASquadUnit>(Unit.Get()) == nullptr && GetIsBattleCutUnitFilmable(Unit.Get()))
			{
				Candidates.Add(Unit.Get());
			}
		}
		for (const TWeakObjectPtr<AActor>& Squad : Squads)
		{
			if (Squad.IsValid() && GetIsBattleCutUnitFilmable(Squad.Get()))
			{
				Candidates.Add(Squad.Get());
			}
		}
		for (AActor* Candidate : Candidates)
		{
			FVector Point;
			if (not GetIsBattleCutUnitAlive(Candidate) || not GetBattleCutUnitPoint(Candidate, Point))
			{
				continue;
			}
			float NearestEnemy = TNumericLimits<float>::Max();
			for (const TWeakObjectPtr<AActor>& Enemy : Enemies)
			{
				if (Enemy.IsValid() && Enemy->IsUnitAlive())
				{
					NearestEnemy = FMath::Min(NearestEnemy, FVector::Dist2D(Point, Enemy->GetActorLocation()));
				}
			}
			int32 GroupVehicles = 0;
			for (const TArray<TWeakObjectPtr<AActor>>* GroupLine : {&M_Battle.PlayerUnits, &M_Battle.EnemyUnits})
			{
				for (const TWeakObjectPtr<AActor>& Other : *GroupLine)
				{
					if (Other.IsValid() && Other.Get() != Candidate && Cast<ASquadUnit>(Other.Get()) == nullptr && Other->IsUnitAlive()
						&& FVector::Dist2D(Point, Other->GetActorLocation()) < GroupRadiusCm)
					{
						++GroupVehicles;
					}
				}
			}
			float Score = State.Random.FRand() + 1.5f * (1.f - FMath::Clamp(NearestEnemy / CloseFightCm, 0.f, 1.f))
				+ GroupScorePerVehicle * FMath::Min(GroupVehicles, GroupScoreMaxVehicles);
			const int32 RecentIndex = State.Recent.IndexOfByKey(TWeakObjectPtr<AActor>(Candidate));
			if (RecentIndex != INDEX_NONE && RecentIndex >= State.Recent.Num() - Settings.RepeatGap)
			{
				Score -= 4.f;
			}
			if (bSquadTurn && Cast<ASquadController>(Candidate) != nullptr)
			{
				Score += SquadTurnScore;
			}
			if (bArrivalTurn && GetIsBattleCutUnitDrivingIn(Candidate))
			{
				// Far from the fight, so the closeness score would never pick it: the arrivals get their own turn.
				Score += ArrivalTurnScore;
			}
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = Candidate;
				bBestIsPlayer = bPlayerSide;
			}
		}
		if (Best != nullptr)
		{
			break;
		}
	}
	const float Jitter = Settings.ShotJitterSeconds > 0.f ? State.Random.FRandRange(-Settings.ShotJitterSeconds, Settings.ShotJitterSeconds) : 0.f;
	State.ShotStartSeconds = SegmentSeconds;
	State.ShotEndSeconds = SegmentSeconds + FMath::Max(MinShotSeconds, Settings.ShotSeconds + Jitter);
	State.DiedAtSeconds = -1.f;
	State.BlockedFrames = 0;
	if (Best == nullptr)
	{
		// Nobody left on either side: the camera keeps the last view until the segment ends.
		State.Subject.Reset();
		return;
	}
	State.Subject = Best;
	State.bSubjectIsPlayer = bBestIsPlayer;
	State.Recent.Add(Best);
	++State.ShotIndex;
	// What the unit fights: its ordered target while that lives, else the nearest living enemy.
	const TArray<TWeakObjectPtr<AActor>>& Enemies = bBestIsPlayer ? M_Battle.EnemyUnits : M_Battle.PlayerUnits;
	const TWeakObjectPtr<AActor>* Ordered = M_Battle.OrderedTargets.Find(TWeakObjectPtr<AActor>(Best));
	AActor* Target = Ordered != nullptr && Ordered->IsValid() && (*Ordered)->IsUnitAlive() ? Ordered->Get() : nullptr;
	FVector SubjectPoint;
	GetBattleCutUnitPoint(Best, SubjectPoint);
	if (Target == nullptr)
	{
		float NearestDistance = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->IsUnitAlive() && FVector::DistSquared2D(SubjectPoint, Enemy->GetActorLocation()) < NearestDistance)
			{
				NearestDistance = FVector::DistSquared2D(SubjectPoint, Enemy->GetActorLocation());
				Target = Enemy.Get();
			}
		}
	}
	State.Target = Target;
	State.Anchor = SubjectPoint;
	State.TargetPoint = IsValid(Target) ? Target->GetActorLocation() : SubjectPoint + M_Stage->GetActorForwardVector() * (bBestIsPlayer ? 3000.f : -3000.f);
	const bool bSquad = Cast<ASquadController>(Best) != nullptr;
	// Shot kind: a wide one every WideEvery shots, otherwise the three close kinds in turn (squads always over the shoulder:
	// soldiers are small and the flank view of a crew shows little).
	ERTSShortsBattleShot Shot = ERTSShortsBattleShot::OverShoulder;
	if (Settings.WideEvery > 0 && State.ShotIndex % Settings.WideEvery == 0)
	{
		Shot = ERTSShortsBattleShot::Wide;
	}
	else if (not bSquad)
	{
		const ERTSShortsBattleShot Close[] = {ERTSShortsBattleShot::OverShoulder, ERTSShortsBattleShot::Flank, ERTSShortsBattleShot::FrontQuarter};
		Shot = Close[State.ShotIndex % 3];
	}
	// Squads are framed closer in a close shot; a wide shot keeps its distance so the vehicles round them stay in view.
	const float SquadScale = bSquad && Shot != ERTSShortsBattleShot::Wide ? 0.6f : 1.f;
	float Distance = Settings.Distance * SquadScale;
	float Height = Settings.Height * SquadScale;
	float Focal = Settings.FocalLength;
	float LookBlend = Settings.TargetLookBlend;
	switch (Shot)
	{
	case ERTSShortsBattleShot::Flank:
		Distance *= 0.85f;
		Height *= 0.6f;
		LookBlend = 0.12f;
		break;
	case ERTSShortsBattleShot::FrontQuarter:
		Distance *= 0.95f;
		Height *= 0.7f;
		LookBlend = 0.f;
		break;
	case ERTSShortsBattleShot::Wide:
		Distance *= 2.2f;
		Height *= 2.6f;
		Focal *= 0.8f;
		LookBlend = 0.45f;
		break;
	case ERTSShortsBattleShot::OverShoulder:
	default:
		break;
	}
	const float LineYaw = (State.TargetPoint - SubjectPoint).GetSafeNormal2D().Rotation().Yaw;
	State.PanSign = State.Random.FRand() < 0.5f ? -1.f : 1.f;
	// Try the shot's side picked at random, then the mirrored side, then closer in; the first view nothing blocks wins.
	const float Side = State.Random.FRand() < 0.5f ? -1.f : 1.f;
	// Wide shots vary their bearing: from behind the line, three-quarter, or from the side across both lines.
	const float WideBearings[] = {155.f, 115.f, 75.f};
	const float YawOffset = Shot == ERTSShortsBattleShot::Wide ? WideBearings[State.Random.RandRange(0, 2)] : GetShotYawOffset(Shot);
	const AActor* SubjectActor = bSquad ? nullptr : Best;
	bool bClear = false;
	// Bearings tried in turn: the shot's own on either side, then swung 30 and 60 degrees; nearer in after that.
	for (const float DistanceScale : {1.f, 0.7f})
	{
		for (const float Swing : {0.f, 30.f, -30.f, 60.f, -60.f})
		{
			for (const float TrySide : {Side, -Side})
			{
				const float Yaw = LineYaw + TrySide * (YawOffset + Swing);
				// The pan starts half its sweep before the bearing and ends half after it.
				const float StartYaw = Yaw - State.PanSign * Settings.PanDegrees * 0.5f;
				if (GetIsBattleCutViewClear(SubjectPoint, StartYaw, Distance * DistanceScale, Height, SubjectActor)
					&& GetIsBattleCutViewClear(SubjectPoint, StartYaw + State.PanSign * Settings.PanDegrees, Distance * DistanceScale, Height,
					                           SubjectActor))
				{
					State.BaseYaw = StartYaw;
					Distance *= DistanceScale;
					bClear = true;
					break;
				}
			}
			if (bClear)
			{
				break;
			}
		}
		if (bClear)
		{
			break;
		}
	}
	if (not bClear)
	{
		// Packed in by its neighbours: look down from above them instead of through them.
		State.BaseYaw = LineYaw + Side * YawOffset - State.PanSign * Settings.PanDegrees * 0.5f;
		Height *= 2.f;
		LookBlend = 0.f;
	}
	State.Shot = Shot;
	State.Distance = Distance;
	State.Height = Height;
	State.FocalLength = Focal;
	State.LookBlend = LookBlend;
	// The opening shot's orbit would keep moving the camera.
	M_Orbit.bIsActive = false;
	ApplyBattleCutLens();
	PlaceBattleCutCamera(SegmentSeconds, true);
	ApplyCameraCut(M_Camera->GetActorLocation());
	if (M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
	{
		M_Manifest.Segments[M_CurrentSegment].CameraCutFrames.Add(Frame);
	}
	UE_LOG(LogRTSShortsBattleCuts, Display,
	       TEXT("RTS_SHORTS_BATTLE_CUT frame=%d shot=%d kind=%s unit=%s side=%s target=%s clear=%d until=%.2f camera=%s"), Frame,
	       State.ShotIndex, GetShotName(Shot), *Best->GetName(), bBestIsPlayer ? TEXT("player") : TEXT("enemy"),
	       IsValid(Target) ? *Target->GetName() : TEXT("none"), bClear ? 1 : 0, State.ShotEndSeconds,
	       *M_Camera->GetActorLocation().ToCompactString());
}

bool ARTSShortsDirector::GetIsBattleCutViewClear(const FVector& Anchor, const float Yaw, const float Distance, const float Height,
                                                 const AActor* Subject) const
{
	using namespace RTSShortsBattleCutsPrivate;
	const FVector From = Anchor + FVector(0.f, 0.f, SubjectAimHeightCm);
	const FVector To = Anchor + FRotator(0.f, Yaw, 0.f).Vector() * Distance + FVector(0.f, 0.f, Height);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShortsBattleCutView), true);
	if (IsValid(Subject))
	{
		Params.AddIgnoredActor(Subject);
		TArray<AActor*> Attached;
		Subject->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	for (const TArray<TWeakObjectPtr<AActor>>* Group : {&M_Battle.PlayerUnits, &M_Battle.EnemyUnits, &M_Battle.SpawnedWrecks})
	{
		for (const TWeakObjectPtr<AActor>& Other : *Group)
		{
			if (Other.IsValid() && Other.Get() != Subject && Cast<ASquadUnit>(Other.Get()) == nullptr
				&& FVector::Dist2D(Other->GetActorLocation(), To) < CameraVehicleClearanceCm)
			{
				return false;
			}
		}
	}
	FHitResult Hit;
	if (not GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
	{
		return true;
	}
	return Hit.Distance >= FVector::Dist(From, To) - ClearViewSlackCm;
}

void ARTSShortsDirector::PlaceBattleCutCamera(const float SegmentSeconds, const bool bSnap)
{
	using namespace RTSShortsBattleCutsPrivate;
	FRTSShortsBattleCutState& State = M_BattleCuts;
	const FRTSShortsBattleCuts& Settings = M_Job.Segments[M_CurrentSegment].BattleCuts;
	const float DeltaSeconds = bSnap ? 0.f : FMath::Max(0.f, SegmentSeconds - State.LastSeconds);
	State.LastSeconds = SegmentSeconds;
	// A dead unit's wreck stays where it died; a squad that is gone leaves the camera on its last point.
	FVector SubjectPoint = State.Anchor;
	GetBattleCutUnitPoint(State.Subject.Get(), SubjectPoint);
	const FVector TargetPoint = State.Target.IsValid() ? State.Target->GetActorLocation() : State.TargetPoint;
	const float Follow = bSnap ? 1.f : 1.f - FMath::Exp(-Settings.FollowSpeed * DeltaSeconds);
	State.Anchor = FMath::Lerp(State.Anchor, SubjectPoint, Follow);
	State.TargetPoint = FMath::Lerp(State.TargetPoint, TargetPoint, Follow);
	const float ShotLength = FMath::Max(0.1f, State.ShotEndSeconds - State.ShotStartSeconds);
	const float Yaw = State.BaseYaw + State.PanSign * Settings.PanDegrees * Smoothstep((SegmentSeconds - State.ShotStartSeconds) / ShotLength);
	FVector CameraLocation = State.Anchor + FRotator(0.f, Yaw, 0.f).Vector() * State.Distance;
	CameraLocation.Z = State.Anchor.Z + State.Height;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShortsBattleCutGround), false);
	const FVector Above = FVector(CameraLocation.X, CameraLocation.Y, CameraLocation.Z + 3000.f);
	const FVector Below = FVector(CameraLocation.X, CameraLocation.Y, CameraLocation.Z - 5000.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Above, Below, ECC_WorldStatic, Params))
	{
		CameraLocation.Z = FMath::Max(CameraLocation.Z, Hit.ImpactPoint.Z + MinGroundClearanceCm);
	}
	const FVector SubjectAim = State.Anchor + FVector(0.f, 0.f, SubjectAimHeightCm);
	const FVector TargetAim = State.TargetPoint + FVector(0.f, 0.f, TargetAimHeightCm);
	const bool bLeadAlongView = State.Shot == ERTSShortsBattleShot::OverShoulder || State.Shot == ERTSShortsBattleShot::Wide;
	const float Lead = FMath::Min(State.LookBlend * FVector::Dist(SubjectAim, TargetAim),
	                              (bLeadAlongView ? MaxLookLeadDistances : MaxSideLookLeadDistances) * State.Distance);
	const FVector LookAt = SubjectAim + (TargetAim - SubjectAim).GetSafeNormal() * Lead;
	if (not bSnap)
	{
		const AActor* SubjectActor = Cast<ASquadController>(State.Subject.Get()) != nullptr ? nullptr : State.Subject.Get();
		FCollisionQueryParams ViewParams(SCENE_QUERY_STAT(ShortsBattleCutBlocked), true);
		if (IsValid(SubjectActor))
		{
			ViewParams.AddIgnoredActor(SubjectActor);
			TArray<AActor*> Attached;
			SubjectActor->GetAttachedActors(Attached, true, true);
			ViewParams.AddIgnoredActors(Attached);
		}
		FHitResult ViewHit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(ViewHit, SubjectAim, CameraLocation, ECC_Visibility, ViewParams)
			&& ViewHit.Distance < FVector::Dist(SubjectAim, CameraLocation) - ClearViewSlackCm;
		State.BlockedFrames = bBlocked ? State.BlockedFrames + 1 : 0;
	}
	FRotator Rotation = (LookAt - CameraLocation).Rotation();
	Rotation.Roll = 0.f;
	M_Camera->SetActorLocationAndRotation(CameraLocation, Rotation);
}

void ARTSShortsDirector::ApplyBattleCutLens() const
{
	UCineCameraComponent* CineComponent = M_Camera->GetCineCameraComponent();
	if (CineComponent == nullptr || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	// Same grade as the opening shot (FrameCamera), the lens and exposure of the cuts.
	CineComponent->SetCurrentFocalLength(M_BattleCuts.FocalLength);
	CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Disable;
	CineComponent->PostProcessSettings.bOverride_AutoExposureBias = true;
	CineComponent->PostProcessSettings.AutoExposureBias = Segment.BattleCuts.ExposureBias;
	CineComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
	CineComponent->PostProcessSettings.MotionBlurAmount = 0.f;
	CineComponent->PostProcessSettings.bOverride_ColorSaturation = true;
	const float Saturation = Segment.Camera.ColorSaturation;
	CineComponent->PostProcessSettings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	CineComponent->PostProcessSettings.bOverride_WhiteTemp = not FMath::IsNearlyEqual(Segment.Camera.WhiteTemp, 6500.f);
	CineComponent->PostProcessSettings.WhiteTemp = Segment.Camera.WhiteTemp;
	CineComponent->PostProcessBlendWeight = 1.f;
}
