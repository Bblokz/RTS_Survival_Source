// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "PlayerCameraController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
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
	if (bM_IsPlayerInTechTreeOrArchive || !GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
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
	if (bM_IsPlayerInTechTreeOrArchive || !GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
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
	if (M_SpringArmComponent)
	{
		M_SpringArmComponent->PrimaryComponentTick.bCanEverTick = true;
		M_SpringArmComponent->PrimaryComponentTick.bTickEvenWhenPaused = true;
		M_SpringArmComponent->PrimaryComponentTick.bStartWithTickEnabled = true;
	}
	M_BaseCameraZoomLevel = M_SpringArmComponent->TargetArmLength;
}

void UPlayerCameraController::ResetCameraToBaseZoomLevel() const
{
	if (!GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
	{
		return;
	}
	M_SpringArmComponent->TargetArmLength = M_BaseCameraZoomLevel;
}

void UPlayerCameraController::SetCustomCameraZoomLevel(const float NewZoomLevel) const
{
	if (!GetIsValidSpringArmComponent() || GetIsLockedOrDisabled())
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
	const FRotator CamRot = M_PlayerCamera->GetActorRotation();
	const FVector WorldDelta = CamRot.RotateVector(LocalDelta);

	M_PlayerCamera->AddActorWorldOffset(WorldDelta, /*bSweep=*/ true);
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
	if (bOnForward)
	{
		AxisX = AxisX * DeveloperSettings::UIUX::DefaultCameraMovementSpeed;
		AxisX = AxisX * DeveloperSettings::UIUX::ModifierCameraMovementSpeed * M_CameraMovementSpeedMultiplier;

		FVector DirectionVector;
		DirectionVector.X = AxisX;
		DirectionVector.Y = 0;
		DirectionVector.Z = 0;

		FTransform Transform = M_PlayerCamera->GetActorTransform();
		FVector LocationVector = UKismetMathLibrary::TransformDirection(Transform, DirectionVector);
		Transform = M_PlayerCamera->GetActorTransform();
		LocationVector += Transform.GetLocation();
		LocationVector.Z = 110; // Set Z to height of map.
		M_PlayerCamera->SetActorLocation(LocationVector, true);
	}
	if (bOnRight)
	{
		AxisY = AxisY * DeveloperSettings::UIUX::DefaultCameraMovementSpeed;
		AxisY = AxisY * DeveloperSettings::UIUX::ModifierCameraMovementSpeed * M_CameraMovementSpeedMultiplier;

		FVector DirectionVector;
		DirectionVector.X = 0;
		DirectionVector.Y = AxisY;
		DirectionVector.Z = 0;

		FTransform Transform = M_PlayerCamera->GetActorTransform();
		FVector LocationVector = UKismetMathLibrary::TransformDirection(Transform, DirectionVector);
		Transform = M_PlayerCamera->GetActorTransform();
		LocationVector += Transform.GetLocation();

		M_PlayerCamera->SetActorLocation(LocationVector, true);
	}
}

void UPlayerCameraController::PanReset()
{
	if (GetIsLockedOrDisabled()|| bM_IsPlayerInTechTreeOrArchive || !GetIsValidCameraPawn())
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
	if (!GetIsValidCameraPawn() || not M_PlayerCamera->GetWorld())
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
		M_PlayerCamera->SetActorLocation(CurrentMove.MoveToLocation, true);
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
		M_PlayerCamera->SetActorLocation(NewLocation, true);
	}
	EdgeScroll(DeltaTime);
}

bool UPlayerCameraController::GetIsLockedOrDisabled() const
{
	return bM_IsCameraLocked || bM_IsCameraMovementDisabled;
}
