// FAircraftPath.cpp

#include "FAircraftPath.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/GeneratedTypeName.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircaftHelpers/FRTSAircraftHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

constexpr bool bDebugAttackDive = false;
// Yellow -> UturnStart
// Red -> Offset to the enemy with arrow.
// Purple -> ExtremeOffset to the enemy with arrow.
// Black -> Offsetpoint adjusted; outside of attack range.
// white -> offsetpoint adjusted; attack range too close.
constexpr bool bDebugUTurn = false;
constexpr bool bDebugTooClose = false;
// Purple ->
constexpr bool bDebugTooFar = true;

void FAircraftPath::Reset()
{
	PathPoints.Empty();
	CurrentPathIndex = 0;
}

void FAircraftPath::CreateMoveToPath(
	const FVector& StartAirPoint,
	const FVector& EndAirPoint,
	const FRotator& StartRotation,
	const FAircraftBezierCurveSettings& BezierCurveSettings,
	const FAircraftDeadZone& DeadZoneSettings)
{
	Reset();
	using DeveloperSettings::GamePlay::Navigation::Aircraft::InDeadZonePointDistMlt;
	using DeveloperSettings::GamePlay::Navigation::Aircraft::InDeadZonePointAngleDivider;
	PathPoints.Add({
		StartAirPoint, 0.f, EAirPathPointType::Regular
	});

	AddMoveToPathFromStart(
		StartAirPoint,
		EndAirPoint,
		StartRotation,
		BezierCurveSettings,
		DeadZoneSettings);


	OnPathCompleteCalculateRotations(EndAirPoint, StartRotation);

	// todo remove
	DebugPath(5);
}

void FAircraftPath::AddMoveToPathFromStart(
	const FVector& StartAirPoint,
	const FVector& EndAirPoint,
	const FRotator& StartRotation,
	const FAircraftBezierCurveSettings& BezierCurveSettings,
	const FAircraftDeadZone& DeadZoneSettings,
	const bool bIgnoreBezierMaxDistance)
{
	FVector BezierStart = StartAirPoint;
	float AngleToFirstPoint = 0.f;
	if (GetIsWithinDeadZone(StartRotation, StartAirPoint, EndAirPoint, DeadZoneSettings, AngleToFirstPoint))
	{
		FRTSAircraftHelpers::AircraftDebug("Bezier point in deadzone, adjusting", FColor::Orange);
		BezierStart = GetBezierStartOutOfDeadZone(StartAirPoint, EndAirPoint, StartRotation,
		                                          AngleToFirstPoint, DeadZoneSettings);
		FRTSAircraftHelpers::AircraftSphereDebug(Owner, BezierStart, FColor::Orange, 75);
	}
	FVector BezierEnd = EndAirPoint;
	if ((not bIgnoreBezierMaxDistance) && GetIsEndPointOutsideBezierReach(
		StartAirPoint, EndAirPoint, BezierCurveSettings))
	{
		FRTSAircraftHelpers::AircraftDebug("Bezier end out of reach, adjusting", FColor::Orange);
		BezierEnd = GetBezierEndInReach(StartAirPoint, EndAirPoint, BezierCurveSettings);
		FRTSAircraftHelpers::AircraftSphereDebug(Owner, BezierEnd, FColor::Orange, 75);
	}

	AddSmallOrLargeBezierToPath(
		StartRotation,
		BezierStart,
		BezierEnd,
		BezierCurveSettings.BezierCurveTension,
		BezierCurveSettings.AngleConsiderPointBehind);

	PathPoints.Add({EndAirPoint, 0.f, EAirPathPointType::Regular});
}

void FAircraftPath::AddUTurnPathingToEnemy(const FVector& StartLocation, const FRotator& StartRotation,
                                           const FVector& EnemyAirLocation,
                                           const FAircraftBezierCurveSettings& BezierCurveSettings, const
                                           FAircraftAttackMoveSettings& AttackMoveSettings,
                                           const FDistAbsAngleSign& DistAbsAngle)
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::MoveToAngleBezierSampleDivider;
	FVector UTurnEnd;
	const FVector UTurnStart = GetUTurnPointsInRangeToActor(
		DistAbsAngle,
		AttackMoveSettings,
		EnemyAirLocation,
		UTurnEnd,
		StartRotation,
		StartLocation,
		BezierCurveSettings.BezierCurveTension
	);
	// Add the u-turn start.
	PathPoints.Add({UTurnStart, StartRotation.Roll, EAirPathPointType::Regular});
	// Add the bezier-curve points.
	if (GetIsEndPointOutsideBezierReach(UTurnStart, UTurnEnd, BezierCurveSettings))
	{
		UTurnEnd = GetBezierEndInReach(UTurnStart, UTurnEnd, BezierCurveSettings);
	}
	const int32 NumSamples = FMath::Max(0, FMath::CeilToInt(DistAbsAngle.AbsAngle / MoveToAngleBezierSampleDivider));
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	const FVector Delta = UTurnEnd - UTurnStart;
	const float DistanceUTurn = Delta.Size();
	const FVector DirToEndUTurn = Delta / DistanceUTurn;

	// Only adds between-bezier points; no end point.
	SetupLargeBezierMoveTo(
		UTurnStart,
		UTurnEnd,
		BezierCurveSettings.BezierCurveTension,
		DistanceUTurn,
		DirToEndUTurn,
		NumSamples, DistAbsAngle.Sign > 0, Forward);

	// Recover roll at end point.
	PathPoints.Add({
		UTurnEnd,
		0.f,
		EAirPathPointType::Regular
	});

	// Trim the path so we start the attack dive as soon as it becomes possible.
	RemovePointsAfterAttackPossible(StartRotation, EnemyAirLocation, AttackMoveSettings);


	// Update rotations.
	OnPathCompleteCalculateRotations(UTurnEnd, StartRotation);
}

FVector FAircraftPath::GetUTurnPointsInRangeToActor(const FDistAbsAngleSign& DistanceAngle,
                                                    const FAircraftAttackMoveSettings& AttackMoveSettings,
                                                    const FVector& EnemyAirPosition, FVector& UTurnEndPoint, const
                                                    FRotator& StartRotation, const FVector& StartLocation,
                                                    const float BezierCurveTension) const
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::UTurnApproachPointAngle;
	// turn angle degrees the start rotation vector on the up
	const float AngleToTurnBy = UTurnApproachPointAngle * DistanceAngle.Sign;
	// Rotate by up vector.
	FRotator RotationYawOnly = StartRotation;
	RotationYawOnly.Roll = 0.f;
	RotationYawOnly.Pitch = 0.f;
	const FVector DirToStart = RotationYawOnly.Vector().RotateAngleAxis(AngleToTurnBy, FVector::UpVector).
	                                           GetSafeNormal();
	const FVector UTurnStart = StartLocation + DirToStart * DistanceAngle.Distance / 3;
	FRTSAircraftHelpers::AircraftSphereDebug(
		Owner,
		UTurnStart,
		FColor::Yellow, 75, 10);
	const bool bIsExtremeAngle = GetIsExtremeAngleToEnemy(DistanceAngle);
	const float ExtremeAngleAddOffset = bIsExtremeAngle ? AttackMoveSettings.UTurnExtremeAngleOffset : 0.f;
	// The further the enemy is the less offset in the enemy direction we need.
	const float OffsetDistanceEndUTurn = 2 * BezierCurveTension * DistanceAngle.AbsAngle / 90 * AttackMoveSettings.
		StartHzOffsetDistanceUTurnToEnemy;
	FVector OffsetLocation = StartLocation + DistanceAngle.LeftRightUnitVectorToEnemy * OffsetDistanceEndUTurn;
	FVector ExtremeOffsetLocation = StartLocation + DistanceAngle.GetDirection() * (OffsetDistanceEndUTurn +
		ExtremeAngleAddOffset);
	if (bDebugUTurn && DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		// draw a red line for 20 seconds from the start location to the offset location with thickness 5.
		DrawDebugDirectionalArrow(
			Owner ? Owner->GetWorld() : nullptr,
			StartLocation,
			OffsetLocation,
			5,
			FColor::Red,
			false, 20.f, 0, 5.f
		);
		FRTSAircraftHelpers::AircraftSphereDebug(
			Owner, OffsetLocation, FColor::Red, 75, 10);
		// draw a purple line for the extreme offset location also 20 seconds and thickness 2
		DrawDebugDirectionalArrow(
			Owner ? Owner->GetWorld() : nullptr,
			StartLocation,
			ExtremeOffsetLocation,
			5,
			FColor::Purple,
			false, 20.f, 0, 2.f
		);
		FRTSAircraftHelpers::AircraftSphereDebug(
			Owner, ExtremeOffsetLocation, FColor::Purple, 100, 10);
	}
	UTurnEndPoint = GetOffsetLocationInAttackRange(EnemyAirPosition, OffsetLocation, AttackMoveSettings);
	return UTurnStart;
}

bool FAircraftPath::GetIsExtremeAngleToEnemy(const FDistAbsAngleSign& DistanceAngle) const
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::DeltaAngleExtreme;
	constexpr float ExtremeAngle = DeltaAngleExtreme;
	const bool bIsExtremeTo180Deg = (FMath::Abs(DistanceAngle.AbsAngle - 180) <= ExtremeAngle);
	if (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		if (bIsExtremeTo180Deg)
		{
			FRTSAircraftHelpers::AircraftDebug("Angle to enemy is extreme to 180°: " +
			                                   FString::SanitizeFloat(DistanceAngle.AbsAngle) + "°",
			                                   FColor::Green);
		}
	}
	return bIsExtremeTo180Deg;
}

FVector FAircraftPath::GetOffsetLocationInAttackRange(
	const FVector& EnemyAirLocation,
	const FVector& OffsetLocation,
	const FAircraftAttackMoveSettings& AttackMoveSettings) const
{
	//todo remove
	FColor DebugColor = FColor::White;
	const float Distance = FVector::Distance(OffsetLocation, EnemyAirLocation);
	FVector FinalLocation = OffsetLocation;

	// Too far: clamp to 80% of max dive range
	if (Distance >= AttackMoveSettings.AttackDiveRange.Y)
	{
		FRTSAircraftHelpers::AircraftDebug(
			"\nThe offset location was too far from the enemy location to be in dive range, adjusting", FColor::Purple);

		DebugColor = FColor::Black;

		// Desired horizontal distance = 80% of max range
		const float DesiredDistance = AttackMoveSettings.AttackDiveRange.Y * 0.8f;

		// Direction from enemy toward the original offset
		const FVector Direction = (OffsetLocation - EnemyAirLocation).GetSafeNormal();

		// Return the point at that distance along the line
		FinalLocation = EnemyAirLocation + Direction * DesiredDistance;
		FinalLocation.Z = EnemyAirLocation.Z;
		return FinalLocation;
	}

	// Too close: push out to 120% of min dive range
	if (Distance <= AttackMoveSettings.AttackDiveRange.X)
	{
		DebugColor = FColor::White;
		FRTSAircraftHelpers::AircraftDebug(
			"\nThe offset location was too close from the enemy location to be in dive range, adjusting",
			FColor::Red);
		DebugColor = FColor::White;

		// Desired horizontal distance = 120% of min range
		const float DesiredDistance = AttackMoveSettings.AttackDiveRange.X * 1.2f;

		// Same direction from enemy to offset
		const FVector Direction = (OffsetLocation - EnemyAirLocation).GetSafeNormal();

		// Return the point at that distance along the line
		FinalLocation = EnemyAirLocation + Direction * DesiredDistance;
		FinalLocation.Z = EnemyAirLocation.Z;
		return FinalLocation;
	}
	FRTSAircraftHelpers::AircraftSphereDebug(Owner,
	                                         OffsetLocation,
	                                         DebugColor, 200, 30);

	FinalLocation = OffsetLocation;
	FinalLocation.Z = EnemyAirLocation.Z;
	return FinalLocation;
}

bool FAircraftPath::GetIsEnemyExtremelyFar(const FDistAbsAngleSign& DistanceAngle,
                                           const FAircraftAttackMoveSettings& AttackMoveSettings) const
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::MaxAttackDistanceScale;
	const float MaxAttackDistance = AttackMoveSettings.AttackDiveRange.Y * MaxAttackDistanceScale;
	return DistanceAngle.Distance > MaxAttackDistance;
}


void FAircraftPath::CreateAttackPath(const FVector& StartAirPoint, const FRotator& StartRotation,
                                     const FVector& EnemyLocation,
                                     const FAircraftBezierCurveSettings& BezierCurveSettings,
                                     const FAircraftAttackMoveSettings& AttackMoveSettings,
                                     const FAircraftDeadZone& DeadZoneSettings)
{
	Reset();
	PathPoints.Add({StartAirPoint, StartRotation.Roll, EAirPathPointType::Regular});
	FDistAbsAngleSign DistanceAngleToTarget;
	const FVector EnemyAirLocation = FVector(EnemyLocation.X, EnemyLocation.Y, StartAirPoint.Z);
	const EDirectAttackDiveResult DiveResult = GetIsWithinDirectAttackDiveRange(
		StartAirPoint, StartRotation, EnemyAirLocation, AttackMoveSettings, DistanceAngleToTarget);

	FVector EndAirPoint = FVector::ZeroVector;
	switch (DiveResult)
	{
	case EDirectAttackDiveResult::DivePossible:
		// Sets point rotations too.
		AddAttackDivePath(
			StartAirPoint, EnemyLocation, AttackMoveSettings, EndAirPoint);
		break;
	case EDirectAttackDiveResult::TooClose:
		// Adds offset point and sets rotation for it.
		AddAttackPathTooClose(StartAirPoint, StartRotation,
		                      AttackMoveSettings);
		break;
	case EDirectAttackDiveResult::TooFar:
		// Adds move to points and end point. Sets rotations.
		AddAttackPathTooFar(
			StartAirPoint, StartRotation,
			EnemyAirLocation, BezierCurveSettings, AttackMoveSettings,
			DeadZoneSettings, DistanceAngleToTarget, EndAirPoint);
		break;
	case EDirectAttackDiveResult::NotWithinAngle:
		// Adds bezier u turn points and final point.
		AddUTurnPathingToEnemy(StartAirPoint,
		                       StartRotation,
		                       EnemyAirLocation,
		                       BezierCurveSettings,
		                       AttackMoveSettings,
		                       DistanceAngleToTarget);
		break;
	}
	FRTSAircraftHelpers::AircraftDebug("Result : " +
	                                   Global_GetDirectAttackDiveResultString(DiveResult), FColor::Green);
	DistanceAngleToTarget.Debug(5);
	// todo remove
	DebugPath(5);
}

void FAircraftPath::AddAttackPathTooFar(
	const FVector& StartAirPoint,
	const FRotator& StartRotation,
	const FVector& EnemyAirLocation,
	const FAircraftBezierCurveSettings& BezierCurveSettings,
	const FAircraftAttackMoveSettings& AttackMoveSettings,
	const FAircraftDeadZone& DeadZoneSettings,
	const FDistAbsAngleSign& DistanceAngle, FVector& OutEndAirPoint
)
{
	// 0) Extreme‐angle guard
	if (GetIsExtremeAngleToEnemy(DistanceAngle))
	{
		AddAttackPathTooFar_ExtremeAngle(
			StartAirPoint, StartRotation,
			BezierCurveSettings, AttackMoveSettings, DeadZoneSettings,
			DistanceAngle, OutEndAirPoint
		);
		return;
	}
	if (GetIsEnemyExtremelyFar(DistanceAngle, AttackMoveSettings))
	{
		AddAttackPathTooFar_ExtremeDistance(
			StartAirPoint, StartRotation, EnemyAirLocation,
			BezierCurveSettings, AttackMoveSettings, DeadZoneSettings, OutEndAirPoint);
		return;
	}
	const bool bIsWithinAngle = (DistanceAngle.AbsAngle <= AttackMoveSettings.AngleAllowAttackDive);
	if (bIsWithinAngle)
	{
		AttackPathTooFar_WithinAngle(
			StartAirPoint, StartRotation, EnemyAirLocation, DistanceAngle,
			BezierCurveSettings, AttackMoveSettings, DeadZoneSettings,
			OutEndAirPoint);
		return;
	}
	// The angle is not extreme nor is the enemy very far; perform U-turn pathing.
	AddUTurnPathingToEnemy(StartAirPoint, StartRotation, EnemyAirLocation,
	                       BezierCurveSettings, AttackMoveSettings, DistanceAngle);
}


void FAircraftPath::AddAttackPathTooFar_ExtremeAngle(const FVector& StartAirPoint, const FRotator& StartRotation,
                                                     const FAircraftBezierCurveSettings& BezierCurveSettings,
                                                     const FAircraftAttackMoveSettings& AttackMoveSettings,
                                                     const FAircraftDeadZone& DeadZoneSettings,
                                                     const FDistAbsAngleSign& DistanceAngle, FVector& OutEndAirPoint)
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::DeltaAngleExtreme;
	// Set to the direction from aircraft to enemy with amount of degrees needed to recover the angle.
	OutEndAirPoint = FVector::ForwardVector.RotateAngleAxis(DeltaAngleExtreme * DistanceAngle.Sign,
	                                                        FVector::UpVector);
	OutEndAirPoint = StartAirPoint + OutEndAirPoint * AttackMoveSettings.AttackDiveRange.Y / 2;
	if (bDebugTooFar)
	{
		FRTSAircraftHelpers::AircraftSphereDebug(
			Owner,
			OutEndAirPoint,
			FColor::Purple,
			75,
			15);
	}
	AddMoveToPathFromStart(
		StartAirPoint,
		OutEndAirPoint,
		StartRotation,
		BezierCurveSettings,
		DeadZoneSettings);
	OnPathCompleteCalculateRotations(OutEndAirPoint, StartRotation);
}

void FAircraftPath::AddAttackPathTooFar_ExtremeDistance(const FVector& StartAirPoint, const FRotator& StartRotation,
                                                        const FVector& EnemyAirLocation,
                                                        const FAircraftBezierCurveSettings& BezierCurveSettings,
                                                        const FAircraftAttackMoveSettings& AttackMoveSettings,
                                                        const FAircraftDeadZone& DeadZoneSettings,
                                                        FVector& OutEndAirPoint)
{
	FRTSAircraftHelpers::AircraftDebug("Extremely Far", FColor::Green);
	const FRotator RotateToPlayer = UKismetMathLibrary::FindLookAtRotation(EnemyAirLocation, StartAirPoint);
	const FVector BezierEnd = EnemyAirLocation + RotateToPlayer.Vector() * AttackMoveSettings.AttackDiveRange.Y * 0.9;
	OutEndAirPoint = EnemyAirLocation + RotateToPlayer.Vector() * AttackMoveSettings.AttackDiveRange.Y * 1.2;

	AddMoveToPathFromStart(
		StartAirPoint,
		BezierEnd,
		StartRotation,
		BezierCurveSettings,
		DeadZoneSettings);

	OnPathCompleteCalculateRotations(OutEndAirPoint, StartRotation);
}

void FAircraftPath::AttackPathTooFar_WithinAngle(const FVector& StartAirPoint, const FRotator& StartRotation,
                                                 const FVector& EnemyAirLocation,
                                                 const FDistAbsAngleSign& DistanceAngle,
                                                 const FAircraftBezierCurveSettings& BezierCurveSettings,
                                                 const FAircraftAttackMoveSettings& AttackMoveSettings,
                                                 const FAircraftDeadZone& DeadZoneSettings, FVector& OutEndAirPoint)
{
	FRTSAircraftHelpers::AircraftDebug("Too far and within angle", FColor::Green);
	// Calculate fly to point that is within the attack range.
	const FVector GuessLocation = StartAirPoint + DistanceAngle.GetDirection() * DistanceAngle.Distance * 0.8;
	// Move the location to be within attack range of target.
	OutEndAirPoint = GetOffsetLocationInAttackRange(EnemyAirLocation, GuessLocation, AttackMoveSettings);

	// Note this does not add the start point.
	AddMoveToPathFromStart(
		StartAirPoint,
		OutEndAirPoint,
		StartRotation,
		BezierCurveSettings,
		DeadZoneSettings);
	OnPathCompleteCalculateRotations(OutEndAirPoint, StartRotation);
}

void FAircraftPath::AddAttackPathTooClose(const FVector& StartAirPoint, const FRotator& StartRotation,
                                          const FAircraftAttackMoveSettings& AttackMoveSettings)
{
	// Randomly go left or right.
	const bool bGoRight = FMath::RandBool();
	float OffsetAngle = FMath::FRandRange(5, AttackMoveSettings.AngleAllowAttackDive);
	if (not bGoRight)
	{
		OffsetAngle *= -1.f;
	}
	// by up vector from start rotation to degrees calculated.
	const FVector OffsetDir = StartRotation.Vector().RotateAngleAxis(OffsetAngle, FVector::UpVector).GetSafeNormal();
	const float OffsetDistance = AttackMoveSettings.AttackDiveRange.Y / 2;
	FVector OffsetLocation = StartAirPoint + OffsetDir * OffsetDistance;
	OffsetLocation.Z = StartAirPoint.Z;
	PathPoints.Add({OffsetLocation, StartRotation.Roll, EAirPathPointType::Regular});
	OnPathCompleteCalculateRotations(OffsetLocation, StartRotation);
	if (bDebugTooClose)
	{
		FRTSAircraftHelpers::AircraftDebug(
			FString::Printf(TEXT("Too close, offsetting by %.2f° to %s"), OffsetAngle, *OffsetLocation.ToString()),
			FColor::Yellow
		);
	}
}

EDirectAttackDiveResult FAircraftPath::GetIsWithinDirectAttackDiveRange(const FVector& StartAirPoint,
                                                                        const FRotator& StartRotation,
                                                                        const FVector& EnemyAirLocation,
                                                                        const FAircraftAttackMoveSettings&
                                                                        AttackMoveSettings,
                                                                        FDistAbsAngleSign&
                                                                        DistanceAngleToTarget) const
{
	const FVector Delta = EnemyAirLocation - StartAirPoint;
	const float Distance = Delta.Size();
	const FVector DirToTarget = Delta / Distance;
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	const float AngleDiffDeg = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, DirToTarget), -1.f, 1.f))
	);
	DistanceAngleToTarget.Distance = Distance;
	DistanceAngleToTarget.AbsAngle = FMath::Abs(AngleDiffDeg);
	DistanceAngleToTarget.Sign = FVector::CrossProduct(Forward, DirToTarget).Z >= 0.f ? 1.f : -1.f;
	// get local Y‑axis once, rotated into world‑space
	const FVector WorldRight = StartRotation.RotateVector(FVector::RightVector);
	// choose right or left simply by scaling
	DistanceAngleToTarget.LeftRightUnitVectorToEnemy = WorldRight * DistanceAngleToTarget.Sign;

	const bool bIsTooClose = (Distance < AttackMoveSettings.AttackDiveRange.X);
	if (bIsTooClose)
	{
		return EDirectAttackDiveResult::TooClose;
	}
	const bool bIsTooFar = (Distance > AttackMoveSettings.AttackDiveRange.Y);
	if (bIsTooFar)
	{
		return EDirectAttackDiveResult::TooFar;
	}
	const bool bIsWithinAngle = (AngleDiffDeg <= AttackMoveSettings.AngleAllowAttackDive);
	if (not bIsWithinAngle)
	{
		return EDirectAttackDiveResult::NotWithinAngle;
	}
	return EDirectAttackDiveResult::DivePossible;
}

void FAircraftPath::AddAttackDivePath(
	const FVector& DiveStartPoint,
	const FVector& EnemyLocation,
	const FAircraftAttackMoveSettings& AttackMoveSettings,
	FVector& OutFinalLocation
)
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::OutOfDivePointOffset;

	const float StartZ = DiveStartPoint.Z;
	const float DesiredZ = StartZ - AttackMoveSettings.DeltaAttackHeight;

	// -------------------------------------------------------------------------
	// 1) Compute alpha from the designer-controlled ratio:
	//    We want more DiveStrength -> attack point closer to the enemy (larger alpha)
	//            more RecoveryStrength -> earlier pull-out (smaller alpha).
	//    A stable mapping is:  alpha = Dive / (Dive + Recovery), always in (0,1).
	//    We clamp slightly away from the extremes to avoid degenerate 0-length legs.
	// -------------------------------------------------------------------------
	const float DiveStrength = FMath::Max(AttackMoveSettings.AttackDive_DiveStrength, 1.0f);
	const float RecoveryStrength = FMath::Max(AttackMoveSettings.AttackDive_RecoveryStrength, 1.0f);
	const float AlphaRaw = DiveStrength / (DiveStrength + RecoveryStrength);
	const float Alpha = FMath::Clamp(AlphaRaw, 0.001f, 0.999f);

	// Attack point lies along Start->Enemy at alpha, but its Z is forced to the desired dive depth.
	FVector AttackPoint = FMath::Lerp(DiveStartPoint, EnemyLocation, Alpha);
	AttackPoint.Z = DesiredZ;

	const FRotator AttackRot = UKismetMathLibrary::FindLookAtRotation(DiveStartPoint, AttackPoint);
	PathPoints.Add({AttackPoint, AttackRot.Roll, EAirPathPointType::AttackDive});

	// -------------------------------------------------------------------------
	// 2) Recovery leg length:
	//    Base recovery length = distance from AttackPoint to Enemy (in XY).
	//    This distance already reflects the Dive/Recovery ratio via alpha:
	//       |AP->Enemy| / |Start->AP| = RecoveryStrength / DiveStrength
	//    We then add with AttackDive_RecoveryLengthMlt for per-aircraft tuning.
	//    NOTE: We measure in 2D because we snap the recovery Z back to StartZ anyway.
	// -------------------------------------------------------------------------
	const float RecoveryBaseDist2D = (FVector2D(AttackPoint) - FVector2D(EnemyLocation)).Size();
	const float RecoveryLengthAdd = FMath::Max(AttackMoveSettings.AttackDive_RecoveryLengthAddition, 1.f);
	const float OutDiveDistance = RecoveryBaseDist2D + RecoveryLengthAdd;

	// Direction for recovery:
	// We use the horizontal (XY) direction of the dive (Start -> AttackPoint) so the pull-out
	// continues forward along the same bearing. We flatten to XY so that the actual path length
	// matches the 2D distance we just computed (after snapping Z to StartZ).
	FVector DiveDirXY = AttackPoint - DiveStartPoint;
	DiveDirXY.Z = 0.f;
	if (!DiveDirXY.Normalize())
	{
		// Fallback: if Start and AttackPoint share XY, use AttackPoint->Enemy bearing in XY.
		DiveDirXY = EnemyLocation - AttackPoint;
		DiveDirXY.Z = 0.f;
		if (!DiveDirXY.Normalize())
		{
			// Final fallback to world +X to avoid NaNs if everything is coincident.
			DiveDirXY = FVector(1.f, 0.f, 0.f);
		}
	}

	// -------------------------------------------------------------------------
	// 3) Out-of-dive (recovery) point:
	//    Move forward from the attack point along the dive bearing by the computed distance,
	//    then restore altitude to StartZ.
	// -------------------------------------------------------------------------
	FVector OutOfDivePoint = AttackPoint + DiveDirXY * OutDiveDistance;
	OutOfDivePoint.Z = StartZ;

	const FRotator OutOfDiveRot = UKismetMathLibrary::FindLookAtRotation(AttackPoint, OutOfDivePoint);
	PathPoints.Add({OutOfDivePoint, OutOfDiveRot.Roll, EAirPathPointType::GetOutOfDive});

	// -------------------------------------------------------------------------
	// 4) Final post-dive point:
	//    Step further along the same horizontal direction
	//    by the global OutOfDivePointOffset and keep Z at StartZ.
	// -------------------------------------------------------------------------
	OutFinalLocation = OutOfDivePoint + DiveDirXY * OutOfDivePointOffset;
	OutFinalLocation.Z = StartZ;

	const FRotator PostDiveRotation = UKismetMathLibrary::FindLookAtRotation(OutOfDivePoint, OutFinalLocation);
	PathPoints.Add({OutFinalLocation, PostDiveRotation.Roll, EAirPathPointType::Regular});

	if (bDebugAttackDive)
	{
		FRTSAircraftHelpers::AircraftDebug(
			TEXT("AttackDive: Alpha=") + FString::SanitizeFloat(Alpha) +
			TEXT("  DiveStrength=") + FString::SanitizeFloat(DiveStrength) +
			TEXT("  RecoveryStrength=") + FString::SanitizeFloat(RecoveryStrength) +
			TEXT("  RecBase2D=") + FString::SanitizeFloat(RecoveryBaseDist2D) +
			TEXT("  RecMlt=") + FString::SanitizeFloat(RecoveryLengthAdd) +
			TEXT("  OutDiveDist=") + FString::SanitizeFloat(OutDiveDistance),
			FColor::Yellow);
	}
}


bool FAircraftPath::GetIsWithinDeadZone(const FRotator& StartRotation, const FVector& Start,
                                        const FVector& PointToCheck, const FAircraftDeadZone& DeadZoneSettings,
                                        float& OutAngle) const
{
	const FVector Delta = PointToCheck - Start;
	const float Distance = Delta.Size();

	const FVector DirToTarget = Delta / Distance;
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	OutAngle = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, DirToTarget), -1.f, 1.f))
	);
	const bool bIsInAngleDeadZone = OutAngle >= DeadZoneSettings.AngleAtDeadZone;
	const bool bIsDistanceDeadZone = Distance <= DeadZoneSettings.DistanceAtDeadZone;
	if (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		if (bIsInAngleDeadZone && bIsDistanceDeadZone)
		{
			const FString DeadZoneText = "In DeadZone. Angle: " + FString::SanitizeFloat(OutAngle) +
				"° Distance: " + FString::SanitizeFloat(Distance);
			DrawDebugString(
				Owner ? Owner->GetWorld() : nullptr,
				PointToCheck,
				DeadZoneText,
				nullptr,
				FColor::Orange,
				8.f, false, 1.f
			);
		}
		else
		{
			const FString NotInDeadZoneText = "No DeadZone. Angle: " + FString::SanitizeFloat(OutAngle) +
				"° Distance: " + FString::SanitizeFloat(Distance);
			DrawDebugString(
				Owner ? Owner->GetWorld() : nullptr,
				PointToCheck,
				NotInDeadZoneText,
				nullptr,
				FColor::Green,
				8.f, false, 1.f);
		}
	}
	return bIsInAngleDeadZone && bIsDistanceDeadZone;
}

bool FAircraftPath::GetIsEndPointOutsideBezierReach(const FVector& StartAirPoint, const FVector& EndAirPoint,
                                                    const FAircraftBezierCurveSettings& BezierCurveSettings) const
{
	const float Distance = FVector::Distance(StartAirPoint, EndAirPoint);
	return Distance > BezierCurveSettings.MaxBezierDistance;
}

FVector FAircraftPath::GetBezierEndInReach(const FVector& StartAirPoint, const FVector& EndAirPoint,
                                           const FAircraftBezierCurveSettings& BezierCurveSettings) const
{
	// Get the unit vector in the direction from start to end
	const FVector DirToTarget = (EndAirPoint - StartAirPoint).GetSafeNormal();
	return StartAirPoint
		+ DirToTarget * BezierCurveSettings.MaxBezierDistance;
}

void FAircraftPath::AddSmallOrLargeBezierToPath(const FRotator& StartRotation, const FVector& BezierStart,
                                                const FVector& BezierEnd, const float CurveTension,
                                                const float AngleConsiderPointBehind)
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::MoveToAngleBezierSampleDivider;
	// 1) Early out if no movement needed
	const FVector Delta = BezierEnd - BezierStart;
	const float Distance = Delta.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		PathPoints.Add({BezierEnd});
		return;
	}

	// 2) Compute forward & target directions + angle
	const FVector DirToTarget = Delta / Distance;
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	const float AngleDiffDeg = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, DirToTarget), -1.f, 1.f))
	);
	const bool bIsToTheRight = FVector::CrossProduct(Forward, DirToTarget).Z >= 0.f;

	FRTSAircraftHelpers::AircraftDebug(
		FString::Printf(TEXT("Angle to target: %.1f° (threshold %.1f°)"),
		                AngleDiffDeg, AngleConsiderPointBehind),
		AngleDiffDeg > AngleConsiderPointBehind ? FColor::Yellow : FColor::Red
	);

	// 3) Determine sample count
	const int32 NumSamples = FMath::Max(0, FMath::CeilToInt(AngleDiffDeg / MoveToAngleBezierSampleDivider));

	// 4) Branch: quadratic vs cubic large-curve
	const bool bLargeTurn = (AngleDiffDeg > AngleConsiderPointBehind);
	if (bLargeTurn)
	{
		SetupLargeBezierMoveTo(
			BezierStart,
			BezierEnd,
			CurveTension,
			Distance,
			DirToTarget,
			NumSamples, bIsToTheRight, Forward);
	}
	else
	{
		const FVector Control = BezierStart + Forward * (Distance * CurveTension);
		AddQuadraticBezier(
			BezierStart,
			Control,
			BezierEnd,
			NumSamples,
			PathPoints
		);
	}
}

void FAircraftPath::SetupLargeBezierMoveTo(
	const FVector& StartAirPoint,
	const FVector& EndAirPoint,
	const float CurveTension,
	const float Distance,
	const FVector& DirToTarget,
	const int32 NumSamples,
	const bool bIsToTheRight,
	const FVector& Forward)
{
	FRTSAircraftHelpers::AircraftDebug(
		TEXT("Using cubic large-curve Bézier generator"), FColor::Yellow);
	FRTSAircraftHelpers::AircraftDebug(
		FString::Printf(TEXT("bIsToTheRight: %s"), bIsToTheRight ? TEXT("true") : TEXT("false")),
		FColor::Yellow);

	// 1) Build a lateral direction (unit)
	FVector SideDir = FVector::CrossProduct(DirToTarget, FVector::UpVector);
	SideDir.Normalize();
	if (!bIsToTheRight) SideDir *= -1.f;

	// 2) Scale both forward/back and sideways by tension * distance
	constexpr float CurveLength = 1.5f;
	const float ForwardDist = Distance * CurveTension;
	const float SideDist = Distance * CurveTension * CurveLength;
	// <-- tweak 0.5 to control how wide the side‐bend is

	const FVector Control1 = StartAirPoint
		+ Forward * ForwardDist
		+ SideDir * SideDist;

	const FVector Control2 = EndAirPoint
		- DirToTarget * ForwardDist
		+ SideDir * SideDist;

	// 3) Cubic Bézier with two independent tangents
	AddCubicBezierLargeCurve(
		StartAirPoint,
		Control1,
		Control2,
		EndAirPoint,
		NumSamples + 2, // a couple extra samples for smoothness
		PathPoints
	);
}


void FAircraftPath::AddQuadraticBezier(
	const FVector& P0,
	const FVector& P1,
	const FVector& P2,
	int32 NumSamples,
	TArray<FAircraftPathPoint>& OutPoints)
{
	if (NumSamples <= 0) return;
	const int32 Div = NumSamples + 1;
	for (int32 i = 1; i <= NumSamples; ++i)
	{
		const float t = float(i) / float(Div);
		const FVector A = FMath::Lerp(P0, P1, t);
		const FVector B = FMath::Lerp(P1, P2, t);
		// Roll will be set by OnPathCompleteCalculateRotations
		OutPoints.Add({FMath::Lerp(A, B, t), 0.f, EAirPathPointType::Bezier});
	}
}

void FAircraftPath::AddCubicBezierLargeCurve(
	const FVector& P0,
	const FVector& P1,
	const FVector& P2,
	const FVector& P3,
	int32 NumSamples,
	TArray<FAircraftPathPoint>& OutPoints)
{
	if (NumSamples <= 0) return;
	const int32 Div = NumSamples + 1;

	// De Casteljau for cubic Bézier:
	for (int32 i = 1; i <= NumSamples; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(Div);

		// First layer
		const FVector A = FMath::Lerp(P0, P1, t);
		const FVector B = FMath::Lerp(P1, P2, t);
		const FVector C = FMath::Lerp(P2, P3, t);

		// Second layer
		const FVector D = FMath::Lerp(A, B, t);
		const FVector E = FMath::Lerp(B, C, t);

		// Final point
		const FVector Sample = FMath::Lerp(D, E, t);
		OutPoints.Add({Sample, 0.f, EAirPathPointType::Bezier});
	}
}

void FAircraftPath::RemovePointsAfterAttackPossible(
	const FRotator& StartRotation,
	const FVector& EnemyAirLocation,
	const FAircraftAttackMoveSettings& AttackMoveSettings)
{
	// Stricter settings to tolerate pose drift when the actor restarts pathing from its current transform.
	FAircraftAttackMoveSettings StrictSettings = AttackMoveSettings;
	StrictSettings.AttackDiveRange.X *= 1.1f; // min range +10%
	StrictSettings.AttackDiveRange.Y *= 0.9f; // max range -10%
	StrictSettings.AngleAllowAttackDive *= 0.8f; // angle -20%

	// Safety: ensure min < max after tightening
	if (StrictSettings.AttackDiveRange.X >= StrictSettings.AttackDiveRange.Y)
	{
		const float Mid = (StrictSettings.AttackDiveRange.X + StrictSettings.AttackDiveRange.Y) * 0.5f;
		StrictSettings.AttackDiveRange.X = Mid * 0.95f;
		StrictSettings.AttackDiveRange.Y = Mid * 1.05f;
	}

	const int32 Num = PathPoints.Num();
	if (Num == 0)
	{
		return;
	}

	// Helper: is a dive possible at index i with a reasonable facing?
	auto IsDivePossibleAt = [&](int32 i) -> bool
	{
		const FVector& Curr = PathPoints[i].Location;

		FRotator FacingRot = StartRotation;
		if (i == 0)
		{
			// use StartRotation
		}
		else if (i < Num - 1)
		{
			// look from current to next
			FacingRot = UKismetMathLibrary::FindLookAtRotation(Curr, PathPoints[i + 1].Location);
		}
		else // i == Num - 1
		{
			// look from previous to current
			FacingRot = UKismetMathLibrary::FindLookAtRotation(PathPoints[i - 1].Location, Curr);
		}

		// Flatten enemy Z to this point's altitude
		const FVector EnemyAtPoint(EnemyAirLocation.X, EnemyAirLocation.Y, Curr.Z);

		FDistAbsAngleSign Dummy;
		const EDirectAttackDiveResult Res = GetIsWithinDirectAttackDiveRange(
			Curr,
			FacingRot,
			EnemyAtPoint,
			StrictSettings, 
			Dummy);

		return Res == EDirectAttackDiveResult::DivePossible;
	};

	// Scan from start; first success wins → trim after it
	for (int32 i = 0; i < Num; ++i)
	{
		if (IsDivePossibleAt(i))
		{
			PathPoints.SetNum(i + 1);
			// Ensure last point has the correct z.
			PathPoints[i].Location.Z = EnemyAirLocation.Z;
			return;
		}
	}
	// no point qualifies → keep path as-is
}


float FAircraftPath::GetRollToAddToBezierPoint(const FVector& Curr, const FVector& Next,
                                               const FRotator& StartRotation) const
{
	const FVector Delta = Next - Curr;
	const float Distance = Delta.Size();
	const FVector DirToTarget = Delta / Distance;
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	const float AngleDiffDeg = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, DirToTarget), -1.f, 1.f)));
	const float SideSign = (FVector::CrossProduct(Forward, Delta.GetSafeNormal()).Z >= 0.f) ? 1.f : -1.f;

	constexpr float MaxAddedRoll = 45;
	constexpr float MinAddedRoll = 10;
	// lerp from Min to Max roll based on angle
	const float RollToAdd = FMath::Lerp(MinAddedRoll, MaxAddedRoll, AngleDiffDeg / 180.f) * SideSign;
	return RollToAdd;
}

FVector FAircraftPath::GetBezierStartOutOfDeadZone(const FVector& StartAirPoint, const FVector& EndAirPoint,
                                                   const FRotator& StartRotation, const float AngleToFirstPoint,
                                                   const FAircraftDeadZone& DeadZoneSettings)
{
	using DeveloperSettings::GamePlay::Navigation::Aircraft::InDeadZonePointAngleDivider;
	using DeveloperSettings::GamePlay::Navigation::Aircraft::InDeadZonePointDistMlt;
	const FVector Forward = StartRotation.Vector().GetSafeNormal();
	const FVector ToTarget = (EndAirPoint - StartAirPoint).GetSafeNormal();
	const float SideSign = (FVector::CrossProduct(Forward, ToTarget).Z >= 0.f) ? 1.f : -1.f;
	const float StepAngle = (AngleToFirstPoint / InDeadZonePointAngleDivider) * SideSign;

	// Rotate forward by signed step
	const FVector SteppedDir = Forward
	                           .RotateAngleAxis(StepAngle, FVector::UpVector)
	                           .GetSafeNormal();

	// Push out by dead-zone distance × multiplier
	FVector BezierStart = StartAirPoint
		+ SteppedDir * (DeadZoneSettings.DistanceAtDeadZone * InDeadZonePointDistMlt);

	PathPoints.Add({BezierStart, 0.f, EAirPathPointType::Regular});
	return BezierStart;
}


void FAircraftPath::DebugPath(const float DebugTime)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const auto& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE)
			{
				World = Ctx.World();
				break;
			}
		}
	}
	if (!World) return;

	// Sizes for debug visuals
	constexpr float SphereRadius = 50.f;
	constexpr int32 Segments = 12;
	constexpr float LineWidth = 2.f;
	constexpr float ArrowLength = SphereRadius * 2.f; // how long each rotation‑arrow is
	constexpr float ArrowSize = SphereRadius * 0.5f; // size of the arrow head

	auto GetColorForType = [](EAirPathPointType Type) -> FColor
	{
		switch (Type)
		{
		case EAirPathPointType::Regular:
			return FColor::Green;
		case EAirPathPointType::Bezier:
			return FColor::Blue;
		case EAirPathPointType::AttackDive:
			return FColor::Red;
		case EAirPathPointType::GetOutOfDive:
			return FColor::Orange;
		default:
			return FColor::White;
		}
	};
	const int32 NumPts = PathPoints.Num();
	for (int32 i = 0; i < NumPts; ++i)
	{
		const FVector& Pos = PathPoints[i].Location;

		// 1) draw the sphere for the point
		DrawDebugSphere(
			World,
			Pos,
			SphereRadius,
			Segments,
			GetColorForType(PathPoints[i].PointType),
			/*bPersistentLines=*/ false,
			DebugTime
		);

		// 2) draw the connection to the next point
		if (i + 1 < NumPts)
		{
			DrawDebugLine(
				World,
				Pos,
				PathPoints[i + 1].Location,
				GetColorForType(PathPoints[i + 1].PointType),
				/*bPersistentLines=*/ false,
				DebugTime,
				/*DepthPriority=*/ 0,
				LineWidth
			);
		}
	}
}

namespace AircraftBezierRoll
{
	constexpr int32 Neighbors = 4;
	constexpr float MaxNeighborDistance = 300.f;
	constexpr float BezierRollExponent = 0.6f;
}

void FAircraftPath::OnPathCompleteCalculateRotations(
	const FVector& EndAirPoint,
	const FRotator& StartRotation)
{
	const int32 NumPoints = PathPoints.Num();
	if (NumPoints == 0) return;

	SetInitialRoll(StartRotation);
	if (NumPoints == 1) return;

	SetFinalRoll(EndAirPoint);
	CalculateInteriorRolls();
}

void FAircraftPath::SetInitialRoll(const FRotator& StartRotation)
{
	PathPoints[0].Roll = StartRotation.Roll;
}

void FAircraftPath::SetFinalRoll(const FVector& EndAirPoint)
{
	const int32 LastIdx = PathPoints.Num() - 1;
	const FRotator EndRot = UKismetMathLibrary::FindLookAtRotation(
		PathPoints[LastIdx - 1].Location,
		EndAirPoint
	);
	PathPoints[LastIdx].Roll = EndRot.Roll;
}

void FAircraftPath::CalculateInteriorRolls()
{
	const int32 NumPoints = PathPoints.Num();
	for (int32 i = 1; i < NumPoints - 1; ++i)
	{
		const FVector& Prev = PathPoints[i - 1].Location;
		const FVector& Curr = PathPoints[i].Location;
		const FVector& Next = PathPoints[i + 1].Location;

		// 1) Bezier‐curve roll bonus (if on curve)
		float RollBonus = 0.f;
		if (PathPoints[i].IsOnBezierCurve())
		{
			RollBonus = GetBezierRollBonus(i);

			TArray<FVector> PrevPts, NextPts;
			GatherBezierNeighbors(i, PrevPts, NextPts);

			if (PrevPts.Num() > 0 && NextPts.Num() > 0)
			{
				float ExtraRoll = GetExtraBezierRoll(Curr, PrevPts, NextPts, FMath::Sign(RollBonus));
				RollBonus += ExtraRoll;
			}
		}

		// 2) zero‐out on dive/recovery
		if (PathPoints[i + 1].IsDivePoint() ||
			PathPoints[i + 1].IsDiveRecoveryPoint())
		{
			PathPoints[i].Roll = 0.f;
			continue;
		}

		// 3) straight‐line blend
		PathPoints[i].Roll = GetBlendedRoll(Prev, Curr, Next) + RollBonus;
	}
}

float FAircraftPath::GetBezierRollBonus(int32 Index) const
{
	const FVector& Prev = PathPoints[Index - 1].Location;
	const FVector& Curr = PathPoints[Index].Location;
	const FVector& Next = PathPoints[Index + 1].Location;

	const FRotator PrevToCurr = UKismetMathLibrary::FindLookAtRotation(Prev, Curr);
	return GetRollToAddToBezierPoint(Curr, Next, PrevToCurr);
}

void FAircraftPath::GatherBezierNeighbors(int32 Index, TArray<FVector>& OutPrev, TArray<FVector>& OutNext) const
{
	const FVector& Curr = PathPoints[Index].Location;
	const int32 Half = AircraftBezierRoll::Neighbors / 2;
	const int32 Num = PathPoints.Num();

	for (int32 offset = 1; offset <= Half; ++offset)
	{
		int32 b = Index - offset;
		if (b >= 0)
		{
			const FVector& P = PathPoints[b].Location;
			if ((P - Curr).Size() <= AircraftBezierRoll::MaxNeighborDistance)
			{
				OutPrev.Add(P);
			}
		}

		int32 a = Index + offset;
		if (a < Num)
		{
			const FVector& P = PathPoints[a].Location;
			if ((P - Curr).Size() <= AircraftBezierRoll::MaxNeighborDistance)
			{
				OutNext.Add(P);
			}
		}
	}
}

float FAircraftPath::GetExtraBezierRoll(
	const FVector& Curr,
	const TArray<FVector>& Prev,
	const TArray<FVector>& Next,
	float BonusSign) const
{
	const FVector Before = (Prev.Last() - Curr).GetSafeNormal();
	const FVector After = (Next.Last() - Curr).GetSafeNormal();
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Before, After)));
	return FMath::Pow(AngleDeg, AircraftBezierRoll::BezierRollExponent) * BonusSign;
}

float FAircraftPath::GetBlendedRoll(
	const FVector& Prev,
	const FVector& Curr,
	const FVector& Next) const
{
	float dPrev = (Curr - Prev).Size();
	float dNext = (Next - Curr).Size();
	float total = dPrev + dNext;
	float w = (total > KINDA_SMALL_NUMBER) ? (dPrev / total) : 0.5f;

	FRotator r0 = (Curr - Prev).GetSafeNormal().Rotation();
	FRotator r1 = (Next - Curr).GetSafeNormal().Rotation();
	FQuat q0 = r0.Quaternion();
	FQuat q1 = r1.Quaternion();

	return FQuat::Slerp(q0, q1, w).Rotator().Roll;
}
