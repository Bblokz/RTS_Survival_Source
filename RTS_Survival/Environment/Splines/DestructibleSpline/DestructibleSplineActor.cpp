// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DestructibleSplineActor.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "PhysicsEngine/BodySetup.h"
#include "SplinePieceCollapseProxy.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ADestructibleSplineActor::ADestructibleSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("DestructibleSpline"));
	RootComponent = Spline;
	if (Spline)
	{
		Spline->CastShadow = false;
		Spline->bCastDynamicShadow = false;
	}
}

void ADestructibleSplineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildSpline();
}

void ADestructibleSplineActor::RebuildSpline()
{
	BuildSpline();
}

void ADestructibleSplineActor::BeginPlay()
{
	Super::BeginPlay();

	// Mirror ARoadSplineActor: remove any stale spline mesh components carried over from the
	// editor world, then build fresh runtime pieces (this is where crush overlaps get bound).
	TArray<USplineMeshComponent*> StaleComponents;
	GetComponents<USplineMeshComponent>(StaleComponents);
	for (USplineMeshComponent* StaleComponent : StaleComponents)
	{
		if (IsValid(StaleComponent))
		{
			StaleComponent->DestroyComponent();
		}
	}
	M_Segments.Empty();
	M_SegmentIndexByComponent.Empty();

	BuildSpline();
}

void ADestructibleSplineActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Components are destroyed with the actor; just drop the bookkeeping.
	// Running collapse proxies own their lifecycle (weak-owner utilities), no teardown needed here.
	M_Segments.Empty();
	M_SegmentIndexByComponent.Empty();
	Super::EndPlay(EndPlayReason);
}

// ------------------------- BUILD ----------------------------------

void ADestructibleSplineActor::ClearBuiltSegments()
{
	for (const FSplineDestructibleSegment& Segment : M_Segments)
	{
		if (IsValid(Segment.MeshComponent))
		{
			Segment.MeshComponent->DestroyComponent();
		}
	}
	M_Segments.Empty();
	M_SegmentIndexByComponent.Empty();
	M_RemainingPieces = 0;
}

void ADestructibleSplineActor::BuildSpline()
{
	ClearBuiltSegments();

	if (not IsValid(PieceMesh) || not IsValid(Spline))
	{
		return;
	}
	const int32 NumPoints = Spline->GetNumberOfSplinePoints();
	if (NumPoints < 2)
	{
		return;
	}

	ValidatePieceMeshCollision();

	const int32 NumSegments = NumPoints - 1;
	M_Segments.Reserve(NumSegments);
	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		BuildSegment(SegmentIndex);
	}
	M_RemainingPieces = M_Segments.Num();
}

void ADestructibleSplineActor::ValidatePieceMeshCollision()
{
	UBodySetup* MeshBodySetup = IsValid(PieceMesh) ? PieceMesh->GetBodySetup() : nullptr;
	const bool bHasSimpleCollision = MeshBodySetup && MeshBodySetup->AggGeom.GetElementCount() > 0;
	const bool bUsesComplexAsSimple = MeshBodySetup &&
		MeshBodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple;
	if (bHasSimpleCollision || bUsesComplexAsSimple)
	{
		return;
	}
	if (bM_ReportedMissingPieceCollision)
	{
		return;
	}
	bM_ReportedMissingPieceCollision = true;
	RTSFunctionLibrary::ReportError(
		"DestructibleSplineActor: PieceMesh '" + (IsValid(PieceMesh) ? PieceMesh->GetName() : FString("null")) +
		"' has NO usable collision data: no simple collision shapes and Collision Complexity is not "
		"UseComplexAsSimple. Spline pieces will have NO physics body: they cannot block, be hit by weapons, "
		"or be crushed by vehicles. Fix the mesh asset: add simple collision (box/convex, preferred) in the "
		"Static Mesh editor, or set Collision Complexity to UseComplexAsSimple."
		"\n actor: " + GetName());
}

void ADestructibleSplineActor::BuildSegment(const int32 SegmentIndex)
{
	USplineMeshComponent* SegmentComponent = NewObject<USplineMeshComponent>(this);
	if (not IsValid(SegmentComponent))
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: failed to create a spline mesh segment."
			"\n actor: " + GetName());
		return;
	}

	SegmentComponent->SetMobility(EComponentMobility::Movable);
	SegmentComponent->RegisterComponent();
	SegmentComponent->AttachToComponent(Spline, FAttachmentTransformRules::KeepRelativeTransform);
	SegmentComponent->SetStaticMesh(PieceMesh);
	if (IsValid(PieceMaterialOverride))
	{
		SegmentComponent->SetMaterial(0, PieceMaterialOverride);
	}
	// R1: the piece mesh runs forward along +X.
	SegmentComponent->SetForwardAxis(ESplineMeshAxis::X);
	SegmentComponent->SetCastShadow(true);

	FSplineDestructibleSegment SegmentData;
	SegmentData.MeshComponent = SegmentComponent;
	SegmentData.CurrentHealth = PieceHealth;
	SegmentData.StartPos = Spline->GetLocationAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
	SegmentData.EndPos = Spline->GetLocationAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
	SegmentData.StartTangent = Spline->GetTangentAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
	SegmentData.EndTangent = Spline->GetTangentAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
	SegmentComponent->SetStartAndEnd(
		SegmentData.StartPos, SegmentData.StartTangent, SegmentData.EndPos, SegmentData.EndTangent, true);

	ApplyPieceCollision(SegmentComponent);
	EnsureSegmentCollisionBuilt(SegmentComponent);
	M_SegmentIndexByComponent.Add(SegmentComponent, SegmentIndex);

	// Crush overlaps only matter in a running game world; editor preview stays unbound.
	const UWorld* World = GetWorld();
	const bool bIsGameWorld = World && World->IsGameWorld();
	if (bCrushableByVehicles && bIsGameWorld)
	{
		SegmentComponent->OnComponentBeginOverlap.AddDynamic(
			this, &ADestructibleSplineActor::HandlePieceBeginOverlap);
	}

	M_Segments.Add(MoveTemp(SegmentData));
}

void ADestructibleSplineActor::ApplyPieceCollision(USplineMeshComponent* Segment) const
{
	// EXACT same responses as crushable ADestructableEnvActor meshes; bOverlapTanks enables the
	// vehicle overlap channels + overlap event generation used by the crush logic.
	FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(Segment, bCrushableByVehicles);
}

void ADestructibleSplineActor::EnsureSegmentCollisionBuilt(USplineMeshComponent* SegmentComponent)
{
	if (SegmentComponent->GetBodySetup() != nullptr)
	{
		return;
	}
	// The engine builds the deformed body on spline-param changes; force it if that was skipped
	// (USplineMeshComponent::GetBodySetup returns null while no collision has been built).
	SegmentComponent->RecreateCollision();
	SegmentComponent->RecreatePhysicsState();
	if (SegmentComponent->GetBodySetup() != nullptr)
	{
		return;
	}
	if (bM_ReportedMissingPieceCollision)
	{
		return;
	}
	bM_ReportedMissingPieceCollision = true;
	RTSFunctionLibrary::ReportError(
		"DestructibleSplineActor: a spline piece has NO collision body even after forcing a rebuild. "
		"Weapons cannot hit it and vehicles cannot crush it. This means the piece mesh '" +
		(IsValid(PieceMesh) ? PieceMesh->GetName() : FString("null")) +
		"' provides no cookable collision (check simple collision / UseComplexAsSimple on the asset)."
		"\n actor: " + GetName());
}

// ------------------------- CRUSH DESTRUCTION ----------------------

void ADestructibleSplineActor::HandlePieceBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                                                       AActor* OtherActor,
                                                       UPrimitiveComponent* /*OtherComp*/,
                                                       const int32 /*OtherBodyIndex*/,
                                                       const bool /*bFromSweep*/,
                                                       const FHitResult& /*SweepResult*/)
{
	if (not IsValid(OtherActor))
	{
		return;
	}
	const int32* SegmentIndex = M_SegmentIndexByComponent.Find(OverlappedComponent);
	if (not SegmentIndex)
	{
		return;
	}
	if (not GetPassesCrushGate(OtherActor))
	{
		return;
	}
	DestroySegment(*SegmentIndex);
}

bool ADestructibleSplineActor::GetPassesCrushGate(AActor* OtherActor) const
{
	const IRTSNavAgentInterface* NavAgent = Cast<IRTSNavAgentInterface>(OtherActor);
	if (not NavAgent)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	switch (CrushDestructionType)
	{
	case ECrushDestructionType::HeavyTank:
		return AgentType == ERTSNavAgents::HeavyTank;

	case ECrushDestructionType::MediumOrHeavyTank:
		return AgentType == ERTSNavAgents::MediumTank || AgentType == ERTSNavAgents::HeavyTank;

	case ECrushDestructionType::AnyTank:
	default:
		return AgentType != ERTSNavAgents::Character;
	}
}

// ------------------------- HEALTH / DAMAGE ------------------------

float ADestructibleSplineActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                           AController* EventInstigator, AActor* DamageCauser)
{
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		const int32 SegmentIndex = ResolveSegmentFromPointDamage(*PointEvent);
		if (SegmentIndex == INDEX_NONE)
		{
			ReportUnattributableDamageOnce(TEXT("point damage event without hit component/location"));
			// Never return 0: weapons treat 0 as a kill (see UWeaponState::FluxDamageHitActor_DidActorDie).
			return 1.f;
		}
		ApplyDamageToSegment(SegmentIndex, DamageAmount);
		return DamageAmount;
	}
	if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent* RadialEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
		ApplyRadialDamageToSegments(*RadialEvent, DamageAmount);
		return DamageAmount;
	}
	// Plain damage events carry no spatial data to attribute the hit to a piece; ignore them
	// (same convention as AInstancedDestrucablesEnvActor::TakeDamage).
	ReportUnattributableDamageOnce(TEXT("plain FDamageEvent (e.g. hitscan trace/laser/flamethrower weapons)"));
	return 1.f;
}

void ADestructibleSplineActor::ReportUnattributableDamageOnce(const TCHAR* Reason)
{
	if (bM_ReportedUnattributableDamage)
	{
		return;
	}
	bM_ReportedUnattributableDamage = true;
	RTSFunctionLibrary::ReportError(
		"DestructibleSplineActor: received damage that cannot be attributed to a piece and is IGNORED: " +
		FString(Reason) +
		".\nPer-piece damage needs spatial data: projectile/point damage (with hit location), radial damage, "
		"or explicit DamageSegment/DamageSegmentByComponent calls."
		"\n actor: " + GetName());
}

int32 ADestructibleSplineActor::ResolveSegmentFromPointDamage(const FPointDamageEvent& PointEvent) const
{
	// Fast path: the damage event carries the hit component (e.g. full hit results like the ICBM).
	if (UPrimitiveComponent* HitComponent = PointEvent.HitInfo.Component.Get())
	{
		if (const int32* SegmentIndex = M_SegmentIndexByComponent.Find(HitComponent))
		{
			return *SegmentIndex;
		}
	}
	// Projectiles and bombs only fill HitInfo.Location; resolve to the nearest alive piece.
	FVector QueryLocation = PointEvent.HitInfo.Location;
	if (QueryLocation.IsNearlyZero())
	{
		QueryLocation = PointEvent.HitInfo.ImpactPoint;
	}
	if (QueryLocation.IsNearlyZero())
	{
		return INDEX_NONE;
	}
	return FindNearestAliveSegment(QueryLocation);
}

void ADestructibleSplineActor::ApplyRadialDamageToSegments(const FRadialDamageEvent& RadialEvent,
                                                           const float DamageAmount)
{
	const float Radius = FMath::Max(RadialEvent.Params.OuterRadius, 0.f);
	const float RadiusSquared = Radius * Radius;

	// Collect first: applying damage can destroy pieces and mutate the segment state.
	const FTransform SplineWorld = Spline->GetComponentTransform();
	TArray<int32> AffectedSegments;
	for (int32 SegmentIndex = 0; SegmentIndex < M_Segments.Num(); ++SegmentIndex)
	{
		const FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
		if (Segment.bIsCollapsing || not IsValid(Segment.MeshComponent))
		{
			continue;
		}
		// Distance to the piece's start-end line, not its center: long pieces must react to blasts
		// anywhere along their length.
		const FVector WorldStart = SplineWorld.TransformPosition(Segment.StartPos);
		const FVector WorldEnd = SplineWorld.TransformPosition(Segment.EndPos);
		if (FMath::PointDistToSegmentSquared(RadialEvent.Origin, WorldStart, WorldEnd) <= RadiusSquared)
		{
			AffectedSegments.Add(SegmentIndex);
		}
	}
	for (const int32 SegmentIndex : AffectedSegments)
	{
		ApplyDamageToSegment(SegmentIndex, DamageAmount);
	}
}

void ADestructibleSplineActor::ApplyDamageToSegment(const int32 SegmentIndex, const float DamageAmount)
{
	if (not M_Segments.IsValidIndex(SegmentIndex))
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: ApplyDamageToSegment called with invalid segment index: " +
			FString::FromInt(SegmentIndex) + "\n actor: " + GetName());
		return;
	}
	FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
	if (Segment.bIsCollapsing)
	{
		return;
	}
	const float EffectiveDamage = FMath::Max(0.f, DamageAmount - PieceDamageReduction);
	if (EffectiveDamage <= 0.f)
	{
		return;
	}
	Segment.CurrentHealth -= EffectiveDamage;
	if (Segment.CurrentHealth <= 0.f)
	{
		DestroySegment(SegmentIndex);
		return;
	}
	BP_OnPieceDamaged(SegmentIndex, Segment.CurrentHealth);
}

int32 ADestructibleSplineActor::FindNearestAliveSegment(const FVector& WorldLocation) const
{
	int32 NearestIndex = INDEX_NONE;
	float NearestDistSquared = TNumericLimits<float>::Max();
	const FTransform SplineWorld = Spline->GetComponentTransform();
	for (int32 SegmentIndex = 0; SegmentIndex < M_Segments.Num(); ++SegmentIndex)
	{
		const FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
		if (Segment.bIsCollapsing || not IsValid(Segment.MeshComponent))
		{
			continue;
		}
		// Distance to the piece's start-end line, not its center: a hit near the far end of a long
		// piece must not be attributed to the neighbouring piece whose center happens to be closer.
		const FVector WorldStart = SplineWorld.TransformPosition(Segment.StartPos);
		const FVector WorldEnd = SplineWorld.TransformPosition(Segment.EndPos);
		const float DistSquared = FMath::PointDistToSegmentSquared(WorldLocation, WorldStart, WorldEnd);
		if (DistSquared < NearestDistSquared)
		{
			NearestDistSquared = DistSquared;
			NearestIndex = SegmentIndex;
		}
	}
	return NearestIndex;
}

void ADestructibleSplineActor::DamageSegment(const int32 SegmentIndex, const float DamageAmount)
{
	ApplyDamageToSegment(SegmentIndex, DamageAmount);
}

void ADestructibleSplineActor::DamageSegmentByComponent(UPrimitiveComponent* PieceComponent,
                                                        const float DamageAmount)
{
	if (not IsValid(PieceComponent))
	{
		return;
	}
	const int32* SegmentIndex = M_SegmentIndexByComponent.Find(PieceComponent);
	if (not SegmentIndex)
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: DamageSegmentByComponent got a component that is not a piece of this spline: "
			+ PieceComponent->GetName() + "\n actor: " + GetName());
		return;
	}
	ApplyDamageToSegment(*SegmentIndex, DamageAmount);
}

float ADestructibleSplineActor::GetSegmentHealth(const int32 SegmentIndex) const
{
	if (not M_Segments.IsValidIndex(SegmentIndex))
	{
		return 0.f;
	}
	const FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
	if (Segment.bIsCollapsing || not IsValid(Segment.MeshComponent))
	{
		return 0.f;
	}
	return Segment.CurrentHealth;
}

// ------------------------- DESTRUCTION ----------------------------

void ADestructibleSplineActor::DestroyAllSegments()
{
	if (not GetIsValidDestructionConfig())
	{
		return;
	}

	for (int32 SegmentIndex = 0; SegmentIndex < M_Segments.Num(); ++SegmentIndex)
	{
		if (IsActorBeingDestroyed())
		{
			return;
		}

		const FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
		if (Segment.bIsCollapsing || not IsValid(Segment.MeshComponent))
		{
			continue;
		}
		DestroySegment(SegmentIndex);
	}
}

void ADestructibleSplineActor::DestroySegment(const int32 SegmentIndex)
{
	if (not M_Segments.IsValidIndex(SegmentIndex))
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: DestroySegment called with invalid segment index: " +
			FString::FromInt(SegmentIndex) + "\n actor: " + GetName());
		return;
	}
	FSplineDestructibleSegment& Segment = M_Segments[SegmentIndex];
	if (Segment.bIsCollapsing)
	{
		return;
	}
	if (not GetIsValidDestructionConfig())
	{
		return;
	}
	Segment.bIsCollapsing = true;

	RemovePieceComponent(Segment);
	SpawnCollapseProxy(Segment);

	M_RemainingPieces = FMath::Max(0, M_RemainingPieces - 1);
	BP_OnPieceCollapsed(SegmentIndex);
	if (M_RemainingPieces == 0)
	{
		BP_OnSplineFullyDestroyed();
	}
}

void ADestructibleSplineActor::DestroySegmentByComponent(UPrimitiveComponent* PieceComponent)
{
	if (not IsValid(PieceComponent))
	{
		return;
	}
	const int32* SegmentIndex = M_SegmentIndexByComponent.Find(PieceComponent);
	if (not SegmentIndex)
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: DestroySegmentByComponent got a component that is not a piece of this spline: "
			+ PieceComponent->GetName() + "\n actor: " + GetName());
		return;
	}
	DestroySegment(*SegmentIndex);
}

bool ADestructibleSplineActor::GetIsValidDestructionConfig() const
{
	const bool bIsGeoCollapse = DestructionType == ESplineDestructionType::GeometryCollectionCollapse;
	if (bIsGeoCollapse && PieceGeometryCollection.IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: DestructionType is GeometryCollectionCollapse but no "
			"PieceGeometryCollection asset is assigned; refusing to destroy the piece."
			"\n actor: " + GetName());
		return false;
	}
	return true;
}

void ADestructibleSplineActor::RemovePieceComponent(FSplineDestructibleSegment& Segment)
{
	if (not IsValid(Segment.MeshComponent))
	{
		return;
	}
	// Kill overlap + collision first so the crushing vehicle cannot re-trigger against a ghost piece.
	Segment.MeshComponent->OnComponentBeginOverlap.RemoveDynamic(
		this, &ADestructibleSplineActor::HandlePieceBeginOverlap);
	Segment.MeshComponent->SetGenerateOverlapEvents(false);
	Segment.MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	M_SegmentIndexByComponent.Remove(Segment.MeshComponent);
	Segment.MeshComponent->DestroyComponent();
	Segment.MeshComponent = nullptr;
}

void ADestructibleSplineActor::SpawnCollapseProxy(const FSplineDestructibleSegment& Segment)
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	// Spawn the proxy at the piece center with the spline's rotation/scale and re-express the deform
	// parameters relative to that center: the replica lands pixel-identical to the destroyed piece,
	// while FX, the vertical sink and the fracture force all act at the piece instead of the spline origin.
	const FTransform SplineWorld = Spline->GetComponentTransform();
	const FVector CenterLocal = (Segment.StartPos + Segment.EndPos) * 0.5f;
	const FTransform ProxyTransform(
		SplineWorld.GetRotation(), SplineWorld.TransformPosition(CenterLocal), SplineWorld.GetScale3D());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASplinePieceCollapseProxy* Proxy =
		World->SpawnActor<ASplinePieceCollapseProxy>(ASplinePieceCollapseProxy::StaticClass(),
		                                             ProxyTransform, SpawnParameters);
	if (not IsValid(Proxy))
	{
		RTSFunctionLibrary::ReportError(
			"DestructibleSplineActor: failed to spawn the collapse proxy for a destroyed piece."
			"\n actor: " + GetName());
		return;
	}

	const FVector LocalStartPos = Segment.StartPos - CenterLocal;
	const FVector LocalEndPos = Segment.EndPos - CenterLocal;
	switch (DestructionType)
	{
	case ESplineDestructionType::GeometryCollectionCollapse:
		Proxy->StartGeometryCollapse(
			PieceMesh, PieceMaterialOverride,
			LocalStartPos, Segment.StartTangent, LocalEndPos, Segment.EndTangent,
			PieceGeometryCollection, CollapseDuration, CollapseForce, CollapseFX);
		break;

	case ESplineDestructionType::VerticalCollapse:
	default:
		Proxy->StartVerticalCollapse(
			PieceMesh, PieceMaterialOverride,
			LocalStartPos, Segment.StartTangent, LocalEndPos, Segment.EndTangent,
			VerticalCollapseSettings, CollapseFX);
		break;
	}
}
