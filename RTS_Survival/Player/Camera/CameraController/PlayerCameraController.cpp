// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "PlayerCameraController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/Player/Camera/CameraPawn.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPlayerCameraController::UPlayerCameraController(): M_PlayerCamera(nullptr), M_SpringArmComponent(nullptr),
                                                    M_EdgeScrollAccelX(1),
                                                    M_EdgeScrollAccelY(1),
                                                    M_CameraMovementSpeedMultiplier(RTSGameUserSettingsRanges::DefaultCameraMovementSpeedMultiplier),
                                                    bM_IsPlayerInTechTreeOrArchive(false),
                                                    M_BaseCameraZoomLevel(0),
                                                    CurrentMove(), MoveStartLocation()
{
	EdgeScrollSpeedX = 0.0f;
	EdgeScrollSpeedY = 0.0f;
	bM_IsCameraMovementDisabled = false;
	bM_IsCameraLocked = false;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
}

void UPlayerCameraController::SetCameraMovementSpeedMultiplier(const float NewMultiplier)
{
	M_CameraMovementSpeedMultiplier = FMath::Clamp(
		NewMultiplier,
		RTSGameUserSettingsRanges::MinCameraMovementSpeedMultiplier,
		RTSGameUserSettingsRanges::MaxCameraMovementSpeedMultiplier
	);
}

void UPlayerCameraController::ZoomIn()
{
	if (bM_IsPlayerInTechTreeOrArchive || not GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
	{
		return;
	}
	float NewLength = M_SpringArmComponent->TargetArmLength;
	NewLength -= DeveloperSettings::UIUX::ZoomSpeed;
	M_SpringArmComponent->TargetArmLength = FMath::Clamp(NewLength, DeveloperSettings::UIUX::MinZoomLimit,
	                                                     DeveloperSettings::UIUX::MaxZoomLimit);
	if constexpr (DeveloperSettings::Debugging::GCamera_Player_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Camera Zoom In: " + FString::SanitizeFloat(M_SpringArmComponent->TargetArmLength));
	}
}

void UPlayerCameraController::ZoomOut()
{
	if (bM_IsPlayerInTechTreeOrArchive || not GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
	{
		return;
	}
	float NewLength = M_SpringArmComponent->TargetArmLength;
	NewLength += DeveloperSettings::UIUX::ZoomSpeed;
	M_SpringArmComponent->TargetArmLength = FMath::Clamp(NewLength, DeveloperSettings::UIUX::MinZoomLimit,
	                                                     DeveloperSettings::UIUX::MaxZoomLimit);
	if constexpr (DeveloperSettings::Debugging::GCamera_Player_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Camera Zoom Out: " + FString::SanitizeFloat(M_SpringArmComponent->TargetArmLength));
	}
}

void UPlayerCameraController::InitPlayerCameraController(ACameraPawn* NewCameraRef,
                                                         USpringArmComponent* NewSpringarmRef)
{
	M_PlayerCamera = NewCameraRef;
	M_SpringArmComponent = NewSpringarmRef;
	if (not GetIsValidSpringArmComponent())
	{
		return;
	}

	M_SpringArmComponent->PrimaryComponentTick.bCanEverTick = true;
	M_SpringArmComponent->PrimaryComponentTick.bTickEvenWhenPaused = true;
	M_SpringArmComponent->PrimaryComponentTick.bStartWithTickEnabled = true;
	M_BaseCameraZoomLevel = M_SpringArmComponent->TargetArmLength;
}

void UPlayerCameraController::SetPlayableAreaBounds(const FVector& NewPlayableAreaCenter, const float NewPlayableAreaExtent)
{
	SetFowBoundaryConstraints(NewPlayableAreaCenter, NewPlayableAreaExtent);
}

bool UPlayerCameraController::RegisterAdditionalCameraBoundary(
	const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams)
{
	if (not GetIsBoundaryIdValidForRegistration(BoundaryRegistrationParams.M_BoundaryId,
	                                            "RegisterAdditionalCameraBoundary"))
	{
		return false;
	}
	if (not GetCanBuildBoundaryFromActor(BoundaryRegistrationParams.M_BoundaryActor,
	                                     "RegisterAdditionalCameraBoundary"))
	{
		return false;
	}

	const AActor* const BoundaryActor = BoundaryRegistrationParams.M_BoundaryActor.Get();
	TArray<FPlayerCameraBoundaryPlaneConstraint> CachedPlaneConstraints = {};
	if (BoundaryRegistrationParams.bM_UseActorBoundsAsBoundary)
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
		RTSFunctionLibrary::ReportError(
			"No camera boundary constraints were generated for boundary id: "
			+ BoundaryRegistrationParams.M_BoundaryId.ToString()
			+ "\n See function: UPlayerCameraController::RegisterAdditionalCameraBoundary()");
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

bool UPlayerCameraController::RemoveAdditionalCameraBoundaryById(const FName BoundaryId)
{
	if (not GetIsBoundaryIdValidForRegistration(BoundaryId, "RemoveAdditionalCameraBoundaryById"))
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

void UPlayerCameraController::RemoveAllAdditionalCameraBounds()
{
	M_AdditionalBoundarySubmissionCaches.Empty();
	M_CachedAdditionalPlaneConstraints.Empty();
}

void UPlayerCameraController::ResetCameraToBaseZoomLevel() const
{
	if (not GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
	{
		return;
	}
	M_SpringArmComponent->TargetArmLength = M_BaseCameraZoomLevel;
}

void UPlayerCameraController::SetCustomCameraZoomLevel(const float NewZoomLevel) const
{
	if (not GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
	{
		return;
	}
	M_SpringArmComponent->TargetArmLength = NewZoomLevel;
}

void UPlayerCameraController::EdgeScroll(const float DeltaTime)
{
	// Early‐return and reset *only* the untouched axis
	if (bM_IsPlayerInTechTreeOrArchive
		|| GetIsLockedOrDisabled()
		|| not GetIsValidCameraPawn())
	{
		M_EdgeScrollAccelX = 1.0f;
		M_EdgeScrollAccelY = 1.0f;
		return;
	}

	// Fetch normalized mouse position in [0,1]
	float ScreenX, ScreenY, MouseX, MouseY;
	bool bIsOnScreen = false;
	GetViewportSizeAndMouse(ScreenX, ScreenY, MouseX, MouseY, bIsOnScreen);


	if (not bIsOnScreen)
	{
		M_EdgeScrollAccelX = 1.0f;
		M_EdgeScrollAccelY = 1.0f;
		return;
	}
	const float NormX = (ScreenX > 0.0f) ? (MouseX / ScreenX) : 0.0f;
	const float NormY = (ScreenY > 0.0f) ? (MouseY / ScreenY) : 0.0f;

	// Time & accel constants
	constexpr float AccelRate = 25.0f; // accel per second
	constexpr float MaxAccelMul = 250.0f; // top multiplier
	constexpr float SoftZone = 0.30f; // 30% blend start
	constexpr float HardZone = 0.025f; // 2.5% hard edge
	const float ZoneSpan = SoftZone - HardZone;
	bool bIsInHardZone = false;
	bool bDirXHardZone = false;
	bool bDirYHardZone = false;
	FString Direction = "null";

	// X axis (left/right) 
	int32 DirX = 0;
	float StrX = 0.0f;
	if (NormX <= HardZone)
	{
		DirX = -1;
		StrX = 1.0f;
		bIsInHardZone = true;
		bDirXHardZone = true;
		Direction = "left";
	}
	else if (NormX <= SoftZone)
	{
		DirX = -1;
		StrX = FMath::Clamp((SoftZone - NormX) / ZoneSpan, 0.0f, 1.0f);
		Direction = "soft left";
	}
	else if (NormX >= 1.0f - HardZone)
	{
		DirX = 1;
		StrX = 1.0f;
		bIsInHardZone = true;
		bDirXHardZone = true;
		Direction = "right";
	}
	else if (NormX >= 1.0f - SoftZone)
	{
		DirX = 1;
		StrX = FMath::Clamp((NormX - (1.0f - SoftZone)) / ZoneSpan, 0.0f, 1.0f);
		Direction = "soft right";
	}

	M_EdgeScrollAccelX = (DirX != 0)
		                     ? FMath::Clamp(M_EdgeScrollAccelX + AccelRate * DeltaTime, 1.0f, MaxAccelMul)
		                     : 1.0f;

	// Y axis (up/down)
	int32 DirY = 0;
	float StrY = 0.0f;
	if (NormY <= HardZone)
	{
		DirY = 1;
		StrY = 1.0f; // top
		bIsInHardZone = true;
		bDirYHardZone = true;
		Direction += " top";
	}
	else if (NormY <= SoftZone)
	{
		DirY = 1;
		StrY = FMath::Clamp((SoftZone - NormY) / ZoneSpan, 0.0f, 1.0f);
		Direction += " soft top";
	}
	else if (NormY >= 1.0f - HardZone)
	{
		DirY = -1;
		StrY = 1.0f; // bottom
		bIsInHardZone = true;
		bDirYHardZone = true;
		Direction += " bottom";
	}
	else if (NormY >= 1.0f - SoftZone)
	{
		DirY = -1;
		StrY = FMath::Clamp((NormY - (1.0f - SoftZone)) / ZoneSpan, 0.0f, 1.0f);
		Direction += " soft bottom";
	}

	M_EdgeScrollAccelY = (DirY != 0)
		                     ? FMath::Clamp(M_EdgeScrollAccelY + AccelRate * DeltaTime, 1.0f, MaxAccelMul)
		                     : 1.0f;
	if (not bIsInHardZone)
	{
		if constexpr (DeveloperSettings::Debugging::GCamera_Player_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("NO hardzone", FColor::Red);
		}
		M_EdgeScrollAccelX = 1.0f;
		M_EdgeScrollAccelY = 1.0f;
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GCamera_Player_Compile_DebugSymbols)
	{
		const FString DirXHardzone = bDirXHardZone ? "Hard" : "";
		const FString DirYHardzone = bDirYHardZone ? "Hard" : "";

		const FString SignX = DirX > 0 ? "Positive" : "Negative";
		const FString SignY = DirY > 0 ? "Positive" : "Negative";

		RTSFunctionLibrary::PrintString("Moving in " + Direction +
		                                "\n With StrenghX: " + FString::SanitizeFloat(StrX) + " --> " + SignX +
		                                "\n With StrenghY: " + FString::SanitizeFloat(StrY) + " --> " + SignY
		                                , FColor::Blue);

		RTSFunctionLibrary::PrintString(
			"\n NormX: " + FString::SanitizeFloat(NormX) + " " + DirXHardzone +
			"\n NormY: " + FString::SanitizeFloat(NormY) + " " + DirYHardzone
			, FColor::White);
	}

	// Build local‐space pan delta (X=forward, Y=right)
	const float BaseSpeed = DeveloperSettings::UIUX::ModifierCameraMovementSpeed * M_CameraMovementSpeedMultiplier * 230.0f;
	const FVector LocalDelta(
		DirY * BaseSpeed * M_EdgeScrollAccelY * StrY * DeltaTime,
		DirX * BaseSpeed * M_EdgeScrollAccelX * StrX * DeltaTime,
		0.0f
	);

	// Rotate by camera yaw so “up” on screen follows the viewport
	const FRotator CameraYawOnlyRotation = FRotator(0.0f, M_PlayerCamera->GetActorRotation().Yaw, 0.0f);
	const FVector WorldDelta = CameraYawOnlyRotation.RotateVector(LocalDelta);

	TryMoveCameraByWorldDelta(WorldDelta);
}


bool UPlayerCameraController::GetIsValidCameraPawn() const
{
	if (IsValid(M_PlayerCamera))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("M_PlayerCamera is not valid in UPlayerCameraController");
	return false;
}

bool UPlayerCameraController::GetIsValidSpringArmComponent() const
{
	if (IsValid(M_SpringArmComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("M_SpringArmComponent is not valid in UPlayerCameraController");
	return false;
}

void UPlayerCameraController::GetViewportSizeAndMouse(
	float& OutScreenX,
	float& OutScreenY,
	float& OutMouseX,
	float& OutMouseY,
	bool& bOutIsMouseInViewport) const
{
	// Ensure we have a valid viewport
	if (!GEngine || !GEngine->GameViewport)
	{
		OutScreenX = OutScreenY = OutMouseX = OutMouseY = 0.0f;
		bOutIsMouseInViewport = false;
		return;
	}

	// 1) Get the raw viewport size (in “logical” pixels) :contentReference[oaicite:0]{index=0}
	FVector2D ViewSize = UWidgetLayoutLibrary::GetViewportSize(GetWorld());
	OutScreenX = ViewSize.X;
	OutScreenY = ViewSize.Y;

	// 2) Get the DPI / UI scale factor 
	const float ViewScale = UWidgetLayoutLibrary::GetViewportScale(this);

	// 3) Get the raw mouse position (in widget-local space) 
	const FVector2D RawMousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetWorld());

	// 4) Scale it to match the viewport’s logical pixels :
	const FVector2D ScaledMousePos = RawMousePos * ViewScale;

	OutMouseX = ScaledMousePos.X;
	OutMouseY = ScaledMousePos.Y;

	// 5) Determine if within [0..Size] bounds 
	bOutIsMouseInViewport =
		(OutMouseX >= 0.0f && OutMouseX <= OutScreenX) &&
		(OutMouseY >= 0.0f && OutMouseY <= OutScreenY);
}


void UPlayerCameraController::ForwardRightMovement(const bool bOnForward, float AxisX, const bool bOnRight, float AxisY)
{
	if (bM_IsPlayerInTechTreeOrArchive || GetIsLockedOrDisabled() || not GetIsValidCameraPawn())
	{
		return;
	}

	FVector LocalDelta = FVector::ZeroVector;
	if (bOnForward)
	{
		AxisX = AxisX * DeveloperSettings::UIUX::DefaultCameraMovementSpeed;
		AxisX = AxisX * DeveloperSettings::UIUX::ModifierCameraMovementSpeed * M_CameraMovementSpeedMultiplier;
		LocalDelta.X += AxisX;
	}

	if (bOnRight)
	{
		AxisY = AxisY * DeveloperSettings::UIUX::DefaultCameraMovementSpeed;
		AxisY = AxisY * DeveloperSettings::UIUX::ModifierCameraMovementSpeed * M_CameraMovementSpeedMultiplier;
		LocalDelta.Y += AxisY;
	}

	if (LocalDelta.IsNearlyZero())
	{
		return;
	}

	const FRotator CameraYawOnlyRotation = FRotator(0.0f, M_PlayerCamera->GetActorRotation().Yaw, 0.0f);
	const FVector WorldDelta = CameraYawOnlyRotation.RotateVector(LocalDelta);
	TryMoveCameraByWorldDelta(WorldDelta);
}

void UPlayerCameraController::PanReset()
{
	if (GetIsLockedOrDisabled() || bM_IsPlayerInTechTreeOrArchive || not GetIsValidCameraPawn())
	{
		return;
	}
	FRotator Rotation = M_PlayerCamera->GetActorRotation();
	Rotation.Pitch = 0;
	Rotation.Yaw = 0;
	M_PlayerCamera->SetActorRotation(Rotation);
}

void UPlayerCameraController::MoveCameraOverTime(const FMovePlayerCamera& NewMove)
{
	if (not GetIsValidCameraPawn() || not M_PlayerCamera->GetWorld())
	{
		return;
	}

	// Make a local copy so we can adjust the times.
	FMovePlayerCamera AdjustedMove = NewMove;
	// If TimeCameraInputDisabled is nonzero, ensure it is at least as long as TimeToMove.
	if (AdjustedMove.TimeCameraInputDisabled > 0.0f && AdjustedMove.TimeCameraInputDisabled < AdjustedMove.TimeToMove)
	{
		AdjustedMove.TimeCameraInputDisabled = AdjustedMove.TimeToMove;
	}
	// If a move is already in progress, add this move to the queue.
	if (bIsCameraMoving)
	{
		CameraMoveQueue.Add(AdjustedMove);
	}
	else
	{
		// Start the move immediately.
		UGameplayStatics::PlaySound2D(this, AdjustedMove.MoveSound, 1.0f,
		                              1.0f, 0.0f);
		bIsCameraMoving = true;
		CurrentMove = AdjustedMove;
		MoveStartLocation = M_PlayerCamera->GetActorLocation();
		MoveStartTime = M_PlayerCamera->GetWorld()->GetTimeSeconds();
		// The move duration is the max of TimeToMove and TimeCameraInputDisabled.
		MoveDuration = FMath::Max(CurrentMove.TimeToMove, CurrentMove.TimeCameraInputDisabled);
		// Start the timer; when it completes, consider the move finished.
		M_PlayerCamera->GetWorld()->GetTimerManager().SetTimer(MoveCameraTimerHandle, this,
		                                                       &UPlayerCameraController::OnCameraMoveComplete,
		                                                       MoveDuration,
		                                                       false);
	}
}

void UPlayerCameraController::OnCameraMoveComplete()
{
	// Ensure the camera is exactly at the target location.
	if (GetIsValidCameraPawn())
	{
		TryMoveCameraToLocation(CurrentMove.MoveToLocation);
	}
	bIsCameraMoving = false;
	// If there is a queued move, start it.
	if (CameraMoveQueue.Num() > 0)
	{
		FMovePlayerCamera NextMove = CameraMoveQueue[0];
		CameraMoveQueue.RemoveAt(0);
		MoveCameraOverTime(NextMove);
	}
}

void UPlayerCameraController::PostInitProperties()
{
	Super::PostInitProperties();
	SetComponentTickEnabled(true);
}

void UPlayerCameraController::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Process camera movement over time.
	if (bIsCameraMoving && GetIsValidCameraPawn())
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - MoveStartTime;
		float Alpha = (MoveDuration > 0.0f) ? FMath::Clamp(ElapsedTime / MoveDuration, 0.0f, 1.0f) : 1.0f;
		FVector NewLocation = FMath::Lerp(MoveStartLocation, CurrentMove.MoveToLocation, Alpha);
		TryMoveCameraToLocation(NewLocation);
	}
	EdgeScroll(DeltaTime);
}

bool UPlayerCameraController::GetIsLockedOrDisabled() const
{
	return bM_IsCameraLocked || bM_IsCameraMovementDisabled;
}

bool UPlayerCameraController::GetCanMoveCameraByWorldDelta(const FVector& WorldDelta) const
{
	if (not GetIsValidCameraPawn())
	{
		return false;
	}

	const FVector TargetCameraLocation = M_PlayerCamera->GetActorLocation() + WorldDelta;
	return GetCanMoveCameraToLocation(TargetCameraLocation);
}

bool UPlayerCameraController::GetCanMoveCameraToLocation(const FVector& TargetCameraLocation) const
{
	if (bM_HasFowPlayableAreaBounds
		&& not GetAreAllPlaneConstraintsSatisfied(TargetCameraLocation, M_FowPlaneConstraints))
	{
		return false;
	}

	return GetAreAllPlaneConstraintsSatisfied(TargetCameraLocation, M_CachedAdditionalPlaneConstraints);
}

bool UPlayerCameraController::GetAreAllPlaneConstraintsSatisfied(
	const FVector& TargetCameraLocation,
	const TArray<FPlayerCameraBoundaryPlaneConstraint>& PlaneConstraints) const
{
	constexpr float PlaneSignedDistanceTolerance = 0.1f;
	for (const FPlayerCameraBoundaryPlaneConstraint& Constraint : PlaneConstraints)
	{
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

bool UPlayerCameraController::GetIsBoundaryIdValidForRegistration(
	const FName& BoundaryId,
	const FString& CallingFunctionName) const
{
	if (BoundaryId != NAME_None)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Boundary id must be set."
		"\n See function: UPlayerCameraController::" + CallingFunctionName + "()");
	return false;
}

bool UPlayerCameraController::GetCanBuildBoundaryFromActor(
	const AActor* BoundaryActor,
	const FString& CallingFunctionName) const
{
	if (not IsValid(BoundaryActor))
	{
		RTSFunctionLibrary::ReportError("Boundary actor is not valid."
			"\n See function: UPlayerCameraController::" + CallingFunctionName + "()");
		return false;
	}
	return true;
}

bool UPlayerCameraController::GetHasSideFlag(
	const int32 BlockedSideFlags,
	const ECameraBoundaryBlockedSides SideFlag) const
{
	const int32 SideFlagValue = static_cast<int32>(SideFlag);
	return (BlockedSideFlags & SideFlagValue) != 0;
}

void UPlayerCameraController::SetFowBoundaryConstraints(
	const FVector& NewPlayableAreaCenter,
	const float NewPlayableAreaExtent)
{
	M_FowPlaneConstraints.Empty();
	bM_HasFowPlayableAreaBounds = NewPlayableAreaExtent > 0.0f;
	if (not bM_HasFowPlayableAreaBounds)
	{
		return;
	}

	const FVector LeftBoundaryPoint = NewPlayableAreaCenter + FVector(-NewPlayableAreaExtent, 0.0f, 0.0f);
	const FVector RightBoundaryPoint = NewPlayableAreaCenter + FVector(NewPlayableAreaExtent, 0.0f, 0.0f);
	const FVector BottomBoundaryPoint = NewPlayableAreaCenter + FVector(0.0f, -NewPlayableAreaExtent, 0.0f);
	const FVector TopBoundaryPoint = NewPlayableAreaCenter + FVector(0.0f, NewPlayableAreaExtent, 0.0f);

	AddPlaneConstraint(LeftBoundaryPoint, FVector::ForwardVector, true, M_FowPlaneConstraints);
	AddPlaneConstraint(RightBoundaryPoint, FVector::ForwardVector, false, M_FowPlaneConstraints);
	AddPlaneConstraint(BottomBoundaryPoint, FVector::RightVector, true, M_FowPlaneConstraints);
	AddPlaneConstraint(TopBoundaryPoint, FVector::RightVector, false, M_FowPlaneConstraints);
}

void UPlayerCameraController::AddPlaneConstraint(
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

void UPlayerCameraController::AddBoxBoundaryConstraints(
	const FBox& WorldSpaceBox,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	if (not WorldSpaceBox.IsValid)
	{
		return;
	}

	const FVector MinPoint = WorldSpaceBox.Min;
	const FVector MaxPoint = WorldSpaceBox.Max;

	const FVector LeftBoundaryPoint = FVector(MinPoint.X, 0.0f, 0.0f);
	const FVector RightBoundaryPoint = FVector(MaxPoint.X, 0.0f, 0.0f);
	const FVector BottomBoundaryPoint = FVector(0.0f, MinPoint.Y, 0.0f);
	const FVector TopBoundaryPoint = FVector(0.0f, MaxPoint.Y, 0.0f);

	AddPlaneConstraint(LeftBoundaryPoint, FVector::ForwardVector, true, OutConstraints);
	AddPlaneConstraint(RightBoundaryPoint, FVector::ForwardVector, false, OutConstraints);
	AddPlaneConstraint(BottomBoundaryPoint, FVector::RightVector, true, OutConstraints);
	AddPlaneConstraint(TopBoundaryPoint, FVector::RightVector, false, OutConstraints);
}

void UPlayerCameraController::AddDirectionalSideConstraints(
	const AActor* BoundaryActor,
	const ECameraBoundaryAxisSpace AxisSpaceForBlockedSides,
	const int32 BlockedSideFlags,
	TArray<FPlayerCameraBoundaryPlaneConstraint>& OutConstraints) const
{
	if (not GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveX)
		&& not GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeX)
		&& not GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveY)
		&& not GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeY))
	{
		return;
	}

	const FVector Origin = BoundaryActor->GetActorLocation();
	const bool bUseActorLocalAxis = AxisSpaceForBlockedSides == ECameraBoundaryAxisSpace::ActorLocal;
	const FVector AxisX = bUseActorLocalAxis ? BoundaryActor->GetActorForwardVector() : FVector::ForwardVector;
	const FVector AxisY = bUseActorLocalAxis ? BoundaryActor->GetActorRightVector() : FVector::RightVector;
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveX))
	{
		AddPlaneConstraint(Origin, AxisX, false, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeX))
	{
		AddPlaneConstraint(Origin, AxisX, true, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::PositiveY))
	{
		AddPlaneConstraint(Origin, AxisY, false, OutConstraints);
	}
	if (GetHasSideFlag(BlockedSideFlags, ECameraBoundaryBlockedSides::NegativeY))
	{
		AddPlaneConstraint(Origin, AxisY, true, OutConstraints);
	}
}

void UPlayerCameraController::RebuildCachedAdditionalPlaneConstraints()
{
	M_CachedAdditionalPlaneConstraints.Empty();
	for (const FPlayerCameraBoundarySubmissionCache& Submission : M_AdditionalBoundarySubmissionCaches)
	{
		M_CachedAdditionalPlaneConstraints.Append(Submission.M_CachedPlaneConstraints);
	}
}

void UPlayerCameraController::TryMoveCameraByWorldDelta(const FVector& WorldDelta) const
{
	if (not GetIsValidCameraPawn())
	{
		return;
	}

	FVector HorizontalWorldDelta = WorldDelta;
	HorizontalWorldDelta.Z = 0.0f;
	if (not GetCanMoveCameraByWorldDelta(HorizontalWorldDelta))
	{
		return;
	}

	M_PlayerCamera->AddActorWorldOffset(HorizontalWorldDelta, true);
}

void UPlayerCameraController::TryMoveCameraToLocation(const FVector& TargetCameraLocation) const
{
	if (not GetIsValidCameraPawn())
	{
		return;
	}

	FVector HorizontalTargetLocation = TargetCameraLocation;
	HorizontalTargetLocation.Z = M_PlayerCamera->GetActorLocation().Z;
	if (not GetCanMoveCameraToLocation(HorizontalTargetLocation))
	{
		return;
	}

	M_PlayerCamera->SetActorLocation(HorizontalTargetLocation, true);
}
