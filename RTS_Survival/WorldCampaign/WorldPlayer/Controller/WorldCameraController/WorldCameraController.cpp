// Copyright (C) Bas Blokzijl - All rights reserved.


#include "WorldCameraController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"

UWorldCameraController::UWorldCameraController()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldCameraController::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitCameraCarrier();
	BeginPlay_InitSpringArmComponent();
}

void UWorldCameraController::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCameraInputDisabled(DeltaTime);
	UpdateCameraOvertake(DeltaTime);
	UpdateMoveToLocation(DeltaTime);
	UpdateAxisMovement(DeltaTime);
	UpdateEdgeScroll(DeltaTime);
	UpdateBoundarySafety();
}

void UWorldCameraController::SetIsCameraMovementDisabled(const bool bIsDisabled)
{
	bM_IsCameraMovementDisabled = bIsDisabled;
	if (bM_IsCameraMovementDisabled)
	{
		M_AxisInputState = {};
	}
}

bool UWorldCameraController::GetIsCameraMovementDisabled() const
{
	return bM_IsCameraMovementDisabled || M_CameraInputDisabledRemainingSeconds > 0.0f;
}

void UWorldCameraController::ZoomIn()
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (GetIsValidSpringArmComponent())
	{
		const float NewTargetLength = FMath::Clamp(M_SpringArmComponent->TargetArmLength - M_ZoomSpeed, M_MinZoom, M_MaxZoom);
		M_SpringArmComponent->TargetArmLength = NewTargetLength;
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector CurrentLocation = CameraCarrier->GetActorLocation();
	const FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, -M_ZoomSpeed);
	TryMoveCameraToLocation(NewLocation);
}

void UWorldCameraController::ZoomOut()
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (GetIsValidSpringArmComponent())
	{
		const float NewTargetLength = FMath::Clamp(M_SpringArmComponent->TargetArmLength + M_ZoomSpeed, M_MinZoom, M_MaxZoom);
		M_SpringArmComponent->TargetArmLength = NewTargetLength;
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector CurrentLocation = CameraCarrier->GetActorLocation();
	const FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, M_ZoomSpeed);
	TryMoveCameraToLocation(NewLocation);
}

void UWorldCameraController::ForwardMovement(const float AxisValue)
{
	M_AxisInputState.M_ForwardAxisValue = AxisValue;
}

void UWorldCameraController::RightMovement(const float AxisValue)
{
	M_AxisInputState.M_RightAxisValue = AxisValue;
}

void UWorldCameraController::MoveCameraTo(const FMovePlayerCamera& MoveRequest)
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	if (M_CameraOvertakeState.bM_IsActive)
	{
		return;
	}

	PlayOptionalMoveSound(MoveRequest.MoveSound);

	M_CameraInputDisabledRemainingSeconds = FMath::Max(
		M_CameraInputDisabledRemainingSeconds,
		MoveRequest.TimeCameraInputDisabled
	);

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	if (MoveRequest.TimeToMove <= 0.0f)
	{
		TryMoveCameraToLocation(MoveRequest.MoveToLocation);
		M_MoveState.bM_IsMoveActive = false;
		M_MoveState.bM_IgnoreBoundaryConstraints = false;
		return;
	}

	M_MoveState.M_StartLocation = CameraCarrier->GetActorLocation();
	M_MoveState.M_TargetLocation = MoveRequest.MoveToLocation;
	M_MoveState.M_ElapsedTime = 0.0f;
	M_MoveState.M_TotalTime = MoveRequest.TimeToMove;
	M_MoveState.bM_IsMoveActive = true;
	M_MoveState.bM_IgnoreBoundaryConstraints = false;
}

bool UWorldCameraController::StartCameraOvertake(
	const FCameraOvertakeSettings& CameraOvertakeSettings,
	const FWorldCameraOvertakePointReachedNativeDelegate& PointReachedDelegate,
	const FWorldCameraOvertakeFinishedNativeDelegate& FinishedDelegate)
{
	if (not GetCanStartCameraOvertake(CameraOvertakeSettings))
	{
		return false;
	}

	CancelActiveCameraMove();
	CacheCameraOvertakeDurations(CameraOvertakeSettings);
	M_CameraOvertakeState.M_PointReachedDelegate = PointReachedDelegate;
	M_CameraOvertakeState.M_FinishedDelegate = FinishedDelegate;
	M_CameraOvertakeState.bM_IsActive = true;
	M_CameraOvertakeState.M_LastReachedLocation = GetCurrentCameraLocation();
	constexpr int32 FirstPointIndex = 0;
	StartCameraOvertakeSegment(FirstPointIndex);
	if (M_CameraOvertakeState.M_SegmentTotalTime <= 0.0f)
	{
		CompleteCameraOvertakeSegment();
	}
	return true;
}

bool UWorldCameraController::GetIsCameraOvertakeActive() const
{
	return M_CameraOvertakeState.bM_IsActive;
}

FVector UWorldCameraController::GetCurrentCameraLocation() const
{
	if (not GetIsValidCameraCarrierActor())
	{
		return FVector::ZeroVector;
	}

	const AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	return CameraCarrier->GetActorLocation();
}


bool UWorldCameraController::RegisterAdditionalCameraBoundary(
	const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams)
{
	if (not GetIsBoundaryIdValidForRegistration(BoundaryRegistrationParams.M_BoundaryId,
	                                            TEXT("RegisterAdditionalCameraBoundary")))
	{
		return false;
	}
	if (not GetCanBuildBoundaryFromActor(BoundaryRegistrationParams.M_BoundaryActor,
	                                     TEXT("RegisterAdditionalCameraBoundary")))
	{
		return false;
	}

	const AActor* const BoundaryActor = BoundaryRegistrationParams.M_BoundaryActor.Get();
	TArray<FPlayerCameraBoundaryPlaneConstraint> CachedPlaneConstraints = {};
	const bool bHasAnyBlockedSideFlags = GetHasAnyBlockedSideFlags(BoundaryRegistrationParams.M_BlockedSideFlags);
	if (BoundaryRegistrationParams.bM_UseActorBoundsAsBoundary && not bHasAnyBlockedSideFlags)
	{
		const FBox WorldSpaceActorBounds = BoundaryActor->GetComponentsBoundingBox(true);
		AddBoxBoundaryConstraints(WorldSpaceActorBounds, CachedPlaneConstraints);
	}

	AddDirectionalSideConstraints(
		BoundaryActor,
		BoundaryRegistrationParams.M_AxisSpaceForBlockedSides,
		BoundaryRegistrationParams.M_BlockedSideFlags,
		CachedPlaneConstraints);
	if (CachedPlaneConstraints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("No world camera boundary constraints were generated for boundary id: "
			+ BoundaryRegistrationParams.M_BoundaryId.ToString()
			+ "\n See function: UWorldCameraController::RegisterAdditionalCameraBoundary()");
		return false;
	}

	for (FPlayerCameraBoundarySubmissionCache& ExistingSubmission : M_AdditionalBoundarySubmissionCaches)
	{
		if (ExistingSubmission.M_BoundaryId != BoundaryRegistrationParams.M_BoundaryId)
		{
			continue;
		}

		ExistingSubmission.M_CachedPlaneConstraints = CachedPlaneConstraints;
		RebuildCachedAdditionalPlaneConstraints();
		return true;
	}

	FPlayerCameraBoundarySubmissionCache NewSubmission = {};
	NewSubmission.M_BoundaryId = BoundaryRegistrationParams.M_BoundaryId;
	NewSubmission.M_CachedPlaneConstraints = CachedPlaneConstraints;
	M_AdditionalBoundarySubmissionCaches.Add(NewSubmission);
	RebuildCachedAdditionalPlaneConstraints();
	return true;
}

bool UWorldCameraController::RemoveAdditionalCameraBoundaryById(const FName BoundaryId)
{
	if (not GetIsBoundaryIdValidForRegistration(BoundaryId, TEXT("RemoveAdditionalCameraBoundaryById")))
	{
		return false;
	}

	const int32 InitialSubmissionCount = M_AdditionalBoundarySubmissionCaches.Num();
	M_AdditionalBoundarySubmissionCaches.RemoveAll(
		[BoundaryId](const FPlayerCameraBoundarySubmissionCache& Submission)
		{
			return Submission.M_BoundaryId == BoundaryId;
		});

	if (M_AdditionalBoundarySubmissionCaches.Num() == InitialSubmissionCount)
	{
		return false;
	}

	RebuildCachedAdditionalPlaneConstraints();
	return true;
}

void UWorldCameraController::RemoveAllAdditionalCameraBounds()
{
	M_AdditionalBoundarySubmissionCaches.Empty();
	M_CachedAdditionalPlaneConstraints.Empty();
}

void UWorldCameraController::BeginPlay_InitCameraCarrier()
{
	AActor* const OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	if (APlayerController* const PlayerController = Cast<APlayerController>(OwnerActor))
	{
		if (APawn* const PossessedPawn = PlayerController->GetPawn())
		{
			M_CameraCarrierActor = PossessedPawn;
			return;
		}
	}

	M_CameraCarrierActor = OwnerActor;
}

void UWorldCameraController::BeginPlay_InitSpringArmComponent()
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	M_SpringArmComponent = CameraCarrier->FindComponentByClass<USpringArmComponent>();
}

void UWorldCameraController::UpdateCameraInputDisabled(const float DeltaTime)
{
	if (M_CameraInputDisabledRemainingSeconds <= 0.0f)
	{
		M_CameraInputDisabledRemainingSeconds = 0.0f;
		return;
	}

	M_CameraInputDisabledRemainingSeconds = FMath::Max(M_CameraInputDisabledRemainingSeconds - DeltaTime, 0.0f);
}

void UWorldCameraController::UpdateMoveToLocation(const float DeltaTime)
{
	if (M_CameraOvertakeState.bM_IsActive)
	{
		return;
	}

	if (not M_MoveState.bM_IsMoveActive)
	{
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		M_MoveState.bM_IsMoveActive = false;
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	if (M_MoveState.M_TotalTime <= 0.0f)
	{
		TryMoveCameraToLocation(M_MoveState.M_TargetLocation);
		M_MoveState.bM_IsMoveActive = false;
		M_MoveState.bM_IgnoreBoundaryConstraints = false;
		bM_IsSafetyMoveToPlayerHQActive = false;
		return;
	}

	M_MoveState.M_ElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(M_MoveState.M_ElapsedTime / M_MoveState.M_TotalTime, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(M_MoveState.M_StartLocation, M_MoveState.M_TargetLocation, Alpha);
	if (M_MoveState.bM_IgnoreBoundaryConstraints)
	{
		CameraCarrier->SetActorLocation(NewLocation, true);
	}
	else
	{
		TryMoveCameraToLocation(NewLocation);
	}

	if (Alpha >= 1.0f)
	{
		if (M_MoveState.bM_IgnoreBoundaryConstraints)
		{
			CameraCarrier->SetActorLocation(M_MoveState.M_TargetLocation, true);
		}
		else
		{
			TryMoveCameraToLocation(M_MoveState.M_TargetLocation);
		}
		M_MoveState.bM_IsMoveActive = false;
		M_MoveState.bM_IgnoreBoundaryConstraints = false;
		bM_IsSafetyMoveToPlayerHQActive = false;
	}
}

void UWorldCameraController::UpdateCameraOvertake(const float DeltaTime)
{
	if (not M_CameraOvertakeState.bM_IsActive)
	{
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		FinishCameraOvertake();
		return;
	}

	if (M_CameraOvertakeState.M_SegmentTotalTime <= 0.0f)
	{
		CompleteCameraOvertakeSegment();
		return;
	}

	M_CameraOvertakeState.M_SegmentElapsedTime += DeltaTime;
	const float Alpha = FMath::Clamp(
		M_CameraOvertakeState.M_SegmentElapsedTime / M_CameraOvertakeState.M_SegmentTotalTime,
		0.0f,
		1.0f);
	const FVector NewLocation = FMath::Lerp(
		M_CameraOvertakeState.M_SegmentStartLocation,
		M_CameraOvertakeState.M_SegmentTargetLocation,
		Alpha);
	TryMoveCameraToLocation(NewLocation);
	if (Alpha >= 1.0f)
	{
		CompleteCameraOvertakeSegment();
	}
}

void UWorldCameraController::UpdateAxisMovement(const float DeltaTime)
{
	if (GetIsCameraMovementDisabled())
	{
		return;
	}

	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	constexpr float AxisDeadzone = KINDA_SMALL_NUMBER;
	const float ForwardAxisValue = FMath::Abs(M_AxisInputState.M_ForwardAxisValue) > AxisDeadzone
		? M_AxisInputState.M_ForwardAxisValue
		: 0.0f;
	const float RightAxisValue = FMath::Abs(M_AxisInputState.M_RightAxisValue) > AxisDeadzone
		? M_AxisInputState.M_RightAxisValue
		: 0.0f;
	if (ForwardAxisValue == 0.0f && RightAxisValue == 0.0f)
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector ForwardVector = CameraCarrier->GetActorForwardVector();
	const FVector RightVector = CameraCarrier->GetActorRightVector();
	const FVector WorldDelta = (ForwardVector * ForwardAxisValue + RightVector * RightAxisValue) * M_XYSpeed * DeltaTime;
	TryMoveCameraByWorldDelta(WorldDelta);
}

bool UWorldCameraController::GetIsValidCameraCarrierActor() const
{
	if (M_CameraCarrierActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CameraCarrierActor"),
		TEXT("GetIsValidCameraCarrierActor"),
		this
	);
	return false;
}

bool UWorldCameraController::GetIsValidSpringArmComponent() const
{
	if (M_SpringArmComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_SpringArmComponent"),
		TEXT("GetIsValidSpringArmComponent"),
		this
	);
	return false;
}


void UWorldCameraController::UpdateEdgeScroll(const float DeltaTime)
{
	if (GetIsCameraMovementDisabled() || not GetIsValidCameraCarrierActor())
	{
		M_EdgeScrollAccelX = 1.0f;
		M_EdgeScrollAccelY = 1.0f;
		return;
	}

	float ScreenX = 0.0f;
	float ScreenY = 0.0f;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	bool bIsMouseInViewport = false;
	GetViewportSizeAndMouse(ScreenX, ScreenY, MouseX, MouseY, bIsMouseInViewport);
	if (not bIsMouseInViewport || ScreenX <= 0.0f || ScreenY <= 0.0f)
	{
		M_EdgeScrollAccelX = 1.0f;
		M_EdgeScrollAccelY = 1.0f;
		return;
	}

	constexpr float MaxEdgeScrollStartZone = 0.5f;
	const float NormalizedMouseX = MouseX / ScreenX;
	const float NormalizedMouseY = MouseY / ScreenY;
	const float EdgeScrollStartZone = FMath::Clamp(1.0f - EdgeScrollingPercentage, KINDA_SMALL_NUMBER, MaxEdgeScrollStartZone);
	const float EdgeScrollHardZone = FMath::Min(M_EdgeScrollHardZone, EdgeScrollStartZone);
	const float ZoneSpan = FMath::Max(EdgeScrollStartZone - EdgeScrollHardZone, KINDA_SMALL_NUMBER);
	int32 DirectionX = 0;
	int32 DirectionY = 0;
	float StrengthX = 0.0f;
	float StrengthY = 0.0f;
	if (NormalizedMouseX <= EdgeScrollHardZone)
	{
		DirectionX = -1;
		StrengthX = 1.0f;
	}
	else if (NormalizedMouseX <= EdgeScrollStartZone)
	{
		DirectionX = -1;
		StrengthX = FMath::Clamp((EdgeScrollStartZone - NormalizedMouseX) / ZoneSpan, 0.0f, 1.0f);
	}
	else if (NormalizedMouseX >= 1.0f - EdgeScrollHardZone)
	{
		DirectionX = 1;
		StrengthX = 1.0f;
	}
	else if (NormalizedMouseX >= 1.0f - EdgeScrollStartZone)
	{
		DirectionX = 1;
		StrengthX = FMath::Clamp((NormalizedMouseX - (1.0f - EdgeScrollStartZone)) / ZoneSpan, 0.0f, 1.0f);
	}

	if (NormalizedMouseY <= EdgeScrollHardZone)
	{
		DirectionY = 1;
		StrengthY = 1.0f;
	}
	else if (NormalizedMouseY <= EdgeScrollStartZone)
	{
		DirectionY = 1;
		StrengthY = FMath::Clamp((EdgeScrollStartZone - NormalizedMouseY) / ZoneSpan, 0.0f, 1.0f);
	}
	else if (NormalizedMouseY >= 1.0f - EdgeScrollHardZone)
	{
		DirectionY = -1;
		StrengthY = 1.0f;
	}
	else if (NormalizedMouseY >= 1.0f - EdgeScrollStartZone)
	{
		DirectionY = -1;
		StrengthY = FMath::Clamp((NormalizedMouseY - (1.0f - EdgeScrollStartZone)) / ZoneSpan, 0.0f, 1.0f);
	}

	M_EdgeScrollAccelX = DirectionX != 0
		? FMath::Clamp(M_EdgeScrollAccelX + M_EdgeScrollAccelerationRate * DeltaTime, 1.0f, M_EdgeScrollMaxAccelerationMultiplier)
		: 1.0f;
	M_EdgeScrollAccelY = DirectionY != 0
		? FMath::Clamp(M_EdgeScrollAccelY + M_EdgeScrollAccelerationRate * DeltaTime, 1.0f, M_EdgeScrollMaxAccelerationMultiplier)
		: 1.0f;
	if (DirectionX == 0 && DirectionY == 0)
	{
		return;
	}

	const FVector WorldDelta = GetWorldDeltaFromLocalXY(
		DirectionY * M_XYSpeed * EdgeScrollingSpeedMlt * M_EdgeScrollAccelY * StrengthY * DeltaTime,
		DirectionX * M_XYSpeed * EdgeScrollingSpeedMlt * M_EdgeScrollAccelX * StrengthX * DeltaTime);
	TryMoveCameraByWorldDelta(WorldDelta);
}

void UWorldCameraController::UpdateBoundarySafety()
{
	if (M_CameraOvertakeState.bM_IsActive || bM_IsSafetyMoveToPlayerHQActive || not GetIsCameraOutsideBoundaries())
	{
		return;
	}

	StartSafetyMoveToPlayerHQ();
}

void UWorldCameraController::GetViewportSizeAndMouse(
	float& OutScreenX,
	float& OutScreenY,
	float& OutMouseX,
	float& OutMouseY,
	bool& bOutIsMouseInViewport) const
{
	if (GEngine == nullptr || GEngine->GameViewport == nullptr || GetWorld() == nullptr)
	{
		OutScreenX = 0.0f;
		OutScreenY = 0.0f;
		OutMouseX = 0.0f;
		OutMouseY = 0.0f;
		bOutIsMouseInViewport = false;
		return;
	}

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
	const float ViewScale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld()) * ViewScale;
	OutScreenX = ViewportSize.X;
	OutScreenY = ViewportSize.Y;
	OutMouseX = MousePosition.X;
	OutMouseY = MousePosition.Y;
	bOutIsMouseInViewport = OutMouseX >= 0.0f && OutMouseX <= OutScreenX && OutMouseY >= 0.0f && OutMouseY <= OutScreenY;
}

FVector UWorldCameraController::GetWorldDeltaFromLocalXY(const float LocalForwardValue, const float LocalRightValue) const
{
	if (not GetIsValidCameraCarrierActor())
	{
		return FVector::ZeroVector;
	}

	const AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	const FVector ForwardVector = CameraCarrier->GetActorForwardVector();
	const FVector RightVector = CameraCarrier->GetActorRightVector();
	return ForwardVector * LocalForwardValue + RightVector * LocalRightValue;
}

bool UWorldCameraController::GetCanMoveCameraToLocation(const FVector& TargetCameraLocation) const
{
	return GetAreAllPlaneConstraintsSatisfied(TargetCameraLocation, M_CachedAdditionalPlaneConstraints);
}

FVector UWorldCameraController::GetConstrainedCameraLocation(const FVector& TargetCameraLocation) const
{
	constexpr float PlaneSignedDistanceTolerance = 0.1f;
	FVector ConstrainedLocation = TargetCameraLocation;
	for (const FPlayerCameraBoundaryPlaneConstraint& Constraint : M_CachedAdditionalPlaneConstraints)
	{
		if (Constraint.bM_HasSpanLimit)
		{
			const float SpanAxisValue = FVector::DotProduct(ConstrainedLocation, Constraint.M_SpanAxis);
			if (SpanAxisValue < Constraint.M_SpanAxisMin - PlaneSignedDistanceTolerance
				|| SpanAxisValue > Constraint.M_SpanAxisMax + PlaneSignedDistanceTolerance)
			{
				continue;
			}
		}

		const FVector Difference = ConstrainedLocation - Constraint.M_PlaneOrigin;
		const float SignedDistance = FVector::DotProduct(Difference, Constraint.M_PlaneNormal);
		if (Constraint.bM_AllowPositiveSide && SignedDistance < 0.0f)
		{
			ConstrainedLocation -= Constraint.M_PlaneNormal * SignedDistance;
			continue;
		}
		if (not Constraint.bM_AllowPositiveSide && SignedDistance > 0.0f)
		{
			ConstrainedLocation -= Constraint.M_PlaneNormal * SignedDistance;
		}
	}

	return ConstrainedLocation;
}

bool UWorldCameraController::GetIsCameraOutsideBoundaries() const
{
	if (M_CachedAdditionalPlaneConstraints.IsEmpty())
	{
		return false;
	}
	if (not GetIsValidCameraCarrierActor())
	{
		return false;
	}

	const AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	return not GetCanMoveCameraToLocation(CameraCarrier->GetActorLocation());
}

void UWorldCameraController::StartSafetyMoveToPlayerHQ()
{
	if (bM_IsSafetyMoveToPlayerHQActive || not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AWorldPlayerController* const WorldPlayerController = Cast<AWorldPlayerController>(GetOwner());
	if (not IsValid(WorldPlayerController))
	{
		RTSFunctionLibrary::ReportError("World camera safety move needs an AWorldPlayerController owner."
			"\n See function: UWorldCameraController::StartSafetyMoveToPlayerHQ()");
		return;
	}

	constexpr float SafetyMoveDurationSeconds = 2.0f;
	constexpr float SafetyInputDisabledSeconds = 2.0f;
	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	M_MoveState.M_StartLocation = CameraCarrier->GetActorLocation();
	M_MoveState.M_TargetLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(WorldPlayerController);
	M_MoveState.M_ElapsedTime = 0.0f;
	M_MoveState.M_TotalTime = SafetyMoveDurationSeconds;
	M_MoveState.bM_IsMoveActive = true;
	M_MoveState.bM_IgnoreBoundaryConstraints = true;
	M_CameraInputDisabledRemainingSeconds = FMath::Max(
		M_CameraInputDisabledRemainingSeconds,
		SafetyInputDisabledSeconds
	);
	bM_IsSafetyMoveToPlayerHQActive = true;
}

bool UWorldCameraController::GetAreAllPlaneConstraintsSatisfied(
	const FVector& TargetCameraLocation,
	const TArray<FPlayerCameraBoundaryPlaneConstraint>& PlaneConstraints) const
{
	constexpr float PlaneSignedDistanceTolerance = 0.1f;
	for (const FPlayerCameraBoundaryPlaneConstraint& Constraint : PlaneConstraints)
	{
		if (Constraint.bM_HasSpanLimit)
		{
			const float SpanAxisValue = FVector::DotProduct(TargetCameraLocation, Constraint.M_SpanAxis);
			if (SpanAxisValue < Constraint.M_SpanAxisMin - PlaneSignedDistanceTolerance
				|| SpanAxisValue > Constraint.M_SpanAxisMax + PlaneSignedDistanceTolerance)
			{
				continue;
			}
		}

		const FVector Difference = TargetCameraLocation - Constraint.M_PlaneOrigin;
		const float SignedDistance = FVector::DotProduct(Difference, Constraint.M_PlaneNormal);
		if (Constraint.bM_AllowPositiveSide && SignedDistance >= -PlaneSignedDistanceTolerance)
		{
			continue;
		}
		if (not Constraint.bM_AllowPositiveSide && SignedDistance <= PlaneSignedDistanceTolerance)
		{
			continue;
		}

		return false;
	}
	return true;
}

bool UWorldCameraController::GetIsBoundaryIdValidForRegistration(
	const FName& BoundaryId,
	const FString& CallingFunctionName) const
{
	if (BoundaryId != NAME_None)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Boundary id must be set.\n See function: UWorldCameraController::" + CallingFunctionName + "()");
	return false;
}

bool UWorldCameraController::GetCanBuildBoundaryFromActor(
	const AActor* BoundaryActor,
	const FString& CallingFunctionName) const
{
	if (IsValid(BoundaryActor))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Boundary actor is not valid.\n See function: UWorldCameraController::" + CallingFunctionName + "()");
	return false;
}

bool UWorldCameraController::GetHasSideFlag(const int32 BlockedSideFlags, const ECameraBoundaryBlockedSides SideFlag) const
{
	const int32 SideFlagValue = static_cast<int32>(SideFlag);
	return (BlockedSideFlags & SideFlagValue) != 0;
}

bool UWorldCameraController::GetHasAnyBlockedSideFlags(const int32 BlockedSideFlags) const
{
	return GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveX)
		|| GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeX)
		|| GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveY)
		|| GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeY);
}

void UWorldCameraController::AddPlaneConstraint(
	const FVector& PlaneOrigin,
	const FVector& PlaneNormal,
	const bool bAllowPositiveSide,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	FPlayerCameraBoundaryPlaneConstraint NewConstraint = {};
	NewConstraint.M_PlaneOrigin = PlaneOrigin;
	NewConstraint.M_PlaneNormal = PlaneNormal.GetSafeNormal();
	NewConstraint.bM_AllowPositiveSide = bAllowPositiveSide;
	OutConstraints.Add(NewConstraint);
}

void UWorldCameraController::AddDirectionalSideConstraintWithSpan(
	const float PlaneAxisValue,
	const FVector& PlaneNormal,
	const bool bAllowPositiveSide,
	const FVector& SpanAxis,
	const float SpanAxisMin,
	const float SpanAxisMax,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	FPlayerCameraBoundaryPlaneConstraint NewConstraint = {};
	NewConstraint.M_PlaneOrigin = PlaneNormal * PlaneAxisValue;
	NewConstraint.M_PlaneNormal = PlaneNormal.GetSafeNormal();
	NewConstraint.bM_AllowPositiveSide = bAllowPositiveSide;
	NewConstraint.bM_HasSpanLimit = true;
	NewConstraint.M_SpanAxis = SpanAxis.GetSafeNormal();
	NewConstraint.M_SpanAxisMin = SpanAxisMin;
	NewConstraint.M_SpanAxisMax = SpanAxisMax;
	OutConstraints.Add(NewConstraint);
}

void UWorldCameraController::AddBoxBoundaryConstraints(
	const FBox& WorldSpaceBox,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	if (not WorldSpaceBox.IsValid)
	{
		return;
	}

	AddPlaneConstraint(FVector(WorldSpaceBox.Min.X, 0.0f, 0.0f), FVector::ForwardVector, true, OutConstraints);
	AddPlaneConstraint(FVector(WorldSpaceBox.Max.X, 0.0f, 0.0f), FVector::ForwardVector, false, OutConstraints);
	AddPlaneConstraint(FVector(0.0f, WorldSpaceBox.Min.Y, 0.0f), FVector::RightVector, true, OutConstraints);
	AddPlaneConstraint(FVector(0.0f, WorldSpaceBox.Max.Y, 0.0f), FVector::RightVector, false, OutConstraints);
}

void UWorldCameraController::AddDirectionalSideConstraints(
	const AActor* BoundaryActor,
	const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
	const int32 BlockedSideFlags,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	if (not GetHasAnyBlockedSideFlags(BlockedSideFlags))
	{
		return;
	}

	FVector AxisX = FVector::ZeroVector;
	FVector AxisY = FVector::ZeroVector;
	float MinX = 0.0f;
	float MaxX = 0.0f;
	float MinY = 0.0f;
	float MaxY = 0.0f;
	if (not TryGetBoundaryAxisExtents(BoundaryActor, AxisSpaceForBlockedSides, AxisX, AxisY, MinX, MaxX, MinY, MaxY))
	{
		return;
	}

	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveX))
	{
		AddDirectionalSideConstraintWithSpan(MaxX, AxisX, false, AxisY, MinY, MaxY, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeX))
	{
		AddDirectionalSideConstraintWithSpan(MinX, AxisX, true, AxisY, MinY, MaxY, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveY))
	{
		AddDirectionalSideConstraintWithSpan(MaxY, AxisY, false, AxisX, MinX, MaxX, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeY))
	{
		AddDirectionalSideConstraintWithSpan(MinY, AxisY, true, AxisX, MinX, MaxX, OutConstraints);
	}
}

bool UWorldCameraController::TryGetBoundaryAxisExtents(
	const AActor* BoundaryActor,
	const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
	FVector& OutAxisX,
	FVector& OutAxisY,
	float& OutMinX,
	float& OutMaxX,
	float& OutMinY,
	float& OutMaxY) const
{
	const bool bUseActorLocalAxis = AxisSpaceForBlockedSides == ECameraBoundaryAxisSpace::ActorLocal;
	OutAxisX = bUseActorLocalAxis ? BoundaryActor->GetActorForwardVector() : FVector::ForwardVector;
	OutAxisY = bUseActorLocalAxis ? BoundaryActor->GetActorRightVector() : FVector::RightVector;
	OutAxisX = FVector(OutAxisX.X, OutAxisX.Y, 0.0f).GetSafeNormal();
	OutAxisY = FVector(OutAxisY.X, OutAxisY.Y, 0.0f).GetSafeNormal();
	if (OutAxisX.IsNearlyZero() || OutAxisY.IsNearlyZero())
	{
		RTSFunctionLibrary::ReportError("Cannot build world camera boundary because a projected axis was zero."
			"\n See function: UWorldCameraController::TryGetBoundaryAxisExtents()");
		return false;
	}

	const FBox WorldSpaceActorBounds = BoundaryActor->GetComponentsBoundingBox(true);
	if (not WorldSpaceActorBounds.IsValid)
	{
		RTSFunctionLibrary::ReportError("Cannot build world camera boundary because actor bounds were invalid."
			"\n See function: UWorldCameraController::TryGetBoundaryAxisExtents()");
		return false;
	}

	const FVector MinPoint = WorldSpaceActorBounds.Min;
	const FVector MaxPoint = WorldSpaceActorBounds.Max;
	const TArray<FVector> BoxCorners =
	{
		FVector(MinPoint.X, MinPoint.Y, 0.0f),
		FVector(MinPoint.X, MaxPoint.Y, 0.0f),
		FVector(MaxPoint.X, MinPoint.Y, 0.0f),
		FVector(MaxPoint.X, MaxPoint.Y, 0.0f)
	};
	OutMinX = TNumericLimits<float>::Max();
	OutMaxX = TNumericLimits<float>::Lowest();
	OutMinY = TNumericLimits<float>::Max();
	OutMaxY = TNumericLimits<float>::Lowest();
	for (const FVector& Corner : BoxCorners)
	{
		const float AxisXValue = FVector::DotProduct(Corner, OutAxisX);
		const float AxisYValue = FVector::DotProduct(Corner, OutAxisY);
		OutMinX = FMath::Min(OutMinX, AxisXValue);
		OutMaxX = FMath::Max(OutMaxX, AxisXValue);
		OutMinY = FMath::Min(OutMinY, AxisYValue);
		OutMaxY = FMath::Max(OutMaxY, AxisYValue);
	}
	return true;
}

void UWorldCameraController::RebuildCachedAdditionalPlaneConstraints()
{
	M_CachedAdditionalPlaneConstraints.Empty();
	for (const FPlayerCameraBoundarySubmissionCache& Submission : M_AdditionalBoundarySubmissionCaches)
	{
		M_CachedAdditionalPlaneConstraints.Append(Submission.M_CachedPlaneConstraints);
	}
}

void UWorldCameraController::TryMoveCameraByWorldDelta(const FVector& WorldDelta) const
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	FVector HorizontalWorldDelta = WorldDelta;
	HorizontalWorldDelta.Z = 0.0f;
	const FVector TargetLocation = CameraCarrier->GetActorLocation() + HorizontalWorldDelta;
	const FVector ConstrainedLocation = GetConstrainedCameraLocation(TargetLocation);
	const FVector ConstrainedDelta = ConstrainedLocation - CameraCarrier->GetActorLocation();
	CameraCarrier->AddActorWorldOffset(ConstrainedDelta, true);
}

void UWorldCameraController::TryMoveCameraToLocation(const FVector& TargetCameraLocation) const
{
	if (not GetIsValidCameraCarrierActor())
	{
		return;
	}

	AActor* const CameraCarrier = M_CameraCarrierActor.Get();
	FVector BoundaryCheckLocation = TargetCameraLocation;
	BoundaryCheckLocation.Z = CameraCarrier->GetActorLocation().Z;
	const FVector ConstrainedBoundaryLocation = GetConstrainedCameraLocation(BoundaryCheckLocation);
	FVector ConstrainedTargetLocation = TargetCameraLocation;
	ConstrainedTargetLocation.X = ConstrainedBoundaryLocation.X;
	ConstrainedTargetLocation.Y = ConstrainedBoundaryLocation.Y;
	CameraCarrier->SetActorLocation(ConstrainedTargetLocation, true);
}

void UWorldCameraController::PlayOptionalMoveSound(USoundBase* MoveSound) const
{
	if (not IsValid(MoveSound))
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, MoveSound);
}

bool UWorldCameraController::GetCanStartCameraOvertake(
	const FCameraOvertakeSettings& CameraOvertakeSettings) const
{
	if (CameraOvertakeSettings.Points.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Camera overtake needs at least one point.")
			TEXT("\n See function: UWorldCameraController::StartCameraOvertake()"));
		return false;
	}

	return GetIsValidCameraCarrierActor();
}

void UWorldCameraController::CacheCameraOvertakeDurations(
	const FCameraOvertakeSettings& CameraOvertakeSettings)
{
	M_CameraOvertakeState = {};
	M_CameraOvertakeState.M_Points = CameraOvertakeSettings.Points;
	M_CameraOvertakeState.M_TimeGetToFirstPoint = FMath::Max(CameraOvertakeSettings.TimeGetToFirstPoint, 0.0f);
	M_CameraOvertakeState.M_SequentialSegmentDurations.Reset();
	constexpr int32 FirstSequentialPointIndex = 1;
	const int32 SequentialSegmentCount = FMath::Max(
		CameraOvertakeSettings.Points.Num() - FirstSequentialPointIndex,
		0);
	M_CameraOvertakeState.M_SequentialSegmentDurations.SetNum(SequentialSegmentCount);
	if (SequentialSegmentCount <= 0)
	{
		return;
	}

	float TotalSequentialDistance = 0.0f;
	constexpr int32 PreviousPointOffset = 1;
	for (int32 PointIndex = FirstSequentialPointIndex; PointIndex < CameraOvertakeSettings.Points.Num(); ++PointIndex)
	{
		TotalSequentialDistance += FVector::Distance(
			CameraOvertakeSettings.Points[PointIndex - PreviousPointOffset],
			CameraOvertakeSettings.Points[PointIndex]);
	}

	if (TotalSequentialDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float TotalSequentialMovementTime = FMath::Max(
		CameraOvertakeSettings.TotalTimeMovementSequentialPoints,
		0.0f);
	for (int32 PointIndex = FirstSequentialPointIndex; PointIndex < CameraOvertakeSettings.Points.Num(); ++PointIndex)
	{
		const float SegmentDistance = FVector::Distance(
			CameraOvertakeSettings.Points[PointIndex - PreviousPointOffset],
			CameraOvertakeSettings.Points[PointIndex]);
		const int32 SegmentDurationIndex = PointIndex - FirstSequentialPointIndex;
		M_CameraOvertakeState.M_SequentialSegmentDurations[SegmentDurationIndex] =
			TotalSequentialMovementTime * SegmentDistance / TotalSequentialDistance;
	}
}

float UWorldCameraController::GetCameraOvertakeSequentialSegmentDuration(const int32 TargetPointIndex) const
{
	constexpr int32 FirstSequentialPointIndex = 1;
	const int32 SegmentDurationIndex = TargetPointIndex - FirstSequentialPointIndex;
	if (not M_CameraOvertakeState.M_SequentialSegmentDurations.IsValidIndex(SegmentDurationIndex))
	{
		return 0.0f;
	}

	return M_CameraOvertakeState.M_SequentialSegmentDurations[SegmentDurationIndex];
}

void UWorldCameraController::StartCameraOvertakeSegment(const int32 TargetPointIndex)
{
	if (not M_CameraOvertakeState.M_Points.IsValidIndex(TargetPointIndex))
	{
		FinishCameraOvertake();
		return;
	}

	M_CameraOvertakeState.M_TargetPointIndex = TargetPointIndex;
	M_CameraOvertakeState.M_SegmentStartLocation = GetCurrentCameraLocation();
	M_CameraOvertakeState.M_SegmentTargetLocation = M_CameraOvertakeState.M_Points[TargetPointIndex];
	M_CameraOvertakeState.M_SegmentElapsedTime = 0.0f;
	constexpr int32 FirstPointIndex = 0;
	M_CameraOvertakeState.M_SegmentTotalTime = TargetPointIndex == FirstPointIndex
		? M_CameraOvertakeState.M_TimeGetToFirstPoint
		: GetCameraOvertakeSequentialSegmentDuration(TargetPointIndex);
}

void UWorldCameraController::CompleteCameraOvertakeSegment()
{
	while (M_CameraOvertakeState.bM_IsActive)
	{
		TryMoveCameraToLocation(M_CameraOvertakeState.M_SegmentTargetLocation);
		M_CameraOvertakeState.M_LastReachedLocation = GetCurrentCameraLocation();
		const int32 ReachedPointIndex = M_CameraOvertakeState.M_TargetPointIndex;
		M_CameraOvertakeState.M_PointReachedDelegate.ExecuteIfBound(
			ReachedPointIndex,
			M_CameraOvertakeState.M_LastReachedLocation);

		constexpr int32 NextPointOffset = 1;
		const int32 NextPointIndex = ReachedPointIndex + NextPointOffset;
		if (not M_CameraOvertakeState.M_Points.IsValidIndex(NextPointIndex))
		{
			FinishCameraOvertake();
			return;
		}

		StartCameraOvertakeSegment(NextPointIndex);
		if (M_CameraOvertakeState.M_SegmentTotalTime > 0.0f)
		{
			return;
		}
	}
}

void UWorldCameraController::FinishCameraOvertake()
{
	if (not M_CameraOvertakeState.bM_IsActive)
	{
		return;
	}

	const FVector LastReachedLocation = M_CameraOvertakeState.M_LastReachedLocation;
	FWorldCameraOvertakeFinishedNativeDelegate FinishedDelegate =
		M_CameraOvertakeState.M_FinishedDelegate;
	M_CameraOvertakeState = {};
	FinishedDelegate.ExecuteIfBound(LastReachedLocation);
}

void UWorldCameraController::CancelActiveCameraMove()
{
	M_MoveState = {};
	bM_IsSafetyMoveToPlayerHQActive = false;
}
