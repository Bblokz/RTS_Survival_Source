#include "AircraftCommandsData.h"

#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


void FAAircraftMoveData::Reset()
{
	M_StartPoint = FVector::ZeroVector;
	M_TargetPoint = FVector::ZeroVector;
	Path.Reset();
}

void FAAircraftMoveData::StartPathFinding(const FVector& StartPoint, const FRotator& StartRotation,
                                          const FAircraftBezierCurveSettings& BezierCurveSettings,
                                          const FAircraftDeadZone& DeadZoneSettings)
{
	M_StartPoint = StartPoint;
	Path.CreateMoveToPath(StartPoint, M_TargetPoint,
	                      StartRotation, BezierCurveSettings, DeadZoneSettings);
}

void FAAircraftMoveData::SetTargetPoint(const FVector& TargetPoint)
{
	M_TargetPoint = TargetPoint;
}

FVector FAAircraftMoveData::GetTargetPoint() const
{
	return M_TargetPoint;
}

void FAAircraftAttackData::Reset()
{
	Path.Reset();
}

bool FAAircraftAttackData::IsTargetActorVisible(const uint8 Owningplayer) const
{
	if (RTSFunctionLibrary::RTSIsVisibleTarget(TargetActor, Owningplayer))
	{
		return true;
	}
	return false;
}

void FAAircraftAttackData::StartPathFinding(const FVector& StartPoint, const FRotator& StartRotation,
                                            const FAircraftBezierCurveSettings& BezierCurveSettings,
                                            const FAircraftAttackMoveSettings& AttackMoveSettings, const
                                            FAircraftDeadZone DeadZoneSettings)
{
	Path.CreateAttackPath(
		StartPoint, StartRotation,
		TargetActor->GetActorLocation(),
		BezierCurveSettings, AttackMoveSettings, DeadZoneSettings);
}

FPendingPostLiftOffAction::FPendingPostLiftOffAction()
{
	M_Action = EPostLiftOffAction::Idle;
	M_NewOwner = nullptr;
}

EPostLiftOffAction FPendingPostLiftOffAction::GetAction() const
{
	return M_Action;
}

TWeakObjectPtr<UAircraftOwnerComp> FPendingPostLiftOffAction::GetNewOwner() const
{
	return M_NewOwner;
}

void FPendingPostLiftOffAction::Set(EPostLiftOffAction NewAction, UAircraftOwnerComp* NewOwner)
{
	M_Action = NewAction;
	M_NewOwner = NewOwner;
}

void FPendingPostLiftOffAction::Reset()
{
	M_Action = EPostLiftOffAction::Idle;
	M_NewOwner = nullptr;
}
