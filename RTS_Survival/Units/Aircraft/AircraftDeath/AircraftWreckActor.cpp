// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftWreckActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionProxyData.h"
#include "Field/FieldSystemObjects.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace AircraftWreckPrivate
{
	// The explosion kicks the pieces outward once Chaos has released them from their clusters.
	constexpr float ScatterDelaySeconds = 0.05f;
	// Pieces leave the ground a little: a crash throws them up and forward, not down into the ground.
	constexpr float UpwardScatterShare = 0.3f;
	constexpr float ScatterRadius = 2500.f;
	// Pieces tumble; a wreck sliding without spin looks like one solid object.
	constexpr float TumbleRadiansPerSecond = 2.5f;
	// Heavy pieces piled on a thin wing panel squeeze it through the landscape with the default iterations.
	constexpr uint8 PiecePositionIterations = 16;
	constexpr uint8 PieceVelocityIterations = 2;
	constexpr uint8 PieceProjectionIterations = 4;
	// Settled pieces stop jittering on each other instead of creeping into the ground.
	constexpr float PieceAngularDamping = 0.5f;
	// The collection's physics is waited for this long at most; then the wreck is shown anyway.
	constexpr float CarriedPhysicsReadyTimeoutSeconds = 2.f;
	// Frames between the first move onto the aircraft and showing the wreck: the kinematic pieces reach their first
	// target on the next physics step.
	constexpr uint64 FramesBeforeCarriedWreckShows = 2;
	// Strain far above any damage threshold: the piece tears out of its cluster on the next step.
	constexpr float TearOutStrain = 1.0e12f;
	// The field that turns a released piece dynamic reaches this far around it (by its own size, at least the minimum).
	constexpr float ReleaseFieldExtentShare = 0.5f;
	constexpr float MinReleaseFieldRadius = 25.f;
	// The physics state of a released piece is read back this many frames after its field; a missed piece gets a
	// field this much wider, at most this many times.
	constexpr uint64 FramesBeforeReleaseCheck = 6;
	constexpr float ReleaseFieldRetryGrowth = 2.f;
	constexpr int32 MaxReleaseFieldAttempts = 4;
	// Reaches every piece of the largest aircraft from its centre.
	constexpr float WholeWreckFieldRadius = 10000.f;
	// A torn-out piece is still kinematic and moves with the wreck; it is made dynamic this many frames later, by
	// when it has the wreck's speed of its own.
	constexpr uint64 FramesBeforeTornPieceIsLoose = 2;
}

AAircraftWreckActor::AAircraftWreckActor()
{
	// Ticks only while carried, to move released pieces off the wreck.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WreckRoot"));
}

bool AAircraftWreckActor::CreateCollectionComponent(UGeometryCollection* CrashCollection, const bool bKinematic,
                                                    const float PieceLinearDamping)
{
	if (not IsValid(CrashCollection) || not CrashCollection->GetGeometryCollection().IsValid())
	{
		return false;
	}
	M_GeometryCollection = NewObject<UGeometryCollectionComponent>(this, TEXT("WreckPieces"));
	M_GeometryCollection->SetRestCollection(CrashCollection);
	M_GeometryCollection->ObjectType = bKinematic
		                                   ? EObjectStateTypeEnum::Chaos_Object_Kinematic
		                                   : EObjectStateTypeEnum::Chaos_Object_Dynamic;
	M_GeometryCollection->SetEnableDamageFromCollision(false);
	M_GeometryCollection->SetupAttachment(RootComponent);
	M_GeometryCollection->SetRelativeTransform(FTransform::Identity);
	SetupPieceCollision();
	M_GeometryCollection->BodyInstance.LinearDamping = PieceLinearDamping;
	if (bKinematic)
	{
		// A carried wreck lets its pieces go one by one while the rest still moves kinematically: released pieces
		// overlapping held ones were thrown tens of metres, and continuous collision on pieces switching from
		// kinematic to dynamic tripped a Chaos assert, or (with the pieces ignoring each other) silently broke the
		// ground's collision for the rest of the run. Its pieces only land on static ground, without CCD; the death
		// component lets each go before it reaches the ground, so none starts out buried.
		M_GeometryCollection->BodyInstance.bUseCCD = false;
		// Motion-aware collision in place of CCD: fast pieces (a bomber's, or parts torn off high up) tunnelled
		// through the landscape without it.
		M_GeometryCollection->BodyInstance.SetUseMACD(true);
	}
	// Not on a wreck that breaks up at once (it has CCD): with both, pieces piled on each other were squeezed through
	// the landscape. A Bf109 blown up in its bay lost a piece under the floor in half the runs, and none without MACD.
	// The pieces of every wreck pass through each other: a parked aircraft's pieces collapse into one pile, and the
	// pile still squeezed a thin panel through the landscape now and then (about one run in fifteen).
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	return true;
}

void AAircraftWreckActor::GatherPieces(const UGeometryCollection& CrashCollection)
{
	const FGeometryCollection& Collection = *CrashCollection.GetGeometryCollection();
	M_Pieces.Reset();
	for (int32 TransformIndex = 0; TransformIndex < Collection.Children.Num(); ++TransformIndex)
	{
		// Leaves with geometry; the bone transforms of a skeletal aircraft are leaves without any.
		const int32 GeometryIndex = Collection.TransformToGeometryIndex[TransformIndex];
		if (not Collection.Children[TransformIndex].IsEmpty() || not Collection.BoundingBox.IsValidIndex(GeometryIndex))
		{
			continue;
		}
		const FBox GeometryBox(Collection.BoundingBox[GeometryIndex]);
		const FTransform RestTransform(Collection.Transform[TransformIndex]);
		const FBox PlacedBox = GeometryBox.TransformBy(RestTransform);
		FAircraftWreckPiece& Piece = M_Pieces.AddDefaulted_GetRef();
		Piece.TransformIndex = TransformIndex;
		Piece.LocalCentre = PlacedBox.GetCenter();
		Piece.LocalExtent = PlacedBox.GetExtent();
	}
	M_PieceCount = M_Pieces.Num();
}

int32 AAircraftWreckActor::BreakUp(UGeometryCollection* CrashCollection, const FVector& ImpactVelocity,
                                   const float VelocityRetained, const float ScatterSpeed, const float LifeSeconds)
{
	using namespace AircraftWreckPrivate;
	SetLifeSpan(LifeSeconds);
	if (not CreateCollectionComponent(CrashCollection, false, 0.f))
	{
		return 0;
	}
	// Pieces keep the direction of flight; the downward part of the speed is spent on the ground.
	FVector PieceVelocity = ImpactVelocity * VelocityRetained;
	PieceVelocity.Z = ScatterSpeed * UpwardScatterShare;
	M_GeometryCollection->InitialVelocityType = EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined;
	M_GeometryCollection->InitialLinearVelocity = PieceVelocity;
	M_GeometryCollection->InitialAngularVelocity = FVector(TumbleRadiansPerSecond, TumbleRadiansPerSecond * 0.5f, 0.f);
	M_GeometryCollection->RegisterComponent();
	AddInstanceComponent(M_GeometryCollection);
	// Every cluster lets go at once: the aircraft comes apart in all its pieces where it stands.
	M_GeometryCollection->CrumbleActiveClusters();
	GatherPieces(*CrashCollection);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return M_PieceCount;
	}
	TWeakObjectPtr<AAircraftWreckActor> WeakThis(this);
	const float Strength = ScatterSpeed;
	FTimerHandle ScatterHandle;
	World->GetTimerManager().SetTimer(ScatterHandle, FTimerDelegate::CreateLambda([WeakThis, Strength]()
	{
		if (not WeakThis.IsValid() || not WeakThis->GetIsValidGeometryCollection())
		{
			return;
		}
		WeakThis->M_GeometryCollection->AddRadialImpulse(WeakThis->GetActorLocation(), ScatterRadius, Strength,
		                                                RIF_Linear, true);
	}), ScatterDelaySeconds, false);
	return M_PieceCount;
}

int32 AAircraftWreckActor::StartCarried(UGeometryCollection* CrashCollection, const float LifeSeconds,
                                        const float PieceLinearDamping)
{
	SetLifeSpan(LifeSeconds);
	if (not CreateCollectionComponent(CrashCollection, true, PieceLinearDamping))
	{
		return 0;
	}
	M_GeometryCollection->RegisterComponent();
	AddInstanceComponent(M_GeometryCollection);
	// Whole and kinematic: the cluster moves with the actor. Pieces are torn out of it one by one (ReleasePiece).
	// Hidden and left where it was spawned until its physics exists (FollowTransform).
	M_GeometryCollection->SetVisibility(false);
	GatherPieces(*CrashCollection);
	bM_IsCarried = true;
	bM_IsPhysicsReady = false;
	bM_IsShown = false;
	M_ReleasedPieceCount = 0;
	M_CarriedSinceSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	SetActorTickEnabled(true);
	return M_PieceCount;
}

void AAircraftWreckActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not bM_IsCarried || not IsValid(M_GeometryCollection) || not IsValid(GetWorld()))
	{
		SetActorTickEnabled(false);
		return;
	}
	if (not Carried_CheckPhysicsReady())
	{
		return;
	}
	Carried_AdvanceReleases();
}

bool AAircraftWreckActor::Carried_CheckPhysicsReady()
{
	if (bM_IsPhysicsReady)
	{
		return true;
	}
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - M_CarriedSinceSeconds
		> AircraftWreckPrivate::CarriedPhysicsReadyTimeoutSeconds;
	if (not M_GeometryCollection->GetIsObjectDynamic() && not bTimedOut)
	{
		return false;
	}
	bM_IsPhysicsReady = true;
	M_PhysicsReadyFrame = GFrameCounter;
	return true;
}

void AAircraftWreckActor::FollowTransform(const FTransform& AircraftTransform)
{
	if (not bM_IsPhysicsReady || not IsValid(M_GeometryCollection))
	{
		return;
	}
	SetActorTransform(AircraftTransform, false, nullptr, ETeleportType::None);
	if (not bM_IsShown && GFrameCounter - M_PhysicsReadyFrame >= AircraftWreckPrivate::FramesBeforeCarriedWreckShows)
	{
		bM_IsShown = true;
		M_GeometryCollection->SetVisibility(true);
	}
}

void AAircraftWreckActor::Carried_AdvanceReleases()
{
	using namespace AircraftWreckPrivate;
	bool bAllLoose = true;
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		FAircraftWreckPiece& Piece = M_Pieces[PieceIndex];
		if (Piece.State == EAircraftWreckPieceState::ReleaseRequested)
		{
			// Out of its cluster. The strain spreads (damage propagation), so the first tear frees every piece; each
			// stays kinematic and keeps following the wreck on its own until it is released.
			M_GeometryCollection->ApplyExternalStrain(Piece.TransformIndex, GetActorLocation(), 0.f, 0, 0.f, TearOutStrain);
			Piece.State = EAircraftWreckPieceState::TornOut;
			Piece.TornOutFrame = GFrameCounter;
		}
		else if (Piece.State == EAircraftWreckPieceState::TornOut
			&& GFrameCounter - Piece.TornOutFrame >= FramesBeforeTornPieceIsLoose)
		{
			Piece.State = EAircraftWreckPieceState::Loose;
			Carried_ApplyReleaseField(PieceIndex);
			// The first tear's strain spreads to every piece: from now on each is a body of its own.
			bM_AreClustersBroken = true;
		}
		else if (Piece.State == EAircraftWreckPieceState::Loose && not Piece.bIsConfirmedDynamic
			&& GFrameCounter - Piece.ReleaseFieldFrame >= FramesBeforeReleaseCheck)
		{
			// A field placed from a stale physics result can miss the piece; it would hang in the air for good.
			Piece.bIsConfirmedDynamic = Carried_GetIsPieceDynamicInPhysics(Piece);
			if (not Piece.bIsConfirmedDynamic && Piece.ReleaseFieldAttempts >= MaxReleaseFieldAttempts)
			{
				// Still missed: one field over the whole wreck, rather than a piece left hanging in the air.
				Carried_MakeDynamicInSphere(GetActorLocation(), WholeWreckFieldRadius);
				Piece.bIsConfirmedDynamic = true;
			}
			if (not Piece.bIsConfirmedDynamic)
			{
				Carried_ApplyReleaseField(PieceIndex);
			}
		}
		bAllLoose &= Piece.State == EAircraftWreckPieceState::Loose && Piece.bIsConfirmedDynamic;
	}
	if (bAllLoose)
	{
		// Nothing left to carry: the wreck lies on the ground.
		bM_IsCarried = false;
		SetActorTickEnabled(false);
	}
}

void AAircraftWreckActor::Carried_ApplyReleaseField(const int32 PieceIndex)
{
	using namespace AircraftWreckPrivate;
	FAircraftWreckPiece& Piece = M_Pieces[PieceIndex];
	// A dynamic-state field is the engine's way to turn collection pieces dynamic (it also hands them to the solver's
	// islands; switching the state directly leaves them frozen). The piece keeps the speed its kinematic motion gave
	// it. The field covers where the piece is between the last physics result and its target, wider on each retry.
	const FVector PhysicsCentre = GetPiecePhysicsCentre(PieceIndex);
	const FVector TargetCentre = M_GeometryCollection->GetComponentTransform().TransformPosition(Piece.LocalCentre);
	const float RetryScale = FMath::Pow(ReleaseFieldRetryGrowth, static_cast<float>(Piece.ReleaseFieldAttempts));
	const FVector FieldCentre = FMath::Lerp(PhysicsCentre, TargetCentre, 0.5f);
	const float FieldRadius = static_cast<float>(FVector::Dist(PhysicsCentre, FieldCentre))
		+ RetryScale * FMath::Max(MinReleaseFieldRadius,
		                          ReleaseFieldExtentShare * static_cast<float>(Piece.LocalExtent.GetMin()));
	Carried_MakeDynamicInSphere(FieldCentre, FieldRadius);
	if (Piece.ReleaseFieldAttempts == 0)
	{
		// Once: the velocity adds up, and a retried piece sped up that way punched through the ground.
		M_GeometryCollection->ApplyLinearVelocity(Piece.TransformIndex, Piece.PendingVelocity);
		M_GeometryCollection->ApplyAngularVelocity(Piece.TransformIndex, Piece.PendingSpin);
	}
	Piece.ReleaseFieldFrame = GFrameCounter;
	++Piece.ReleaseFieldAttempts;
}

void AAircraftWreckActor::Carried_MakeDynamicInSphere(const FVector& Centre, const float Radius) const
{
	// Inside the sphere only. ApplyKinematicField sets everything outside it that is not already dynamic to kinematic:
	// pieces resting asleep on the ground turned kinematic, were carried on by the moving hull and left hanging.
	URadialFalloff* Sphere = NewObject<URadialFalloff>(GetTransientPackage());
	Sphere->SetRadialFalloff(1.f, 0.f, 1.f, 0.f, Radius, Centre, EFieldFalloffType::Field_FallOff_None);
	UUniformInteger* DynamicState = NewObject<UUniformInteger>(GetTransientPackage());
	DynamicState->SetUniformInteger(static_cast<int32>(Chaos::EObjectStateType::Dynamic));
	UCullingField* InsideOnly = NewObject<UCullingField>(GetTransientPackage());
	InsideOnly->SetCullingField(Sphere, DynamicState, EFieldCullingOperationType::Field_Culling_Outside);
	M_GeometryCollection->ApplyPhysicsField(true, EGeometryCollectionPhysicsTypeEnum::Chaos_DynamicState, nullptr,
	                                        InsideOnly);
}

bool AAircraftWreckActor::Carried_GetIsPieceDynamicInPhysics(const FAircraftWreckPiece& Piece) const
{
	const FGeometryDynamicCollection* Dynamic = M_GeometryCollection->GetDynamicCollection();
	if (Dynamic == nullptr || not Dynamic->DynamicState.IsValidIndex(Piece.TransformIndex))
	{
		// Nothing to check against: take the release as done.
		return true;
	}
	// Dynamic only: a piece asleep a few frames after its release went to sleep in mid-air and would hang there; the
	// next field wakes it.
	return Dynamic->DynamicState[Piece.TransformIndex] == static_cast<uint8>(Chaos::EObjectStateType::Dynamic);
}

void AAircraftWreckActor::ReleasePiece(const int32 PieceIndex, const FVector& AddedVelocity,
                                       const FVector& AngularVelocity)
{
	if (not M_Pieces.IsValidIndex(PieceIndex) || M_Pieces[PieceIndex].bReleased || not GetIsValidGeometryCollection())
	{
		return;
	}
	FAircraftWreckPiece& Piece = M_Pieces[PieceIndex];
	Piece.bReleased = true;
	++M_ReleasedPieceCount;
	Piece.ReleasedAtSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	Piece.PendingVelocity = AddedVelocity;
	Piece.PendingSpin = AngularVelocity;
	if (bM_AreClustersBroken)
	{
		// Every piece is already on its own: let it go now. Riding the kinematic hull for two more frames drove
		// pieces deep into the ground at low frame rates, and they fell through once loose.
		Piece.State = EAircraftWreckPieceState::Loose;
		Carried_ApplyReleaseField(PieceIndex);
		return;
	}
	// The tick tears it out of the wreck and then lets it fall.
	Piece.State = EAircraftWreckPieceState::ReleaseRequested;
}

bool AAircraftWreckActor::GetHasHeldPieces() const
{
	return bM_IsCarried && M_ReleasedPieceCount < M_Pieces.Num();
}

FVector AAircraftWreckActor::GetPieceWorldCentre(const int32 PieceIndex) const
{
	if (not M_Pieces.IsValidIndex(PieceIndex) || not IsValid(M_GeometryCollection))
	{
		return GetActorLocation();
	}
	const FAircraftWreckPiece& Piece = M_Pieces[PieceIndex];
	if (Piece.State != EAircraftWreckPieceState::Loose)
	{
		// Still moving with the wreck.
		return M_GeometryCollection->GetComponentTransform().TransformPosition(Piece.LocalCentre);
	}
	return GetPiecePhysicsCentre(PieceIndex);
}

FVector AAircraftWreckActor::GetPiecePhysicsCentre(const int32 PieceIndex) const
{
	if (not M_Pieces.IsValidIndex(PieceIndex) || not IsValid(M_GeometryCollection))
	{
		return GetActorLocation();
	}
	const FAircraftWreckPiece& Piece = M_Pieces[PieceIndex];
	const FTransform& ComponentTransform = M_GeometryCollection->GetComponentTransform();
	// The collections are baked into collection space: a piece's transform moves its geometry from where it sat on the
	// aircraft, so its centre now is that transform applied to the geometry's own centre.
	const TArray<FTransform3f>& PieceTransforms = M_GeometryCollection->GetComponentSpaceTransforms3f();
	const UGeometryCollection* Rest = M_GeometryCollection->GetRestCollection();
	if (not PieceTransforms.IsValidIndex(Piece.TransformIndex) || not IsValid(Rest)
		|| not Rest->GetGeometryCollection().IsValid())
	{
		return ComponentTransform.TransformPosition(Piece.LocalCentre);
	}
	const FGeometryCollection& Collection = *Rest->GetGeometryCollection();
	const int32 GeometryIndex = Collection.TransformToGeometryIndex[Piece.TransformIndex];
	const FVector GeometryCentre = Collection.BoundingBox.IsValidIndex(GeometryIndex)
		                               ? FVector(Collection.BoundingBox[GeometryIndex].GetCenter())
		                               : Piece.LocalCentre;
	const FVector ComponentCentre = FVector(PieceTransforms[Piece.TransformIndex].TransformPosition(FVector3f(GeometryCentre)));
	return ComponentTransform.TransformPosition(ComponentCentre);
}

void AAircraftWreckActor::AddWreckFire(UFXSystemComponent* Fire)
{
	if (IsValid(Fire))
	{
		M_WreckFires.Add(Fire);
	}
}

void AAircraftWreckActor::StopFiresAfter(const float Seconds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (Seconds <= 0.f)
	{
		StopFires();
		return;
	}
	TWeakObjectPtr<AAircraftWreckActor> WeakThis(this);
	FTimerHandle FireHandle;
	World->GetTimerManager().SetTimer(FireHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->StopFires();
		}
	}), Seconds, false);
}

void AAircraftWreckActor::StopFires()
{
	// No new flames or smoke; what is in the air burns out, so the fire dies down instead of vanishing.
	for (const TWeakObjectPtr<UFXSystemComponent>& Fire : M_WreckFires)
	{
		if (Fire.IsValid())
		{
			Fire->Deactivate();
		}
	}
	M_WreckFires.Reset();
}

bool AAircraftWreckActor::GetIsValidGeometryCollection() const
{
	if (IsValid(M_GeometryCollection))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_GeometryCollection",
	                                                      "AAircraftWreckActor::GetIsValidGeometryCollection", this);
	return false;
}

void AAircraftWreckActor::SetupPieceCollision() const
{
	using namespace AircraftWreckPrivate;
	if (not IsValid(M_GeometryCollection))
	{
		return;
	}
	// Pieces land on the ground (CreateCollectionComponent lets them pass through each other); units, weapons, the cursor
	// and building previews pass through.
	M_GeometryCollection->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	M_GeometryCollection->SetCollisionObjectType(ECC_PhysicsBody);
	M_GeometryCollection->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	M_GeometryCollection->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	// Thin panels (wing skin) tunnel through the ground at crash speed without it.
	M_GeometryCollection->BodyInstance.bUseCCD = true;
	M_GeometryCollection->BodyInstance.SetPositionSolverIterationCount(PiecePositionIterations);
	M_GeometryCollection->BodyInstance.SetVelocitySolverIterationCount(PieceVelocityIterations);
	M_GeometryCollection->BodyInstance.SetProjectionSolverIterationCount(PieceProjectionIterations);
	M_GeometryCollection->BodyInstance.AngularDamping = PieceAngularDamping;
	M_GeometryCollection->SetCanEverAffectNavigation(false);
	M_GeometryCollection->SetGenerateOverlapEvents(false);
	M_GeometryCollection->SetReceivesDecals(false);
}

