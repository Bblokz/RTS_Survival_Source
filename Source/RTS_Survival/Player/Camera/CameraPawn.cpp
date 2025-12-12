// Copyright (C) Bas Blokzijl - All rights reserved.


#include "CameraPawn.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/GameUI/MiniMap/W_MiniMap.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values
ACameraPawn::ACameraPawn(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer), M_CameraComponent(nullptr)
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

FVector ACameraPawn::GetCameraGroundLocation_Implementation()
{
	return FVector::ZeroVector;
}


// Called when the game starts or when spawned
void ACameraPawn::BeginPlay()
{
	Super::BeginPlay();
	ACPPController* Lv_Controller = FRTS_Statics::GetRTSController(this);
	if (not Lv_Controller)
	{
		return;
	}
	TWeakObjectPtr<ACameraPawn> Weakthis(this);
	M_PlayerController = Lv_Controller;

	Lv_Controller->OnMainMenuCallbacks.CallbackOnMenuReady(
		[CameraPawn = Weakthis ]()
		{
			if (CameraPawn.IsValid())
			{
				CameraPawn->OnMainMenuLoaded();
			}
		},
		Weakthis
	);
}

void ACameraPawn::InitCameraPawn(UCameraComponent* NewCameraComponent)
{
	if (IsValid(NewCameraComponent))
	{
		M_CameraComponent = NewCameraComponent;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "CameraComponent",
		                                             "ACameraPawn::InitCameraPawn");
	}
}

void ACameraPawn::OnMainMenuLoaded()
{
	M_FowManager = FRTS_Statics::GetFowManager(this);
	auto MainMenu = FRTS_Statics::GetMainGameUI(this);
	if (not MainMenu || not M_FowManager)
	{
		RTSFunctionLibrary::ReportError("Cannot start drawing camera on minimap as either"
			"fow manager or the mainmenu is invalid!");
		return;
	}
	M_MiniMap = MainMenu->GetIsValidMiniMap();

	// Bind click events
    	M_MiniMap->OnMiniMapClicked.AddDynamic(this, &ACameraPawn::HandleMiniMapClick);
	// Set the landscape plane at 0,0 100 z and normal 0,0,1
	M_LandscapePlane = FPlane(FVector(0, 0, 100), FVector(0, 0, 1));
	bM_UpdateMiniMapWithCamera = true;
}

void ACameraPawn::DrawCameraOnMiniMap()
{
	if (not IsValid(M_PlayerController) || not IsValid(M_MiniMap) || not IsValid(M_FowManager) || not IsValid(
		M_SpringArmComponent))
	{
		return;
	}
	UMaterialInstanceDynamic* DynamicMatInstance = M_MiniMap->GetIsValidMiniMapImg()->GetDynamicMaterial();
	if (not DynamicMatInstance)
	{
		return;
	}

	// Get fow settings to calculate the projections relative to the center of the render targets (aka the fow manager position)
	const FVector FowOrigin = M_FowManager->GetActorLocation();
	// Map extent on the FOW manager is half the size of the world as it extents in both directions.
	const float MapExtent = M_FowManager->GetMapExtent() * 2;

	const float SpringArm4X = M_SpringArmComponent->TargetArmLength * 4;
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	
	bool bProjectionSuccessful = false;

	FVector2D ProjectedLocation = GetMiniMapLocationRelativeToFow(SpringArm4X, FowOrigin, MapExtent,
	                                                              FVector2D(0.0, 0.0), bProjectionSuccessful);
	if(bProjectionSuccessful)
	{
		M_ViewportProjections.TopLeft = ProjectedLocation;	
	}
	ProjectedLocation = GetMiniMapLocationRelativeToFow(SpringArm4X, FowOrigin, MapExtent,
	                                     FVector2D(ViewportSize.X, 0.0), bProjectionSuccessful);
	if(bProjectionSuccessful)
	{
		M_ViewportProjections.TopRight = ProjectedLocation;
	}
	ProjectedLocation = GetMiniMapLocationRelativeToFow(SpringArm4X, FowOrigin, MapExtent,
	                                     FVector2D(0.0, ViewportSize.Y), bProjectionSuccessful);
	if(bProjectionSuccessful)
	{
		M_ViewportProjections.BottomLeft = ProjectedLocation;
	
	}
	ProjectedLocation = GetMiniMapLocationRelativeToFow(SpringArm4X, FowOrigin, MapExtent,
	                                     FVector2D(ViewportSize.X, ViewportSize.Y), bProjectionSuccessful);
	if(bProjectionSuccessful)
	{
		M_ViewportProjections.BottomRight = ProjectedLocation;
	}
	const FLinearColor P1P2 = FLinearColor(M_ViewportProjections.TopLeft.X, M_ViewportProjections.TopLeft.Y,
	                                       M_ViewportProjections.TopRight.X, M_ViewportProjections.TopRight.Y);
	DynamicMatInstance->SetVectorParameterValue("P1P2", P1P2);
	// In the shader it seems that P3P4 has the xy as P3 so bottom right comes first.
	const FLinearColor P3P4 = FLinearColor(M_ViewportProjections.BottomRight.X, M_ViewportProjections.BottomRight.Y,
	                                       M_ViewportProjections.BottomLeft.X, M_ViewportProjections.BottomLeft.Y);
	DynamicMatInstance->SetVectorParameterValue("P3P4", P3P4);
	
	
}

FVector2D ACameraPawn::GetMiniMapLocationRelativeToFow(const float SpringArmLength,
                                                       const FVector& FowLocation,
                                                       const float MapExtent,
                                                       const FVector2D ScreenPositionToProject,
                                                       bool& bProjectionSuccessful) const
{
	FVector2D MiniMapLocation = FVector2D(0, 0);
	float TangetIntersect = 0.0f;
	bProjectionSuccessful = false;
	FVector Intersection;
	FVector ProjectedWorldPostion, ProjectedWorldDirection;
	/**
	 * World position: where the pixel (top left in this case) projects into world space.
	 * "If i shoot a ray from this pixel from camera pov it starts here  and goes in this direction" 
	 */
	(void)UGameplayStatics::DeprojectScreenToWorld(M_PlayerController, ScreenPositionToProject, ProjectedWorldPostion,
	                                               ProjectedWorldDirection);
	const FVector ProjectionToLandscapeEnd = ProjectedWorldPostion + (ProjectedWorldDirection * SpringArmLength);
	// Where does our camera ray through the pixel hit the landscape height in world space?
	if (UKismetMathLibrary::LinePlaneIntersection(ProjectedWorldPostion, ProjectionToLandscapeEnd, M_LandscapePlane,
	                                              TangetIntersect,
	                                              Intersection))
	{
		// Get position normalized on the mini map.
		Intersection -= FowLocation;
		Intersection /= MapExtent;
		// After dividing by 2 * MapExtent, the coordinate is in [-0.5, 0.5] range relative to the FOW center.  
		// Adding (0.5, 0.5) shifts it into [0, 1] UV space, aligning it with the texture coordinate system.
		Intersection += FVector(0.5f, 0.5f, 0);
		MiniMapLocation.X = Intersection.X;
		MiniMapLocation.Y = Intersection.Y;
		bProjectionSuccessful = true;
	}
	return MiniMapLocation;
}

void ACameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bM_UpdateMiniMapWithCamera)
	{
		DrawCameraOnMiniMap();
	}
}

// Called to bind functionality to input
void ACameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

FVector ACameraPawn::GetCameraGroundPosition_Implementation()
{
	return FVector::ZeroVector;
}

void ACameraPawn::HandleMiniMapClick(FVector2D LocalClickUV)
{
	if (not IsValid(M_MiniMap) || not IsValid(M_FowManager))
	{
		return;
	}

	// Undo UV→projection normalization:
	const FVector FowOrigin = M_FowManager->GetActorLocation();
	const float MapExtent = M_FowManager->GetMapExtent() * 2.0f;
	const FVector2D OffsetUV = LocalClickUV - FVector2D(0.5f, 0.5f);
	const FVector WorldXY = FowOrigin +
							FVector(OffsetUV.X * MapExtent, OffsetUV.Y * MapExtent, 0.0f);

	// Cast straight down to find the landscape
	const FVector Start = WorldXY + FVector(0,0,10000.0f);
	const FVector End   = WorldXY + FVector(0,0,-10000.0f);

	FHitResult Hit;
	FCollisionQueryParams Params(NAME_None, false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		const FVector HitLoc = Hit.ImpactPoint;
		const FVector Curr   = GetActorLocation();
		// Move so X,Y go to clicked point, keep current camera height.
		SetActorLocation(FVector(HitLoc.X, HitLoc.Y, Curr.Z), true);
	}
}
	


void ACameraPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	UCameraComponent* CameraComponent = FindComponentByClass<UCameraComponent>();
	InitCameraPawn(CameraComponent);
	USpringArmComponent* SpringArmComponent = FindComponentByClass<USpringArmComponent>();
	M_SpringArmComponent = SpringArmComponent;
}
