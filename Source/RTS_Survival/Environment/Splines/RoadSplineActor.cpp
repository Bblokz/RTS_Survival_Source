#include "RoadSplineActor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

ARoadSplineActor::ARoadSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create the spline component and set it as the root.
	RoadSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RoadSpline"));
	RootComponent = RoadSpline;

	// Default values for the properties.
	RoadMesh = nullptr;
	RoadMaterial = nullptr;
	ForwardAxis = ESplineMeshAxis::X;
	if(RoadSpline)
	{
		RoadSpline->CastShadow = false;
		RoadSpline->bCastDynamicShadow = false;
	}
}

void ARoadSplineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildRoad();
}

void ARoadSplineActor::BuildRoad()
{
	// Remove any previously created spline mesh components to avoid duplicates.
	for (USplineMeshComponent* SplineMeshComp : SplineMeshComponents)
	{
		if (IsValid(SplineMeshComp))
		{
			SplineMeshComp->DestroyComponent();
		}
	}
	SplineMeshComponents.Empty();

	// If no mesh is assigned, there's nothing to do.
	if (!RoadMesh)
	{
		return;
	}

	const int32 NumPoints = RoadSpline->GetNumberOfSplinePoints();
	// We need at least two points to create a segment.
	if (NumPoints < 2)
	{
		return;
	}
	RoadSpline->bReceivesDecals = bAllowDecals;

	// Loop through each spline segment and create a SplineMeshComponent for it.
	for (int32 i = 0; i < NumPoints - 1; i++)
	{
		// Create a new spline mesh component dynamically.
		USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(this);
		if (SplineMeshComp)
		{
			SplineMeshComp->RegisterComponent();
			SplineMeshComp->SetMobility(EComponentMobility::Movable);
			SplineMeshComp->SetStaticMesh(RoadMesh);

			// If a visual material is provided, set it.
			if (RoadMaterial)
			{
				SplineMeshComp->SetMaterial(0, RoadMaterial);
			}

			// Attach the spline mesh to the spline component.
			SplineMeshComp->AttachToComponent(RoadSpline, FAttachmentTransformRules::KeepRelativeTransform);

			// Get the start and end positions and tangents for the current segment (in local space).
			FVector StartPos = RoadSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
			FVector EndPos = RoadSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);
			FVector StartTangent = RoadSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local);
			FVector EndTangent = RoadSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local);

			// Set the spline mesh's start and end points, which automatically deforms the mesh.
			SplineMeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);

			// Adjust collision and shadow settings for efficiency.
			SplineMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			SplineMeshComp->SetCastShadow(true);

			FRTS_CollisionSetup::SetupGroundEnvActorCollision(SplineMeshComp, false, false);

			// Add the newly created component to our array for tracking.
			SplineMeshComponents.Add(SplineMeshComp);
		}
	}
}

void ARoadSplineActor::BeginPlay()
{
	Super::BeginPlay();

	TArray<USplineMeshComponent*> SplineComps;
	GetComponents<USplineMeshComponent>(SplineComps);

	for (USplineMeshComponent* Spline : SplineComps)
	{
		if (Spline)
		{
			Spline->DestroyComponent();
		}
	}
	

	// Ensure the road is built when the actor begins play.
	BuildRoad();
}
