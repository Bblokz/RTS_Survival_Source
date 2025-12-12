#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/Units/Aircraft/AircraftPath/FAircraftPath.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

#include "AircraftCommandsData.generated.h"

/**
 * @brief Data needed for idle‐circling: center point and current sweep angle.
 */
USTRUCT()
struct FAircraftIdleData
{
	GENERATED_BODY()

	/** World location (XYZ) around which to orbit when idle */
	UPROPERTY()
	FVector CenterPoint = FVector::ZeroVector;

	/** Current angle in radians around the circle */
	UPROPERTY()
	float CurrentAngle = 0.f;

	/** Total duration of soft entry into idle circling (seconds) */
	UPROPERTY()
	float GraceTotalDuration = 0.f;

	/** Remaining time of the grace period (seconds) */
	UPROPERTY()
	float GraceTimeRemaining = 0.f;

	/** Minimum steer scale at the start of grace (0..1); ramps to 1 by grace end */
	UPROPERTY()
	float GraceMinSteerScale = 0.5f;
};



USTRUCT()
struct FAAircraftMoveData
{
	GENERATED_BODY()

	void Reset();
	void StartPathFinding(const FVector& StartPoint, const FRotator& StartRotation,
	                      const FAircraftBezierCurveSettings& BezierCurveSettings,
	                      const FAircraftDeadZone& DeadZoneSettings);
	void SetTargetPoint(const FVector& TargetPoint);
	FVector GetTargetPoint() const;

	UPROPERTY()
	FAircraftPath Path;

private:
	/** Starting world location at order time */
	UPROPERTY()
	FVector M_StartPoint = FVector::ZeroVector;

	/** Desired world location (with proper Z) */
	UPROPERTY()
	FVector M_TargetPoint = FVector::ZeroVector;
};


USTRUCT()
struct FAAircraftAttackData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	void Reset();

	bool IsTargetActorVisible(const uint8 OwningPlayer) const;

	void StartPathFinding(
		const FVector& StartPoint, const FRotator& StartRotation,
		const FAircraftBezierCurveSettings& BezierCurveSettings, const FAircraftAttackMoveSettings& AttackMoveSettings, const
		FAircraftDeadZone DeadZoneSettings);

	UPROPERTY()
	FAircraftPath Path;
};

USTRUCT()
struct FAircraftLandedData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector LandedPosition = FVector::ZeroVector;

	UPROPERTY()
	FRotator LandedRotation = FRotator::ZeroRotator;

	// Calculated height z value when we take off.
	UPROPERTY()
	float TargetAirborneHeight = 0.f;

	// Used to start animations
	UPROPERTY()
	FTimerHandle Vto_PrepareHandle;
};

// What to do once vertical take‐off (VTO) completes
enum class EPostLiftOffAction : uint8
{
	Idle,
	Move,
	Attack,
	// Set if the aircraft was landed while the player assigned it a new owner.
	// The new owner will then be set post Lift-off and the data in the old owner will be cleared post Lift-off.
	ChangeOwner
};

/**
 * @brief State tracked when an aircraft is instructed to return to base (owner).
 */
USTRUCT()
struct FAAircraftReturnToBaseData
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<class UAircraftOwnerComp> TargetOwner;

	UPROPERTY()
	int32 SocketIndex = INDEX_NONE;

	UPROPERTY()
	bool bWaitingForDoor = false;

	/** True while we're pathing to the air position above our socket. */
	UPROPERTY()
	bool bNavigatingToSocket = false;

	void Reset()
	{
		TargetOwner.Reset();
		SocketIndex = INDEX_NONE;
		bWaitingForDoor = false;
		bNavigatingToSocket = false;
	}
};


USTRUCT()
struct FPendingPostLiftOffAction
{
	GENERATED_BODY()
	FPendingPostLiftOffAction();
	
	EPostLiftOffAction GetAction() const;
	TWeakObjectPtr<UAircraftOwnerComp> GetNewOwner() const;
	void Set(EPostLiftOffAction NewAction, UAircraftOwnerComp* NewOwner = nullptr);
	void Reset();
private:

	EPostLiftOffAction M_Action = EPostLiftOffAction::Idle;
	TWeakObjectPtr<class UAircraftOwnerComp> M_NewOwner = nullptr;
};
