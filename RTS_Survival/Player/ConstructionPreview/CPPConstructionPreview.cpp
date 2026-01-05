// Copyright Bas Blokzijl - All rights reserved.


#include "CPPConstructionPreview.h"

#include "Blueprint/UserWidget.h"
#include "MoveUnitsFromConstruction/FMoveUnitsFromConstruction.h"
#include "NomadicPreviewAttachments/FNomadicPreviewAttachmentState.h"
#include "PreviewToGridHelpers/PreviewToGridHelpers.h"
#include "RTS_Survival/Player/ConstructionPreview/BuildingGridOverlay/BuildingGridOverlay.h"
#include "PreviewWidget/W_PreviewStats.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Player/ConstructionPreview/ConstructionRadiusHelper/ConstructionRadiusHelper.h"
#include "RTS_Survival/Player/Camera/CameraPawn.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
FName ACPPConstructionPreview::PreviewMeshComponentName(TEXT("PreviewMesh"));


ACPPConstructionPreview::ACPPConstructionPreview(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  M_ExpansionRadiusHelper(nullptr),
	  M_ActivePreviewStatus(EConstructionPreviewMode::Construct_None),
	  M_ConstructionPreviewMaterial(nullptr),
	  bM_IsValidCursorLocation(false),
	  bM_IsValidBuildingLocation(false),
	  M_SlopeAngle(0),
	  M_PreviewStatsWidget(nullptr),
	  M_PreviewStatsWidgetComponent(nullptr),
	  M_DynamicMaterialPool(),
	  M_BuildRadii(),
	  M_DistanceToClosestRadius(0.f),
	  M_MaXDistanceToClosestRadius(0.f),
	  bM_NeedWithinBuildRadius(false)
	  , M_SocketWidgetHeight(100.f)
{
	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;
	M_SocketToSnapTo = nullptr;

	// Initialize the preview mesh component
	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(PreviewMeshComponentName);
	if (IsValid(PreviewMesh))
	{
		PreviewMesh->BodyInstance.bSimulatePhysics = false;
		PreviewMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PreviewMesh->SetCollisionObjectType(ECollisionChannel::COLLISION_OBJ_BUILDING_PLACEMENT);
		PreviewMesh->SetCollisionResponseToChannel(
			ECollisionChannel::COLLISION_OBJ_BUILDING_PLACEMENT, ECollisionResponse::ECR_Overlap);
		PreviewMesh->SetCollisionResponseToChannel(
			ECollisionChannel::COLLISION_OBJ_ENEMY, ECollisionResponse::ECR_Overlap);
		PreviewMesh->SetCollisionResponseToChannel(
			ECollisionChannel::COLLISION_TRACE_ENEMY, ECollisionResponse::ECR_Overlap);
		PreviewMesh->SetGenerateOverlapEvents(true);
		PreviewMesh->SetCanEverAffectNavigation(false);
		PreviewMesh->SetCastShadow(false);
		RootComponent = PreviewMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "PreviewMesh",
		                                             "ACPPConstructionPreview::ACPPConstructionPreview");
	}
	// Initialize rotation degrees
	RotationDegrees = 10.f;
}

FVector ACPPConstructionPreview::GetBxpPreviewLocation(const FVector& ClickedLocation) const
{
	switch (M_ActivePreviewStatus)
	{
	case EConstructionPreviewMode::Construct_None:
		RTSFunctionLibrary::ReportError("Requested bxp preview location but the construction mode is None"
			"\n See GetBxpPreviewLocation");
		return ClickedLocation;
	case EConstructionPreviewMode::Construct_NomadicPreview:
		RTSFunctionLibrary::ReportError("Requested bxp preview location but the construction mode Nomadic"
			"\n See GetBxpPreviewLocation");
		return ClickedLocation;
	case EConstructionPreviewMode::Construct_BxpFree:
		return ClickedLocation;
	// Falls through.
	case EConstructionPreviewMode::Construct_BxpSocket:
	case EConstructionPreviewMode::Construct_BxpOrigin:
		return GetActorLocation();
	}
	return ClickedLocation;
}

FName ACPPConstructionPreview::GetBxpSocketName() const
{
	if (M_ActivePreviewStatus == EConstructionPreviewMode::Construct_BxpSocket)
	{
		if (IsValid(M_SocketToSnapTo))
		{
			return M_SocketToSnapTo->SocketName;
		}
		RTSFunctionLibrary::ReportError("M_SocketToSnapTo is null in GetBxpSocketName in CPPConstructionPreview.cpp"
			"\n Actor: " + GetName());
		return NAME_None;
	}
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString(
			"Requested Bxp Socket for placement to save in the construction data"
			"but the bxp is not of type socket so we return NAME_None"));
	}
	return NAME_None;
}


// Called when the game starts or when spawned
void ACPPConstructionPreview::BeginPlay()
{
	Super::BeginPlay();
	if (IsValid(ExpansionRadiusHelperClass))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		M_ExpansionRadiusHelper = GetWorld()->SpawnActor<AConstructionRadiusHelper>(ExpansionRadiusHelperClass,
			SpawnParams);
		if (!IsValid(M_ExpansionRadiusHelper))
		{
			RTSFunctionLibrary::ReportError(
				"Failed to spawn ExpansionRadiusHelper in BeginPlay in CPPConstructionPreview.cpp");
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("ExpansionRadiusHelperClass is null in BeginPlay in CPPConstructionPreview.cpp"
			"\n forgot to set class in defaults of: " + GetName());
	}
}

void ACPPConstructionPreview::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	DestroySocketWidgets();
}


FVector ACPPConstructionPreview::GetGridSnapAdjusted(
	const FVector& MouseWorldPosition, const float ExtraHeight) const
{
	float Snap = DeveloperSettings::GamePlay::Construction::GridSnapSize;

	if (EnsureIsValidGridOverlay())
	{
		// Default to CellSize
		Snap = M_GridOverlay->GetCellSize();

		// If we can, snap to actual center-to-center step (CellSize + Gap)
		FTransform T0, T1;
		if (M_GridOverlay->GetInstanceWorldTransform(0, T0) &&
			M_GridOverlay->GetInstanceWorldTransform(1, T1))
		{
			const float StepX = FMath::Abs(T1.GetLocation().X - T0.GetLocation().X);
			if (StepX > KINDA_SMALL_NUMBER)
			{
				Snap = StepX;
			}
		}
	}

	const float X = FMath::RoundToFloat(MouseWorldPosition.X / Snap) * Snap;
	const float Y = FMath::RoundToFloat(MouseWorldPosition.Y / Snap) * Snap;
	const float Z = MouseWorldPosition.Z + ExtraHeight;
	return FVector(X, Y, Z);
}


FVector ACPPConstructionPreview::GetSnapToClosestSocketLocation(
	const UStaticMeshComponent* SocketMeshComp,
	const TArray<UStaticMeshSocket*>& SocketList,
	const FVector& InMouseWorldPosition,
	bool& bOutValidPosition,
	UStaticMeshSocket*& OutSocket,
	FRotator& OutSocketRotation) const
{
	// Start out invalid
	bOutValidPosition = false;

	// Nothing to snap to?
	if (SocketList.Num() == 0 || !IsValid(SocketMeshComp))
	{
		return FVector::ZeroVector;
	}

	float BestDistSq = TNumericLimits<float>::Max();
	FVector BestLocation = FVector::ZeroVector;
	UStaticMeshSocket* PrevSocket = OutSocket;

	for (UStaticMeshSocket* Socket : SocketList)
	{
		if (!IsValid(Socket))
		{
			RTSFunctionLibrary::PrintString(
				"Invalid socket found in GetSnapToClosestSocketLocation for " + SocketMeshComp->GetName());
			continue;
		}

		// Grab the socket’s world‐space position
		FVector SocketWorldPos = SocketMeshComp->GetSocketLocation(Socket->SocketName);

		if (SocketWorldPos.IsNearlyZero())
		{
			RTSFunctionLibrary::PrintString(
				"Socket position is nearly zero for socket: " + Socket->SocketName.ToString() + " in "
				+ SocketMeshComp->GetName() + ". Please check the socket setup.");
			continue;
		}

		// Compare squared distances (faster than Dist)
		const float DistSq = FVector::DistSquared(InMouseWorldPosition, SocketWorldPos);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestLocation = SocketWorldPos;
			OutSocket = Socket;
			OutSocketRotation = SocketMeshComp->GetSocketRotation(Socket->SocketName);
			bOutValidPosition = true;
		}
	}
	if (OutSocket && PrevSocket != OutSocket)
	{
		M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::Socket);
	}

	// If we never found a valid socket, bOutValidPosition stays false and we return zero
	return BestLocation;
}

FVector ACPPConstructionPreview::GetSnapToBuildingOriginLocation(const UStaticMeshComponent* SocketMeshComp,
                                                                 bool& bOutValidPosition) const
{
	if (not IsValid(SocketMeshComp))
	{
		RTSFunctionLibrary::PrintString("Invalid socket mesh component cannot snap to building origin!"
			"\n at function GetSnapToBuildingOriginLocation in CPPConstructionPreview.cpp"
			"\n Actor: " + GetName());
		bOutValidPosition = false;
		return FVector::ZeroVector;
	}
	bOutValidPosition = true;
	return SocketMeshComp->GetComponentLocation();
}


bool ACPPConstructionPreview::GetIsValidExpansionRadiusHelper() const
{
	if (IsValid(M_ExpansionRadiusHelper))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this,
	                                             "M_ExpansionRadiusHelper",
	                                             "ACPPConstructionPreview::GetIsValidExpansionRadiusHelper");
	return false;
}

bool ACPPConstructionPreview::GetHasActivePreview() const
{
	return M_ActivePreviewStatus != EConstructionPreviewMode::Construct_None;
}


bool ACPPConstructionPreview::IsOverlayFootprintOverlapping() const
{
	if (not EnsureIsValidGridOverlay())
	{
		return false;
	}
	return M_GridOverlay->AreConstructionPreviewSquaresValid() == false;
}


void ACPPConstructionPreview::InitConstructionPreview(
	ACPPController* NewPlayerController,
	UMaterialInstance* ConstructionMaterial,
	UW_PreviewStats* PreviewStatsWidget,
	UWidgetComponent* PreviewStatsWidgetComponent,
	const FConstructionPreviewSounds NewPreviewSoundSettings,
	const TSubclassOf<UWSocketConstructionWidget> InSocketWidgetClass, TSubclassOf<ABuildingGridOverlay>
	InGridOverlayClass)
{
	SocketWidgetClass = InSocketWidgetClass;
	M_GridOverlayClass = InGridOverlayClass;
	if (!SocketWidgetClass)
	{
		RTSFunctionLibrary::ReportError(
			"SocketWidgetClass is null in InitConstructionPreview");
	}
	M_PreviewSounds = NewPreviewSoundSettings;
	if (IsValid(NewPlayerController))
	{
		PlayerController = NewPlayerController;
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Playercontroller is null in InitConstructionPreview in CPPConstructionPreview.cpp");
	}
	if (IsValid(ConstructionMaterial) && IsValid(PreviewStatsWidget) && IsValid(PreviewStatsWidgetComponent))
	{
		M_ConstructionPreviewMaterial = ConstructionMaterial;
		M_PreviewStatsWidget = PreviewStatsWidget;
		M_PreviewStatsWidget->SetVisibility(ESlateVisibility::Hidden);
		PreviewStatsWidget->InitW_PreviewStats();
		M_PreviewStatsWidgetComponent = PreviewStatsWidgetComponent;
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Obtianed NULL for either ConstructionMaterial, PreviewStatsWidget or PreviewStatsWidgetComponent"
			"\n at function InitConstructionPreview in CPPConstructionPreview.cpp"
			"\n Actor: " + GetName());
	}
	// Initialize the pool of dynamic material instances
	InitializeDynamicMaterialPool(ConstructionMaterial);
	CreateGridOverlay();
}


void ACPPConstructionPreview::Tick(float DetlaTime)
{
	Super::Tick(DetlaTime);
	bool bIsSlopeValid = false;
	if (not GetHasActivePreview())
	{
		return;
	}
	SetCursorPosition(
		PlayerController->GetCursorWorldPosition(DeveloperSettings::UIUX::SightDistanceMouse,
		                                         bM_IsValidCursorLocation));
	if (not bM_IsValidCursorLocation)
	{
		// Location outside of view.
		if (DeveloperSettings::Debugging::GConstruction_Preview_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("location outside of view", FColor::Red);
		}
		bM_IsValidBuildingLocation = false;
		// Update camera orientation but make clear that the position is not valid.
		UpdatePreviewStatsWidget(bIsSlopeValid);
		return;
	}
	switch (M_ActivePreviewStatus)
	{
	case EConstructionPreviewMode::Construct_None:
		break;
	case EConstructionPreviewMode::Construct_BxpFree:
		bM_IsValidBuildingLocation = TickRadiiBuildingPlacement(bIsSlopeValid);
		break;
	case EConstructionPreviewMode::Construct_BxpSocket:
		bM_IsValidBuildingLocation = TickSocketBxpBuildingPlacement(bIsSlopeValid);
		break;
	case EConstructionPreviewMode::Construct_BxpOrigin:
		bM_IsValidBuildingLocation = TickAtBuildingOriginBxpPlacement(bIsSlopeValid);
		break;
	case EConstructionPreviewMode::Construct_NomadicPreview:
		bM_IsValidBuildingLocation = TickRadiiBuildingPlacement(bIsSlopeValid);
		break;
	}
	// Update materials depending on whether we can place the preview.
	UpdatePreviewMaterial(bM_IsValidBuildingLocation);
	UpdatePreviewStatsWidget(bIsSlopeValid);
}


bool ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid(
	const UStaticMesh* NewPreviewMesh,
	const bool bNeedWithinBuildRadius,
	const FVector& HostLocation,
	const FBxpConstructionData& ConstructionData) const
{
	const FString ModeName = Global_GetBxpConstructionTypeEnumAsString(ConstructionData.GetConstructionType());
	if (not IsValid(NewPreviewMesh))
	{
		RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
			"\n as the provided preview mesh is null!"
			"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
			"\n Actor: " + GetName());
		return false;
	}
	switch (ConstructionData.GetConstructionType())
	{
	case EBxpConstructionType::None:
		RTSFunctionLibrary::ReportError("Cannot start bxp previw of type : " + ModeName +
			"\n as the provided construction type is NONE!"
			"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
			"\n Actor: " + GetName());
		return false;
	case EBxpConstructionType::Free:
		if (bNeedWithinBuildRadius)
		{
			if (HostLocation.IsNearlyZero())
			{
				RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
					"\n as the provided host location is nearly zero!"
					"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
					"\n Actor: " + GetName());
				return false;
			}
			if (ConstructionData.GetBxpBuildRadius() <= 0)
			{
				RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
					"\n as the provided BXP build radius is not greater than zero!"
					"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
					"\n Actor: " + GetName());
				return false;
			}
		}
		break;
	case EBxpConstructionType::Socket:
		if (ConstructionData.GetSocketList().IsEmpty())
		{
			RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
				"\n as the provided socket list is empty!"
				"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
				"\n Actor: " + GetName());
			return false;
		}
		if (not IsValid(ConstructionData.GetAttachToMesh()))
		{
			RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
				"\n as the provided attach to mesh is not valid!"
				"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
				"\n Actor: " + GetName());
			return false;
		}
		break;
	case EBxpConstructionType::AtBuildingOrigin:

		if (not IsValid(ConstructionData.GetAttachToMesh()))
		{
			RTSFunctionLibrary::ReportError("Cannot start bxp preview of type : " + ModeName +
				"\n as the provided attach to mesh is not valid!"
				"\n at function ACPPConstructionPreview::EnsureBxpPreviewRequestIsValid"
				"\n Actor: " + GetName());
			return false;
		}
		break;
	}
	return true;
}

bool ACPPConstructionPreview::TickRadiiBuildingPlacement(bool& bOutHasValidIncline)
{
	// todo ignore player actors?
	TArray<AActor*> ActorsToIgnore;
	// Move preview along grid.
	SetActorLocation(GetGridSnapAdjusted(CursorWorldPosition));
	UpdateOverlayGridOverlaps();

	// Check if the slope is valid for the current cursor position.
	// Makes sure the landscape blocks visibility traces!
	bOutHasValidIncline = IsSlopeValid(CursorWorldPosition);

	if (!bOutHasValidIncline || IsOverlayFootprintOverlapping() || !GetisWithinBuildRadii(CursorWorldPosition))
	{
		// Invalid location due to slope or overlap.
		if (DeveloperSettings::Debugging::GConstruction_Preview_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Cannot place building here", FColor::Red);
		}
		UpdatePreviewMaterial(false);
		// No valid placement
		return false;
	}
	// Valid location.
	UpdatePreviewMaterial(true);
	return true;
}

bool ACPPConstructionPreview::TickSocketBxpBuildingPlacement(bool& bOutHasValidIncline)
{
	const TArray<AActor*> ActorsToIgnore = {BxpConstructionData.GetBxpOwnerActor()};
	bool bIsValidLocation = false;
	// Disregard incline for socket attachment. Saves the snapping socket.
	bOutHasValidIncline = true;
	FRotator SocketRotation = FRotator::ZeroRotator;
	const FVector PlacementLocation = GetSnapToClosestSocketLocation(BxpConstructionData.GetAttachToMesh(),
	                                                                 BxpConstructionData.GetSocketList(),
	                                                                 CursorWorldPosition,
	                                                                 bIsValidLocation,
	                                                                 M_SocketToSnapTo,
	                                                                 SocketRotation);
	if (not bIsValidLocation)
	{
		RTSFunctionLibrary::PrintString("Cannot snap location", FColor::Red);
		return false;
	}
	SetActorLocation(PlacementLocation);
	SetActorRotation(SocketRotation);
	if (not BxpConstructionData.GetUseCollision())
	{
		return true;
	}
	// If no overlaps we can place the bxp.
	return not IsOverlayFootprintOverlapping();
}

bool ACPPConstructionPreview::TickAtBuildingOriginBxpPlacement(bool& bOutHasValidIncline)
{
	bool bIsValidLocation = false;
	// Disregard incline for socket attachment.
	bOutHasValidIncline = true;
	const FVector PlacementLocation = GetSnapToBuildingOriginLocation(BxpConstructionData.GetAttachToMesh(),
	                                                                  bIsValidLocation);
	if (not bIsValidLocation)
	{
		RTSFunctionLibrary::PrintString("Cannot snap location to building origin", FColor::Red);
		return false;
	}
	SetActorLocation(PlacementLocation);
	return true;
}


void ACPPConstructionPreview::SetCursorPosition(const FVector& CursorLocation)
{
	CursorWorldPosition = CursorLocation;
}

bool ACPPConstructionPreview::GetisWithinBuildRadii(const FVector& PreviewLocation)
{
	if (M_BuildRadii.Num() == 0)
	{
		if (!bM_NeedWithinBuildRadius)
		{
			// No build radii defined; allow placement anywhere as no need for buildradii.
			M_DistanceToClosestRadius = 0.0f;
			M_MaXDistanceToClosestRadius = 0.0f;
			return true;
		}
		return false;
	}
	bool bIsWithinAnyRadius = false;

	// For outside all radii
	float MinDistanceToPerimeter = TNumericLimits<float>::Max();
	float CorrespondingRadiusAtMinDistance = 0.0f;

	// For inside radii
	float MaxRadiusWithin = 0.0f;
	for (const URadiusComp* RadiusComp : M_BuildRadii)
	{
		if (IsValid(RadiusComp) && IsValid(RadiusComp->GetOwner()))
		{
			FVector HostLocation = RadiusComp->GetOwner()->GetActorLocation();
			const float Radius = RadiusComp->GetRadius();

			const float DistanceToCenter = FVector::Dist(PreviewLocation, HostLocation);

			if (DistanceToCenter <= Radius)
			{
				// We are inside this build radius
				if (Radius > MaxRadiusWithin)
				{
					// This is the largest radius we've found that we are within
					MaxRadiusWithin = Radius;

					// Update variables for display
					M_DistanceToClosestRadius = DistanceToCenter;
					M_MaXDistanceToClosestRadius = Radius;
				}
				bIsWithinAnyRadius = true;
			}
			else
			{
				// We are outside this radius
				// Calculate distance to perimeter
				float DistanceToPerimeter = DistanceToCenter - Radius;
				if (DistanceToPerimeter < MinDistanceToPerimeter)
				{
					// This is the closest perimeter so far
					MinDistanceToPerimeter = DistanceToPerimeter;
					CorrespondingRadiusAtMinDistance = Radius;
				}
			}
		}
	}

	if (bIsWithinAnyRadius)
	{
		// We are within at least one radius
		// M_DistanceToClosestRadius and M_MaxDistanceToClosestRadius have been updated
		return true;
	}
	// We are outside all radii
	// Save the minimal distance to the closest circle's perimeter
	M_DistanceToClosestRadius = MinDistanceToPerimeter + CorrespondingRadiusAtMinDistance;
	M_MaXDistanceToClosestRadius = CorrespondingRadiusAtMinDistance;
	return false;
}


bool ACPPConstructionPreview::GetIsBuildingPreviewBlocked() const
{
	return !bM_IsValidBuildingLocation;
}

AStaticPreviewMesh* ACPPConstructionPreview::CreateStaticMeshActor(const FRotator& Rotation, ANomadicVehicle* NomadicConstructing) const
{
	// Validate world first (early return).
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("World is null in CreateStaticMeshActor in CPPConstructionPreview.cpp"
			"\n actor name: " + GetName());
		return nullptr;
	}

	// Validate preview mesh next (early return).
	if (not IsValid(PreviewMesh))
	{
		RTSFunctionLibrary::ReportError("PreviewMesh is null in CreateStaticMeshActor in CPPConstructionPreview.cpp"
			"\n actor name: " + GetName());
		return nullptr;
	}

	// Create a static preview mesh actor and set the preview mesh of this actor.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticPreviewMesh* StaticPreviewMesh = World->SpawnActor<AStaticPreviewMesh>(
		CursorWorldPosition,
		Rotation,
		SpawnParams);

	if (not IsValid(StaticPreviewMesh))
	{
		return nullptr;
	}

	StaticPreviewMesh->SetPreviewMesh(PreviewMesh->GetStaticMesh(), M_ConstructionPreviewMaterial);

	// Move overlapping player-1 units out of the construction bounds.
	FMoveUnitsFromConstruction::MoveOverlappingUnitsForPlayer1AwayFromConstruction(
		this,             // world context for projection
		StaticPreviewMesh,
		Rotation, NomadicConstructing);        // use building rotation as "final facing" for units

	return StaticPreviewMesh;
}

void ACPPConstructionPreview::StartNomadicPreview(
	UStaticMesh* NewPreviewMesh, const TArray<URadiusComp*>& BuildRadii, const bool bNeedWithinBuildRadius, const
	FNomadicPreviewAttachments& PreviewAttachments)
{
	SetGridOverlayEnabled(true);
	M_NomadicPreviewAttachmentState.AttachBuildRadiusToPreview(this, PreviewAttachments.BuildingRadius,
	                                                           PreviewAttachments.AttachmentOffset);
	M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::StartPreview);
	bM_IsValidBuildingLocation = false;
	bM_NeedWithinBuildRadius = bNeedWithinBuildRadius;
	M_BuildRadii = BuildRadii;
	InitPreviewAndStatWidgetForConstruction(NewPreviewMesh, EConstructionPreviewMode::Construct_NomadicPreview
	                                        , true);
}

void ACPPConstructionPreview::StartBxpPreview(UStaticMesh* NewPreviewMesh,
                                              const bool bNeedWithinBuildRadius,
                                              const FVector& HostLocation,
                                              const FBxpConstructionData& ConstructionData)
{
	if (not EnsureBxpPreviewRequestIsValid(
		NewPreviewMesh,
		bNeedWithinBuildRadius,
		HostLocation,
		ConstructionData))
	{
		return;
	}
	M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::StartPreview);
	// before bxp build radius was Separated from the construction data but is now in that struct too.
	BxpConstructionData = ConstructionData;
	bM_IsValidBuildingLocation = false;
	bM_NeedWithinBuildRadius = bNeedWithinBuildRadius;
	switch (ConstructionData.GetConstructionType())
	{
	// Already checked in EnsureBxpPreviewRequestIsValid.
	case EBxpConstructionType::None:
		return;
	case EBxpConstructionType::Free:
		SetGridOverlayEnabled(true);
	// Moves the radius to the host and makes it visible and scales to the correct size.
		InitExpansionRadiusHelperForPlacement(HostLocation, BxpConstructionData.GetBxpBuildRadius());
		if (GetIsValidExpansionRadiusHelper())
		{
			if (URadiusComp* BxpRadiusComp = M_ExpansionRadiusHelper->GetRadiusComp())
			{
				M_BuildRadii.Empty();
				M_BuildRadii.Add(BxpRadiusComp);
			}
		}
		break;
	// Falls Through.
	case EBxpConstructionType::Socket:
	case EBxpConstructionType::AtBuildingOrigin:
		if (DeveloperSettings::Debugging::GConstruction_Preview_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(
				"Starting BXP preview with construction type: " + Global_GetBxpConstructionTypeEnumAsString(
					ConstructionData.GetConstructionType()));
		}
	}
	const EConstructionPreviewMode NewPreviewMode = Global_GetConstructionTypeFromBxpPreviewMode(
		ConstructionData.GetConstructionType());
	InitPreviewAndStatWidgetForConstruction(
		NewPreviewMesh,
		NewPreviewMode,
		ConstructionData.GetUseCollision());
	if (ConstructionData.GetConstructionType() == EBxpConstructionType::Socket)
	{
		// Use the grid for this bxp socket placement as it used collision.
		if (ConstructionData.GetUseCollision())
		{
			SetGridOverlayEnabled(true);
		}
		CreateSocketWidgets();
	}
}


void ACPPConstructionPreview::InitPreviewAndStatWidgetForConstruction(
	UStaticMesh* NewPreviewMesh,
	const EConstructionPreviewMode PreviewMode,
	const bool bPreviewUseCollision)
{
	if (not NewPreviewMesh)
	{
		RTSFunctionLibrary::ReportError("Attempt to start building preview with null mesh!"
			"\n at function InitPReviewAndStatsWidgetForConstruction in CPPConstructionPreview.cpp"
			"\n Actor: " + GetName());
		return;
	}

	PreviewMesh->SetStaticMesh(NewPreviewMesh);
	FRTS_CollisionSetup::SetupStaticBuildingPreviewCollision(PreviewMesh, bPreviewUseCollision);
	// This starts the tick function to check for valid placement.
	M_ActivePreviewStatus = PreviewMode;
	if (EnsureIsValidPreviewStatsWidget())
	{
		M_PreviewStatsWidget->SetVisibility(ESlateVisibility::Visible);
	}
	MoveWidgetToMeshHeight();

	// Apply a dynamic material from the pool to each material slot.
	for (int i = 0; i < PreviewMesh->GetNumMaterials(); ++i)
	{
		if (M_DynamicMaterialPool.IsValidIndex(i))
		{
			PreviewMesh->SetMaterial(i, M_DynamicMaterialPool[i]);
		}
	}
}

void ACPPConstructionPreview::InitExpansionRadiusHelperForPlacement(const FVector& HostLocation,
                                                                    const float BxpBuildRadius) const
{
	if (not GetIsValidExpansionRadiusHelper())
	{
		return;
	}

	M_ExpansionRadiusHelper->SetConstructionRadius(BxpBuildRadius);
	M_ExpansionRadiusHelper->SetActorLocation(HostLocation);
	M_ExpansionRadiusHelper->SetRadiusVisibility(true);
}

bool ACPPConstructionPreview::GetIsRotationAllowed() const
{
	bool bIsAllowed = false;
	switch (M_ActivePreviewStatus)
	{
	case EConstructionPreviewMode::Construct_None:
		break;
	case EConstructionPreviewMode::Construct_NomadicPreview:
		bIsAllowed = true;
		break;
	case EConstructionPreviewMode::Construct_BxpFree:
		bIsAllowed = true;
		break;
	case EConstructionPreviewMode::Construct_BxpSocket:
		break;
	case EConstructionPreviewMode::Construct_BxpOrigin:
		break;
	}
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"GetIsRotationAllowed for construction preview mode: " +
			Global_GetConstructionPreviewModeEnumAsString(M_ActivePreviewStatus) + " returns: " +
			FString(bIsAllowed ? "true" : "false"));
	}
	return bIsAllowed;
}

void ACPPConstructionPreview::CreateSocketWidgets()
{
	if (!SocketWidgetClass)
	{
		RTSFunctionLibrary::ReportError(
			"SocketWidgetClass is null in CreateSocketWidgets");
		return;
	}

	// clean up any old ones
	DestroySocketWidgets();

	if (!IsValid(BxpConstructionData.GetAttachToMesh()))
	{
		RTSFunctionLibrary::ReportError(
			"AttachToMesh is invalid in CreateSocketWidgets");
		return;
	}

	for (UStaticMeshSocket* Socket : BxpConstructionData.GetSocketList())
	{
		if (!IsValid(Socket))
		{
			RTSFunctionLibrary::PrintString(
				FString::Printf(TEXT("Skipping invalid socket in CreateSocketWidgets")));
			continue;
		}

		// new widget‐component
		UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(this);
		WidgetComp->RegisterComponent();

		WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WidgetComp->SetGenerateOverlapEvents(false);
		WidgetComp->SetCanEverAffectNavigation(false);

		UStaticMeshComponent* HostMesh = BxpConstructionData.GetAttachToMesh();
		if (IsValid(HostMesh))
		{
			WidgetComp->AttachToComponent(
				HostMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				Socket->SocketName
			);

			// only apply your height offset on the relative Z:
			WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, M_SocketWidgetHeight));
		}
		else
		{
			RTSFunctionLibrary::ReportError(
				"HostMesh is invalid in CreateSocketWidgets");
			return;
		}

		WidgetComp->SetWidgetClass(SocketWidgetClass);
		WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
		WidgetComp->SetDrawAtDesiredSize(true);

		// position at socket + height offset
		const FVector WorldPos =
			BxpConstructionData.GetAttachToMesh()
			                   ->GetSocketLocation(Socket->SocketName)
			+ FVector(0.f, 0.f, M_SocketWidgetHeight);

		WidgetComp->SetWorldLocation(WorldPos);
		SocketWidgetComponents.Add(WidgetComp);
	}
}


void ACPPConstructionPreview::DestroySocketWidgets()
{
	for (auto* WidgetComp : SocketWidgetComponents)
	{
		if (IsValid(WidgetComp))
		{
			WidgetComp->DestroyComponent();
		}
	}
	SocketWidgetComponents.Empty();
}

void ACPPConstructionPreview::CreateGridOverlay()
{
	if (!IsValid(M_GridOverlayClass))
	{
		RTSFunctionLibrary::ReportError(
			"M_GridOverlayClass is null in CreateGridOverlay in CPPConstructionPreview.cpp\n Actor: " + GetName());
		return;
	}

	M_GridOverlay = GetWorld()->SpawnActor<ABuildingGridOverlay>(M_GridOverlayClass);
	if (!EnsureIsValidGridOverlay())
	{
		return;
	}

	// Place it where the preview is so its grid anchor stays world-aligned as before.
	M_GridOverlay->SetActorLocation(GetActorLocation());

	// Ignore the preview itself when testing overlaps.
	TArray<AActor*> ActorsToIgnore = {this};
	M_GridOverlay->SetExtraOverlapActorsToIgnore(ActorsToIgnore);

	// Attach so it follows TRANSLATION only; do not inherit ROTATION or SCALE.
	// KeepWorldTransform prevents a snap/shift on attach.
	M_GridOverlay->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

	// Make rotation & scale absolute so parent rotation/scale no longer propagate.
	if (USceneComponent* OverlayRoot = M_GridOverlay->GetRootComponent())
	{
		// Location: relative (follows preview movement) = false
		// Rotation: absolute (does NOT rotate with preview) = true
		// Scale:    absolute (does NOT scale with preview)   = true
		OverlayRoot->SetAbsolute(/*bNewAbsoluteLocation=*/false,
		                                                  /*bNewAbsoluteRotation=*/true,
		                                                  /*bNewAbsoluteScale=*/true);
	}

	// Force a neutral world rotation so the grid stays axis-aligned.
	M_GridOverlay->SetActorRotation(FRotator::ZeroRotator);
}


void ACPPConstructionPreview::SetGridOverlayEnabled(const bool bEnabled) const
{
	if (not EnsureIsValidGridOverlay())
	{
		return;
	}
	if (bEnabled)
	{
		M_GridOverlay->SetActorHiddenInGame(false);
		M_GridOverlay->SetActorEnableCollision(true);
	}
	else
	{
		M_GridOverlay->SetActorHiddenInGame(true);
		M_GridOverlay->SetActorEnableCollision(false);
	}
}

bool ACPPConstructionPreview::EnsureIsValidGridOverlay() const
{
	if (not IsValid(M_GridOverlay))
	{
		return false;
	}
	return true;
}


void ACPPConstructionPreview::StopBuildingPreview(const bool bIsPlacedSuccessfully)
{
	DestroySocketWidgets();
	SetGridOverlayEnabled(false);
	M_NomadicPreviewAttachmentState.RemoveAttachmentRadius();
	if (IsValid(PreviewMesh) && IsValid(M_PreviewStatsWidget))
	{
		PreviewMesh->SetStaticMesh(nullptr);
		M_ActivePreviewStatus = EConstructionPreviewMode::Construct_None;
		// Reset rotation.
		PreviewMesh->SetWorldRotation(FRotator::ZeroRotator);
		M_PreviewStatsWidget->SetVisibility(ESlateVisibility::Hidden);
	}
	if (GetIsValidExpansionRadiusHelper())
	{
		M_ExpansionRadiusHelper->SetRadiusVisibility(false);
	}
	M_SocketToSnapTo = nullptr;
	if (bIsPlacedSuccessfully)
	{
		M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::Placement);
	}
}


void ACPPConstructionPreview::UpdatePreviewStatsWidget(const bool bIsInclineValid) const
{
	if (not EnsureIsValidPreviewStatsWidgetComponent() || not GetHasActivePreview())
	{
		return;
	}
	// RotatePreviewStatsToCamera();
	const float Degrees = PreviewMesh->GetComponentRotation().Yaw;

	const float Distance = M_DistanceToClosestRadius;
	const float MaxDistance = M_MaXDistanceToClosestRadius;
	const bool ShowDistance = M_BuildRadii.Num() > 0;
	const bool bNoValidBuildRadii = bM_NeedWithinBuildRadius && M_BuildRadii.Num() == 0;

	const bool ConfirmPlacementOnly = M_ActivePreviewStatus == EConstructionPreviewMode::Construct_BxpOrigin ||
		M_ActivePreviewStatus == EConstructionPreviewMode::Construct_BxpSocket;

	M_PreviewStatsWidget->UpdateInformation(
		Degrees,
		M_SlopeAngle,
		bIsInclineValid,
		ShowDistance,
		Distance,
		MaxDistance,
		bNoValidBuildRadii,
		ConfirmPlacementOnly
	);
}


void ACPPConstructionPreview::RotatePreviewStatsToCamera() const
{
	if (IsValid(PlayerController) && IsValid(PlayerController->CameraRef))
	{
		const FRotator NewRotation = UKismetMathLibrary::FindLookAtRotation(
			M_PreviewStatsWidgetComponent->GetComponentLocation(),
			PlayerController->CameraRef->GetCameraComponent()->GetComponentLocation());
		// Apply the calculated rotation to the widget component
		M_PreviewStatsWidgetComponent->SetWorldRotation(NewRotation);
	}
}


bool ACPPConstructionPreview::IsSlopeValid(const FVector& Location)
{
	if (not EnsureIsValidPreviewMesh())
	{
		return false;
	}
	TArray<FVector> TraceStartPoints;
	TArray<FName> SocketNames = {"FL", "FR", "RL", "RR"};
	bool bSocketsFound = true;

	// Check for sockets and add their locations to the trace points
	for (const FName& SocketName : SocketNames)
	{
		if (PreviewMesh->DoesSocketExist(SocketName))
		{
			TraceStartPoints.Add(PreviewMesh->GetSocketLocation(SocketName) + FVector(0.0f, 0.0f,
				DeveloperSettings::GamePlay::Construction::AddedHeightToTraceSlopeCheckPoint));
		}
		else
		{
			bSocketsFound = false;
			break;
		}
	}

	// The sockets are not found, we fall back to the box extend of the preview mesh.
	if (!bSocketsFound)
	{
		TraceStartPoints.Empty();
		FVector BoxExtent = PreviewMesh->GetStaticMesh()->GetBounds().GetBox().GetExtent();
		// Already adds pivot
		AddBoxExtentToTracePoints(TraceStartPoints, Location, BoxExtent);
	}
	else
	{
		// Add pivot location as the first trace point
		TraceStartPoints.Insert(Location + FVector(0.0f, 0.0f, 100.0f), 0);
	}

	bool bAllPointsValid = true;
	float SlopeAngle = 0;
	constexpr float EndHeightDifference = 2 *
		DeveloperSettings::GamePlay::Construction::AddedHeightToTraceSlopeCheckPoint;
	for (const FVector& StartPoint : TraceStartPoints)
	{
		FHitResult Hit;
		// Adjust height for added z above the original point.
		FVector EndPoint = StartPoint - FVector(0.0f, 0.0f, EndHeightDifference);

		if (GetWorld()->LineTraceSingleByChannel(Hit, StartPoint, EndPoint, ECC_Visibility))
		{
			SlopeAngle = FMath::RadiansToDegrees(acosf(FVector::DotProduct(Hit.Normal, FVector::UpVector)));
			if (DeveloperSettings::Debugging::GConstruction_Preview_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString(
					"Hit angle at (" + StartPoint.ToString() + "): " + FString::SanitizeFloat(SlopeAngle),
					FColor::Red);
			}

			if (SlopeAngle > DeveloperSettings::GamePlay::Construction::DegreesAllowedOnHill)
			{
				bAllPointsValid = false;
				M_SlopeAngle = SlopeAngle;
			}
		}
		else
		{
			// If any trace doesn't hit, consider it invalid
			if (DeveloperSettings::Debugging::GConstruction_Preview_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("No hit at trace point: " + StartPoint.ToString(), FColor::Red);
			}
			bAllPointsValid = false;
		}
	}

	if (bAllPointsValid)
	{
		// All valid; take the last slope as the comparison.
		M_SlopeAngle = SlopeAngle;
	}

	return bAllPointsValid;
}

void ACPPConstructionPreview::AddBoxExtentToTracePoints(
	TArray<FVector>& OutTracePoints,
	const FVector& Location,
	const FVector& BoxExtent) const
{
	constexpr float Height = DeveloperSettings::GamePlay::Construction::AddedHeightToTraceSlopeCheckPoint;
	// Add pivot location
	OutTracePoints.Add(Location + FVector(0.0f, 0.0f, Height));
	// Add the box extend points
	OutTracePoints.Add(Location + FVector(BoxExtent.X, BoxExtent.Y, Height));
	OutTracePoints.Add(Location + FVector(-BoxExtent.X, BoxExtent.Y, Height));
	OutTracePoints.Add(Location + FVector(BoxExtent.X, -BoxExtent.Y, Height));
	OutTracePoints.Add(Location + FVector(-BoxExtent.X, -BoxExtent.Y, Height));
}


void ACPPConstructionPreview::InitializeDynamicMaterialPool(UMaterialInstance* BaseMaterial)
{
	if (IsValid(BaseMaterial))
	{
		for (int i = 0; i < DeveloperSettings::GamePlay::Construction::MaxNumberMaterialsOnPreviewMesh; ++i)
		{
			UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			M_DynamicMaterialPool.Add(DynMaterial);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"BaseMaterial is null in InitializeDynamicMaterialPool in CPPConstructionPreview.cpp"
			"\n Actor: " + GetName());
	}
}

void ACPPConstructionPreview::UpdatePreviewMaterial(bool bIsValidLocation)
{
	for (UMaterialInstanceDynamic* DynMaterial : M_DynamicMaterialPool)
	{
		if (DynMaterial)
		{
			DynMaterial->SetScalarParameterValue(FName("PlacementOkay"), bIsValidLocation);
		}
	}
}

void ACPPConstructionPreview::MoveWidgetToMeshHeight() const
{
	if (not EnsureIsValidPreviewMesh() || not EnsureIsValidPreviewStatsWidgetComponent())
	{
		return;
	}
	// Calculate the position for the widget to be 100 units above the mesh.
	FVector MeshBoundsOrigin, MeshBoundsExtent;
	PreviewMesh->GetStaticMesh()->GetBounds().GetBox().GetCenterAndExtents(MeshBoundsOrigin,
	                                                                       MeshBoundsExtent);
	const float WidgetHeightAboveMesh = MeshBoundsExtent.Z + 300.0f;
	const FVector WidgetWorldPosition = PreviewMesh->GetComponentLocation() +
		FVector(0.0f, 0.0f, WidgetHeightAboveMesh);
	M_PreviewStatsWidgetComponent->SetWorldLocation(WidgetWorldPosition);
}


void ACPPConstructionPreview::RotatePreviewClockwise() const
{
	if (not EnsureIsValidPreviewMesh() || not GetHasActivePreview() || not GetIsRotationAllowed())
	{
		return;
	}
	const float DeltaYaw = M_ActivePreviewStatus == EConstructionPreviewMode::Construct_NomadicPreview ? 90.f : RotationDegrees;
	M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::RotateClockWise);
	FRotator CurrentRotation = PreviewMesh->GetComponentRotation();
	CurrentRotation.Yaw += DeltaYaw;
	PreviewMesh->SetWorldRotation(CurrentRotation);
}

void ACPPConstructionPreview::RotatePreviewCounterclockwise() const
{
	if (not EnsureIsValidPreviewMesh() || not GetHasActivePreview() || not GetIsRotationAllowed())
	{
		return;
	}
	const float DeltaYaw = M_ActivePreviewStatus == EConstructionPreviewMode::Construct_NomadicPreview ? 90.f : RotationDegrees;
	M_PreviewSounds.PlayConstructionSound(this, EConstructionPreviewSoundType::RotateCounterClockWise);
	FRotator CurrentRotation = PreviewMesh->GetComponentRotation();
	CurrentRotation.Yaw -= DeltaYaw;
	PreviewMesh->SetWorldRotation(CurrentRotation);
}

FRotator ACPPConstructionPreview::GetPreviewRotation() const
{
	if (IsValid(PreviewMesh))
	{
		return PreviewMesh->GetComponentToWorld().Rotator();
	}
	return FRotator();
}

bool ACPPConstructionPreview::EnsureIsValidPreviewMesh() const
{
	if (not IsValid(PreviewMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "PreviewMesh",
		                                             "ACPPConstructionPreview::EnsureIsValidPreviewMesh");
		return false;
	}
	return true;
}

bool ACPPConstructionPreview::EnsureIsValidPreviewStatsWidget() const
{
	if (not IsValid(M_PreviewStatsWidget))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "M_PreviewStatsWidget",
		                                             "ACPPConstructionPreview::EnsureIsValidPreviewStatsWidget");
		return false;
	}
	return true;
}

bool ACPPConstructionPreview::EnsureIsValidPreviewStatsWidgetComponent() const
{
	if (not IsValid(M_PreviewStatsWidgetComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "M_PreviewStatsWidgetComponent",
		                                             "ACPPConstructionPreview::EnsureIsValidPreviewStatsWidgetComponent");
		return false;
	}
	return true;
}

void ACPPConstructionPreview::UpdateOverlayGridOverlaps()
{
	if (!EnsureIsValidGridOverlay() || !EnsureIsValidPreviewMesh())
	{
		return;
	}

	// Decide which footprinting method to use:
	//  - Nomadic preview: always use mesh projection (existing behaviour)
	//  - Otherwise: if rules have a non-zero footprint, use a GRID-ALIGNED rectangle
	//    so the tile count is invariant under rotation; else, project the mesh.
	TArray<int32> FootprintIndices;
	const bool bIsNomadic = (M_ActivePreviewStatus == EConstructionPreviewMode::Construct_NomadicPreview);

	if (!bIsNomadic)
	{
		const FBxpConstructionRules& Rules = BxpConstructionData.GetConstructionRules();
		const bool bHasExplicitFootprint =
			(Rules.Footprint.SizeX > 0.f && Rules.Footprint.SizeY > 0.f);

		if (bHasExplicitFootprint)
		{
			// Grid-aligned: ignore the preview yaw and take a fixed block centered at the snapped location.
			// This keeps the footprint constant (e.g., 3×3) for any rotation.
			FPreviewToGridHelpers::ComputeGridIndicesForFootprint_GridAligned(
				/*Grid*/ M_GridOverlay,
				         /*CenterWorld*/ GetActorLocation(), // preview actor is snapped to grid already
				         /*FootprintX*/ Rules.Footprint.SizeX,
				         /*FootprintY*/ Rules.Footprint.SizeY,
				         /*Out*/ FootprintIndices);
		}
		else
		{
			// No explicit footprint → fall back to oriented mesh projection.
			FPreviewToGridHelpers::ComputeGridIndicesUnderPreview(
				M_GridOverlay, PreviewMesh, FootprintIndices);
		}
	}
	else
	{
		// Nomadic: always project mesh (as requested earlier).
		FPreviewToGridHelpers::ComputeGridIndicesUnderPreview(
			M_GridOverlay, PreviewMesh, FootprintIndices);
	}

	// Apply and test overlaps.
	M_GridOverlay->SetConstructionPreviewSquares(FootprintIndices);
	M_GridOverlay->UpdateOverlaps_ForAllTiles();
}
