#include "FRTSInstanceHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FRTSInstanceHelpers::SetupHierarchicalInstanceBox(
	UInstancedStaticMeshComponent* InstStaticMesh,
	FVector2D BoxBounds,
	const float DistanceBetweenInstances,
	AActor* RequestingActor)
{
	// Validate the instanced mesh component.
	if (!IsValid(InstStaticMesh) || not IsValid(RequestingActor))
	{
		FString IstMeshName = IsValid(InstStaticMesh) ? InstStaticMesh->GetName() : "null";
		FString RequestingActorName = IsValid(RequestingActor) ? RequestingActor->GetName() : "null";
		RTSFunctionLibrary::ReportError("No valid instanced static mesh component or invalid actor for setting up the "
			"hierarchical instance box : "
			"\n Actor making request: " + RequestingActorName +
			"\n Instanced Static Mesh Component: " + IstMeshName);
		return;
	}

	InstStaticMesh->SetVisibility(true);
	InstStaticMesh->SetComponentTickEnabled(false);
	InstStaticMesh->CastShadow = 0;

	// Retrieve the actor's rotation and location.
	const FRotator ActorRot = RequestingActor->GetActorRotation();
	const FVector ActorLoc = RequestingActor->GetActorLocation();

	// Compute the local starting position (top left corner, assuming the PopulateMesh faces the X direction).
	const FVector LocalStart = 0.5f * FVector(BoxBounds.X, -BoxBounds.Y, 0.f);
	const FVector WorldStart = ActorLoc + ActorRot.RotateVector(LocalStart);
	RTSFunctionLibrary::PrintToLog("Start Location: " + WorldStart.ToString());

	// Determine the number of instances along each border.
	const int32 AmountOnYDirection = FMath::FloorToInt(BoxBounds.Y / DistanceBetweenInstances);
	const int32 AmountOnXDirection = FMath::FloorToInt(BoxBounds.X / DistanceBetweenInstances);

	float LastUsedYOffset = 0.f, LastUsedXOffset = 0.f;

	// --- Top Border: move along positive local Y from LocalStart ---
	for (int32 Y = 0; Y < AmountOnYDirection; ++Y)
	{
		const float YOffset = Y * DistanceBetweenInstances;
		LastUsedYOffset = YOffset;
		// Calculate local and then world position.
		const FVector LocalPos = LocalStart + FVector(0, YOffset, 0);
		const FVector WorldPos = ActorLoc + ActorRot.RotateVector(LocalPos);
		// Instance rotation is adjusted relative to the actor's rotation.
		const FRotator InstanceRot = ActorRot + FRotator(0, 90, 0);
		InstStaticMesh->AddInstance(FTransform(InstanceRot, WorldPos, FVector(1.f)), true);
	}

	// --- Top Right Corner ---
	const FVector LocalTopRight = LocalStart + FVector(0, LastUsedYOffset + DistanceBetweenInstances, 0);
	const FVector WorldTopRight = ActorLoc + ActorRot.RotateVector(LocalTopRight);
	RTSFunctionLibrary::PrintToLog("Box vertex top right: " + WorldTopRight.ToString());

	// --- Right Border: move along negative local X from Top Right ---
	for (int32 X = 0; X < AmountOnXDirection; ++X)
	{
		const float XOffset = -X * DistanceBetweenInstances;
		LastUsedXOffset = XOffset;
		const FVector LocalPos = LocalTopRight + FVector(XOffset, 0, 0);
		const FVector WorldPos = ActorLoc + ActorRot.RotateVector(LocalPos);
		const FRotator InstanceRot = ActorRot + FRotator(0, 180, 0);
		InstStaticMesh->AddInstance(FTransform(InstanceRot, WorldPos, FVector(1.f)), true);
	}

	// --- Bottom Right Corner ---
	const FVector LocalBottomRight = LocalStart + FVector(LastUsedXOffset - DistanceBetweenInstances,
	                                                      LastUsedYOffset + DistanceBetweenInstances, 0);
	const FVector WorldBottomRight = ActorLoc + ActorRot.RotateVector(LocalBottomRight);
	RTSFunctionLibrary::PrintToLog("box vertex bottom right: " + WorldBottomRight.ToString());

	// --- Bottom Border: move along negative local Y from Bottom Right ---
	for (int32 Y = 0; Y < AmountOnYDirection; ++Y)
	{
		const float YOffset = -Y * DistanceBetweenInstances;
		LastUsedYOffset = YOffset;
		const FVector LocalPos = LocalBottomRight + FVector(0, YOffset, 0);
		const FVector WorldPos = ActorLoc + ActorRot.RotateVector(LocalPos);
		const FRotator InstanceRot = ActorRot + FRotator(0, -90, 0);
		InstStaticMesh->AddInstance(FTransform(InstanceRot, WorldPos, FVector(1.f)), true);
	}

	// --- Bottom Left Corner ---
	const FVector LocalBottomLeft = LocalBottomRight + FVector(0, LastUsedYOffset - DistanceBetweenInstances, 0);
	const FVector WorldBottomLeft = ActorLoc + ActorRot.RotateVector(LocalBottomLeft);
	RTSFunctionLibrary::PrintToLog("box vertex bottom left: " + WorldBottomLeft.ToString());

	// --- Left Border: move along positive local X from Bottom Left ---
	for (int32 X = 0; X < AmountOnXDirection; ++X)
	{
		const float XOffset = X * DistanceBetweenInstances;
		const FVector LocalPos = LocalBottomLeft + FVector(XOffset, 0, 0);
		const FVector WorldPos = ActorLoc + ActorRot.RotateVector(LocalPos);
		// Left border instances follow the actor's rotation.
		const FRotator InstanceRot = ActorRot;
		InstStaticMesh->AddInstance(FTransform(InstanceRot, WorldPos, FVector(1.f)), true);
	}

	// Update the render state.
	InstStaticMesh->MarkRenderStateDirty();
}

void FRTSInstanceHelpers::SetupHierarchicalInstanceAlongRoad(const FRTSRoadInstanceSetup RoadSetup,
                                                             UInstancedStaticMeshComponent* InstStaticMesh,
                                                             const AActor* RequestingActor)
{
	if (not EnsureInstanceAlongRoadRequestIsValid(InstStaticMesh, RequestingActor))
	{
		return;
	}
	const FVector ActorLoc = RequestingActor->GetActorLocation();
	const FRotator ActorRot = RequestingActor->GetActorRotation();
	const FVector ActorScale = RequestingActor->GetActorScale3D();
	const FVector ActorForward = ActorRot.Vector();
	const FVector ActorRight = RequestingActor->GetActorRightVector();

	const int32 AmountAlongDirection = FMath::FloorToInt(
		RoadSetup.DistanceForwardBackward / RoadSetup.DistanceBetweenRoadParts);
	// Create forward direction instances.
	for (int32 i = 0; i < AmountAlongDirection; ++i)
	{
		const float Distance = i * RoadSetup.DistanceBetweenRoadParts;
		const FVector PartLocation = ActorLoc + Distance * ActorForward;
		CreateRoadInstancesWithOffset(RoadSetup, InstStaticMesh, PartLocation, ActorRot, ActorScale, ActorRight, RequestingActor);
	}
	// Create backward direction instances.
	for (int32 i = 1; i < AmountAlongDirection; ++i)
	{
		const float Distance = i * RoadSetup.DistanceBetweenRoadParts;
		const FVector PartLocation = ActorLoc - Distance * ActorForward;
		CreateRoadInstancesWithOffset(RoadSetup, InstStaticMesh, PartLocation, ActorRot, ActorScale, ActorRight, RequestingActor);
	}
}

bool FRTSInstanceHelpers::EnsureInstanceAlongRoadRequestIsValid(
	const UInstancedStaticMeshComponent* InstStaticMesh,
	const AActor* RequestingActor)
{
	if (not IsValid(RequestingActor) || not IsValid(InstStaticMesh))
	{
		const FString ActorName = IsValid(RequestingActor) ? RequestingActor->GetName() : "null";
		const FString InstMeshName = IsValid(InstStaticMesh) ? InstStaticMesh->GetName() : "null";
		RTSFunctionLibrary::ReportError("Invalid request for instance along road setup: "
			"\n Actor: " + ActorName +
			"\n Instanced Mesh: " + InstMeshName);
		return false;
	}
	return true;
}

void FRTSInstanceHelpers::CreateRoadInstancesWithOffset(const FRTSRoadInstanceSetup& RoadSetup,
                                                        UInstancedStaticMeshComponent* InstStaticMesh,
                                                        const FVector& PartLocation, FRotator Rotation,
                                                        const FVector& Scale, const FVector& RightVector, const AActor* RequestingActor)
{
	bool bPlaceLeft = false, bPlaceRight = false;
	if (RoadSetup.bIsTwoSided)
	{
		bPlaceLeft = true;
		bPlaceRight = true;
	}
	else
	{
		bPlaceLeft = RoadSetup.bIsLeftSided;
		bPlaceRight = not RoadSetup.bIsLeftSided;
	}
	FTransform PlacementTransform;
	PlacementTransform.SetRotation(Rotation.Quaternion());
	PlacementTransform.SetScale3D(Scale);
	FVector Location;
	if (bPlaceLeft)
	{
		FRotator LeftRotator = Rotation;
		DetermineRandomRollValue(RoadSetup, LeftRotator);
		DetermineRandomPitchValue(RoadSetup, LeftRotator);
		DetermineRandomYawValue(RoadSetup, LeftRotator);
		Location = PartLocation + (-RightVector) * RoadSetup.DistanceBetweenInstancesAtPart;
		Location.Z = VisibilityTraceForZOnLandscape(Location, InstStaticMesh->GetWorld(), RequestingActor->GetActorLocation().Z);
		PlacementTransform.SetLocation(Location);
		PlacementTransform.SetRotation(LeftRotator.Quaternion());
		InstStaticMesh->AddInstance(PlacementTransform, true);
	}
	if (bPlaceRight)
	{
		FRotator RightRotator = Rotation;
		DetermineRandomRollValue(RoadSetup, RightRotator);
		DetermineRandomPitchValue(RoadSetup, RightRotator);
		DetermineRandomYawValue(RoadSetup, RightRotator);
		Location = PartLocation + RightVector * RoadSetup.DistanceBetweenInstancesAtPart;
		Location.Z = VisibilityTraceForZOnLandscape(Location, InstStaticMesh->GetWorld(), RequestingActor->GetActorLocation().Z);
		PlacementTransform.SetLocation(Location);
		PlacementTransform.SetRotation(RightRotator.Quaternion());
		InstStaticMesh->AddInstance(PlacementTransform, true);
	}
}

void FRTSInstanceHelpers::DetermineRandomRollValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation)
{
	if (RoadSetup.RandomRollRotationValues.IsEmpty())
	{
		// See if there is a range to pick from.
		if (RoadSetup.NegPosRandomRollRange == 0)
		{
			return;
		}
		const float RandomRoll = FMath::RandRange(-RoadSetup.NegPosRandomRollRange, RoadSetup.NegPosRandomRollRange);
		OutRotation.Roll += RandomRoll;
		return;
	}
	// Pick random value from array.
	const int32 Index = FMath::RandRange(0, RoadSetup.RandomRollRotationValues.Num() - 1);
	OutRotation.Roll = RoadSetup.RandomRollRotationValues[Index];
}

void FRTSInstanceHelpers::DetermineRandomPitchValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation)
{
	if (RoadSetup.RandomPitchRotationValues.IsEmpty())
	{
		// See if there is a range to pick from.
		if (RoadSetup.NegPosRandomPitchRange == 0)
		{
			return;
		}
		const float RandomPitch = FMath::RandRange(-RoadSetup.NegPosRandomPitchRange, RoadSetup.NegPosRandomPitchRange);
		OutRotation.Pitch += RandomPitch;
		return;
	}
	// Pick random value from array.
	const int32 Index = FMath::RandRange(0, RoadSetup.RandomPitchRotationValues.Num() - 1);
	OutRotation.Pitch = RoadSetup.RandomPitchRotationValues[Index];
}

void FRTSInstanceHelpers::DetermineRandomYawValue(const FRTSRoadInstanceSetup& RoadSetup, FRotator& OutRotation)
{
	if (RoadSetup.RandomYawRotationValues.IsEmpty())
	{
		// See if there is a range to pick from.
		if (RoadSetup.NegPosRandomYawRange == 0)
		{
			return;
		}
		const float RandomYaw = FMath::RandRange(-RoadSetup.NegPosRandomYawRange, RoadSetup.NegPosRandomYawRange);
		OutRotation.Yaw += RandomYaw;
		return;
	}
	// Pick random value from array.
	const int32 Index = FMath::RandRange(0, RoadSetup.RandomYawRotationValues.Num() - 1);
	OutRotation.Yaw = RoadSetup.RandomYawRotationValues[Index];
}

float FRTSInstanceHelpers::VisibilityTraceForZOnLandscape(const FVector& StartLocation, const UWorld* World, const float ZBackupValue)
{
	if (not IsValid(World))
	{
		return ZBackupValue;
	}
	FHitResult Hit;
	const FVector EndLocation = StartLocation + FVector(0, 0, -5000);
	FCollisionQueryParams TraceParams;
	TraceParams.bTraceComplex = true;
	// Perform trace.
	World->LineTraceSingleByChannel(Hit, StartLocation, EndLocation, ECollisionChannel::ECC_Visibility, TraceParams);
	if (Hit.bBlockingHit)
	{
		return Hit.Location.Z;
	}
	return ZBackupValue;
}
