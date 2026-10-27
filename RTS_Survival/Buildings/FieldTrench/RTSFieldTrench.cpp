// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSFieldTrench.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace RTSFieldTrenchPrivate
{
	constexpr float GroundTraceHalfHeight = 2000.0f;
	// The ground is sampled this far to either side of the centreline to lean the piece with a side slope.
	constexpr float GroundSampleHalfWidth = 150.0f;
	// The end cap lies behind its port; the ground there decides its lean.
	constexpr float EndCapGroundSampleLength = -150.0f;
	const FName PlacementOkayParameter(TEXT("PlacementOkay"));
	const TCHAR* const DefaultPlannedPieceMaterial =
		TEXT("/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev.MI_ConstructionPrev");
}

ARTSFieldTrench::ARTSFieldTrench()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PieceCollisionTemplate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceCollisionTemplate"));
	PieceCollisionTemplate->SetupAttachment(RootComponent);
	PieceCollisionTemplate->SetHiddenInGame(true);
	PieceCollisionTemplate->SetCanEverAffectNavigation(false);
	// The collision of BP_CargoTrench: traces see the trench, but soldiers and vehicles move through it, so the
	// navigation mesh of the landscape under it stays usable.
	PieceCollisionTemplate->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PieceCollisionTemplate->SetCollisionObjectType(ECC_WorldStatic);
	PieceCollisionTemplate->SetCollisionResponseToAllChannels(ECR_Ignore);
	PieceCollisionTemplate->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PieceCollisionTemplate->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
	PieceCollisionTemplate->SetCollisionResponseToChannel(COLLISION_TRACE_LANDSCAPE, ECR_Block);
	PieceCollisionTemplate->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
	PieceCollisionTemplate->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
	PieceCollisionTemplate->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY_HULL, ECR_Block);
	PlannedPieceMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(RTSFieldTrenchPrivate::DefaultPlannedPieceMaterial));
}

void ARTSFieldTrench::BeginPlay()
{
	Super::BeginPlay();
	// Pieces added before the game started are cover too.
	if (GetBuiltPieceCount() > 0)
	{
		ScheduleCoverRefresh();
	}
}

void ARTSFieldTrench::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ARTSFieldTrench::SetPlannedPieces(const TArray<FFieldTrenchPiecePlacement>& Pieces)
{
	TArray<int32, TInlineAllocator<64>> UnbuiltIndices;
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		if (M_Pieces[PieceIndex].State != EFieldTrenchPieceState::Built)
		{
			UnbuiltIndices.Add(PieceIndex);
		}
	}
	for (int32 PlanIndex = 0; PlanIndex < Pieces.Num(); ++PlanIndex)
	{
		const bool bReusesPiece = UnbuiltIndices.IsValidIndex(PlanIndex);
		FFieldTrenchPieceRuntime& Piece = bReusesPiece
			? M_Pieces[UnbuiltIndices[PlanIndex]]
			: M_Pieces.AddDefaulted_GetRef();
		if (bReusesPiece && Piece.State == EFieldTrenchPieceState::Digging)
		{
			RefundPiece(Piece);
		}
		if (not IsValid(Piece.Component))
		{
			Piece.Component = CreatePieceComponent(Pieces[PlanIndex].Piece);
		}
		else if (Piece.Placement.Piece != Pieces[PlanIndex].Piece)
		{
			SetPieceMesh(*Piece.Component, Pieces[PlanIndex].Piece);
		}
		Piece.Placement = Pieces[PlanIndex];
		Piece.State = EFieldTrenchPieceState::Planned;
		Piece.DigProgress = 0.0f;
		FitPieceToGround(Piece);
		ShowPieceState(Piece);
	}
	// Unbuilt pieces the new plan no longer has; removed from the back so the indices stay valid.
	for (int32 LeftoverIndex = UnbuiltIndices.Num() - 1; LeftoverIndex >= Pieces.Num(); --LeftoverIndex)
	{
		RemovePiece(UnbuiltIndices[LeftoverIndex]);
	}
}

void ARTSFieldTrench::SetPlanIsValid(const bool bIsValid)
{
	if (IsValid(M_PlannedMaterial) || IsValid(GetPlannedMaterial()))
	{
		M_PlannedMaterial->SetScalarParameterValue(RTSFieldTrenchPrivate::PlacementOkayParameter, bIsValid ? 1.0f : 0.0f);
	}
}

void ARTSFieldTrench::StartConstruction(
	const TArray<FFieldTrenchPiecePlacement>& Pieces,
	const int32 OwningPlayer,
	const TMap<ERTSResourceType, int32>& CostPerMetre)
{
	if (not Pieces.IsEmpty())
	{
		SetPlannedPieces(Pieces);
	}
	SetPlanIsValid(true);
	M_OwningPlayer = OwningPlayer;
	M_CostPerMetre = CostPerMetre;
	M_TimeUntilResourceRetry = 0.0f;
	SetActorTickEnabled(FindPieceToDig() != INDEX_NONE);
}

int32 ARTSFieldTrench::AddBuiltPiece(const FFieldTrenchPiecePlacement& Placement)
{
	FFieldTrenchPieceRuntime& Piece = M_Pieces.AddDefaulted_GetRef();
	Piece.Placement = Placement;
	Piece.Component = CreatePieceComponent(Placement.Piece);
	Piece.State = EFieldTrenchPieceState::Built;
	Piece.DigProgress = 1.0f;
	FitPieceToGround(Piece);
	OnPieceBuilt(Piece);
	return M_Pieces.Num() - 1;
}

void ARTSFieldTrench::RemovePiece(const int32 PieceIndex)
{
	if (not M_Pieces.IsValidIndex(PieceIndex))
	{
		return;
	}
	const FFieldTrenchPieceRuntime RemovedPiece = M_Pieces[PieceIndex];
	M_Pieces.RemoveAt(PieceIndex);
	if (RemovedPiece.State == EFieldTrenchPieceState::Digging)
	{
		RefundPiece(RemovedPiece);
	}
	if (IsValid(RemovedPiece.Component))
	{
		RemovedPiece.Component->DestroyComponent();
	}
	if (RemovedPiece.State == EFieldTrenchPieceState::Built)
	{
		ScheduleCoverRefresh();
	}
}

void ARTSFieldTrench::CancelUnbuiltPieces()
{
	for (int32 PieceIndex = M_Pieces.Num() - 1; PieceIndex >= 0; --PieceIndex)
	{
		if (M_Pieces[PieceIndex].State != EFieldTrenchPieceState::Built)
		{
			RemovePiece(PieceIndex);
		}
	}
	bM_IsWaitingForResources = false;
	SetActorTickEnabled(false);
}

int32 ARTSFieldTrench::GetBuiltPieceCount() const
{
	int32 BuiltCount = 0;
	for (const FFieldTrenchPieceRuntime& Piece : M_Pieces)
	{
		BuiltCount += Piece.State == EFieldTrenchPieceState::Built ? 1 : 0;
	}
	return BuiltCount;
}

float ARTSFieldTrench::GetSteepestPlannedSlopeDegrees() const
{
	float SteepestSlope = 0.0f;
	for (const FFieldTrenchPieceRuntime& Piece : M_Pieces)
	{
		if (Piece.State != EFieldTrenchPieceState::Built)
		{
			SteepestSlope = FMath::Max(SteepestSlope, Piece.SlopeDegrees);
		}
	}
	return SteepestSlope;
}

TMap<ERTSResourceType, int32> ARTSFieldTrench::GetCostOfLength(
	const TMap<ERTSResourceType, int32>& CostPerMetre,
	const float LengthCm)
{
	TMap<ERTSResourceType, int32> Cost;
	for (const TPair<ERTSResourceType, int32>& ResourcePrice : CostPerMetre)
	{
		if (ResourcePrice.Value <= 0)
		{
			continue;
		}
		Cost.Add(ResourcePrice.Key, FMath::Max(1, FMath::RoundToInt32(ResourcePrice.Value * LengthCm / 100.0f)));
	}
	return Cost;
}

void ARTSFieldTrench::GetUsableCoverSockets(TArray<FRTSActorCoverSocket>& OutCoverSockets) const
{
	for (const FFieldTrenchPieceRuntime& Piece : M_Pieces)
	{
		if (Piece.State != EFieldTrenchPieceState::Built || not IsValid(Piece.Component))
		{
			continue;
		}
		const FVector PieceUp = Piece.GroundTransform.GetRotation().GetUpVector();
		const FVector PieceGround = Piece.GroundTransform.GetLocation();
		// The cover finder keeps the sockets whose names mark cover; the others are passed along unread.
		for (const FName& SocketName : Piece.Component->GetAllSocketNames())
		{
			FRTSActorCoverSocket& CoverSocket = OutCoverSockets.AddDefaulted_GetRef();
			CoverSocket.SocketName = SocketName;
			CoverSocket.WorldTransform = Piece.Component->GetSocketTransform(SocketName, RTS_World);
			// The sockets lie below the ground in the trench; the soldier walks to the ground right above his
			// socket, which the trench leaves to the navigation mesh, and is lowered onto the socket from there.
			const FVector SocketLocation = CoverSocket.WorldTransform.GetLocation();
			CoverSocket.ApproachLocation = SocketLocation + PieceUp * FVector::DotProduct(PieceGround - SocketLocation, PieceUp);
		}
	}
}

void ARTSFieldTrench::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickDigging(DeltaSeconds);
}

UStaticMeshComponent* ARTSFieldTrench::CreatePieceComponent(const EFieldTrenchPiece Piece)
{
	UStaticMeshComponent* PieceComponent = NewObject<UStaticMeshComponent>(this);
	PieceComponent->SetMobility(EComponentMobility::Movable);
	PieceComponent->SetCanEverAffectNavigation(false);
	// Every channel, the project's own included: wrong custom responses put soldiers leaving a socket off their
	// navmesh point.
	PieceComponent->SetCollisionObjectType(PieceCollisionTemplate->GetCollisionObjectType());
	PieceComponent->SetCollisionResponseToChannels(PieceCollisionTemplate->GetCollisionResponseToChannels());
	PieceComponent->SetCollisionEnabled(PieceCollisionTemplate->GetCollisionEnabled());
	PieceComponent->SetGenerateOverlapEvents(PieceCollisionTemplate->GetGenerateOverlapEvents());
	SetPieceMesh(*PieceComponent, Piece);
	PieceComponent->SetupAttachment(RootComponent);
	PieceComponent->RegisterComponent();
	AddInstanceComponent(PieceComponent);
	return PieceComponent;
}

void ARTSFieldTrench::SetPieceMesh(UStaticMeshComponent& PieceComponent, const EFieldTrenchPiece Piece) const
{
	UStaticMesh* PieceMesh = Cast<UStaticMesh>(FieldTrenchLayout::GetPieceMeshPath(Piece).TryLoad());
	if (not IsValid(PieceMesh))
	{
		RTSFunctionLibrary::ReportError(
			"Field trench " + GetName() + ": could not load the mesh of piece " + UEnum::GetValueAsString(Piece) + ".");
	}
	PieceComponent.SetStaticMesh(PieceMesh);
}

void ARTSFieldTrench::FitPieceToGround(FFieldTrenchPieceRuntime& Piece) const
{
	using namespace RTSFieldTrenchPrivate;
	const FFieldTrenchPiecePlacement& Placement = Piece.Placement;
	const FFieldTrenchPieceDefinition& Definition = FieldTrenchLayout::GetPieceDefinition(Placement.Piece);
	// Every piece has its first port at its origin; the ground towards the other port gives the lean along it.
	const float SampleLength = Definition.Ports.Num() >= 2 ? Definition.Ports[1].Location.X : EndCapGroundSampleLength;
	const FRotator YawRotation(0.0f, Placement.Yaw, 0.0f);
	const FVector2D Origin(Placement.Location);
	auto ToWorld = [&YawRotation, &Origin](const float LocalX, const float LocalY)
	{
		return Origin + FVector2D(YawRotation.RotateVector(FVector(LocalX, LocalY, 0.0f)));
	};
	FVector GroundOrigin;
	FVector GroundAlong;
	FVector GroundLeft;
	FVector GroundRight;
	const float AroundHeight = Placement.Location.Z;
	const bool bHasOrigin = TryTraceGround(Origin, AroundHeight, GroundOrigin);
	const bool bHasSlope = bHasOrigin &&
		TryTraceGround(ToWorld(SampleLength, 0.0f), AroundHeight, GroundAlong) &&
		TryTraceGround(ToWorld(SampleLength * 0.5f, -GroundSampleHalfWidth), AroundHeight, GroundLeft) &&
		TryTraceGround(ToWorld(SampleLength * 0.5f, GroundSampleHalfWidth), AroundHeight, GroundRight);
	FRotator GroundRotation = YawRotation;
	if (bHasSlope)
	{
		const FVector AlongAxis = (GroundAlong - GroundOrigin) * FMath::Sign(SampleLength);
		GroundRotation = FRotationMatrix::MakeFromXY(AlongAxis, GroundRight - GroundLeft).Rotator();
	}
	Piece.GroundTransform = FTransform(GroundRotation, bHasOrigin ? GroundOrigin : Placement.Location);
	const FVector PieceUp = GroundRotation.Quaternion().GetUpVector();
	Piece.SlopeDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(PieceUp.Z, -1.0f, 1.0f)));
	// The ground along the centreline and at the sides, measured from the plane the piece lies in.
	Piece.GroundGapCm = 0.0f;
	if (not bHasSlope)
	{
		return;
	}
	for (const FVector2D& LocalSample : {
		     FVector2D(SampleLength * 0.25f, 0.0f), FVector2D(SampleLength * 0.5f, 0.0f),
		     FVector2D(SampleLength * 0.75f, 0.0f), FVector2D(SampleLength, -GroundSampleHalfWidth),
		     FVector2D(SampleLength, GroundSampleHalfWidth), FVector2D(0.0f, -GroundSampleHalfWidth),
		     FVector2D(0.0f, GroundSampleHalfWidth)
	     })
	{
		FVector GroundSample;
		if (TryTraceGround(ToWorld(LocalSample.X, LocalSample.Y), AroundHeight, GroundSample))
		{
			Piece.GroundGapCm = FMath::Max(
				Piece.GroundGapCm, FMath::Abs(FVector::DotProduct(GroundSample - GroundOrigin, PieceUp)));
		}
	}
}

TArray<FFieldTrenchPiecePlacement> ARTSFieldTrench::PlanOnGround(
	const TArray<FVector2D>& DragPath,
	const float MaximumLengthCm,
	const float GroundHeight) const
{
	const float ShortestStraight = FieldTrenchLayout::GetPieceDefinition(EFieldTrenchPiece::Straight_125).LengthCm;
	TArray<FFieldTrenchPiecePlacement> Pieces;
	for (const float MaximumStraight : {500.0f, 250.0f, 125.0f})
	{
		Pieces = FieldTrenchLayout::PlanAlongPath(DragPath, MaximumLengthCm, MaximumStraight);
		bool bStraightsLieOnGround = true;
		for (FFieldTrenchPiecePlacement& Placement : Pieces)
		{
			Placement.Location.Z = GroundHeight;
			// Bends and caps cannot be made shorter; the shortest straight is what is left on rough ground.
			const bool bIsLongStraight = Placement.LengthCm > ShortestStraight &&
				FieldTrenchLayout::GetPieceDefinition(Placement.Piece).Ports.Num() == 2 &&
				FieldTrenchLayout::GetPieceDefinition(Placement.Piece).Ports[1].Yaw == 0.0f;
			if (not bIsLongStraight || not bStraightsLieOnGround)
			{
				continue;
			}
			FFieldTrenchPieceRuntime Probe;
			Probe.Placement = Placement;
			FitPieceToGround(Probe);
			bStraightsLieOnGround = Probe.GroundGapCm <= MaximumGroundGap;
		}
		if (bStraightsLieOnGround)
		{
			return Pieces;
		}
	}
	return Pieces;
}

bool ARTSFieldTrench::TryTraceGround(const FVector2D& GroundPoint, const float AroundHeight, FVector& OutGround) const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector TraceStart(GroundPoint, AroundHeight + RTSFieldTrenchPrivate::GroundTraceHalfHeight);
	const FVector TraceEnd(GroundPoint, AroundHeight - RTSFieldTrenchPrivate::GroundTraceHalfHeight);
	// The trench blocks the landscape channel itself, for whoever looks for the ground on top of it.
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FieldTrenchGround), false, this);
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return false;
	}
	OutGround = GroundHit.ImpactPoint;
	return true;
}

UMaterialInterface* ARTSFieldTrench::GetPlannedMaterial()
{
	if (IsValid(M_PlannedMaterial))
	{
		return M_PlannedMaterial;
	}
	UMaterialInterface* BaseMaterial = PlannedPieceMaterial.LoadSynchronous();
	if (not IsValid(BaseMaterial))
	{
		return nullptr;
	}
	M_PlannedMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	M_PlannedMaterial->SetScalarParameterValue(RTSFieldTrenchPrivate::PlacementOkayParameter, 1.0f);
	return M_PlannedMaterial;
}

void ARTSFieldTrench::ShowPieceState(FFieldTrenchPieceRuntime& Piece)
{
	UStaticMeshComponent* PieceComponent = Piece.Component;
	if (not IsValid(PieceComponent))
	{
		return;
	}
	const bool bIsPlanned = Piece.State == EFieldTrenchPieceState::Planned;
	// A planned piece is only a marking on the ground; nothing collides with it yet.
	PieceComponent->SetCollisionEnabled(bIsPlanned
		? ECollisionEnabled::NoCollision
		: PieceCollisionTemplate->GetCollisionEnabled());
	UMaterialInterface* PlannedMaterial = bIsPlanned ? GetPlannedMaterial() : nullptr;
	const int32 MaterialCount = PieceComponent->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		// Without an override the slot shows the mesh's own material again.
		PieceComponent->SetMaterial(MaterialIndex, PlannedMaterial);
	}
	const float SinkDepth = Piece.State == EFieldTrenchPieceState::Digging
		? (1.0f - Piece.DigProgress) * DigStartDepth
		: 0.0f;
	FTransform ShownTransform = Piece.GroundTransform;
	ShownTransform.AddToTranslation(-Piece.GroundTransform.GetRotation().GetUpVector() * SinkDepth);
	PieceComponent->SetWorldTransform(ShownTransform);
}

int32 ARTSFieldTrench::FindPieceToDig() const
{
	return M_Pieces.IndexOfByPredicate([](const FFieldTrenchPieceRuntime& Piece)
	{
		return Piece.State != EFieldTrenchPieceState::Built;
	});
}

void ARTSFieldTrench::TickDigging(const float DeltaSeconds)
{
	const int32 PieceIndex = FindPieceToDig();
	if (PieceIndex == INDEX_NONE)
	{
		SetActorTickEnabled(false);
		return;
	}
	FFieldTrenchPieceRuntime& Piece = M_Pieces[PieceIndex];
	if (Piece.State == EFieldTrenchPieceState::Planned)
	{
		M_TimeUntilResourceRetry -= DeltaSeconds;
		if (M_TimeUntilResourceRetry > 0.0f)
		{
			return;
		}
		if (not TryPayForPiece(Piece))
		{
			bM_IsWaitingForResources = true;
			M_TimeUntilResourceRetry = ResourceRetrySeconds;
			return;
		}
		bM_IsWaitingForResources = false;
		Piece.State = EFieldTrenchPieceState::Digging;
		Piece.DigProgress = 0.0f;
	}
	const float DigSeconds = DigSecondsPerMetre * Piece.Placement.LengthCm / 100.0f;
	Piece.DigProgress = DigSeconds > KINDA_SMALL_NUMBER
		? FMath::Min(1.0f, Piece.DigProgress + DeltaSeconds / DigSeconds)
		: 1.0f;
	if (Piece.DigProgress >= 1.0f)
	{
		Piece.State = EFieldTrenchPieceState::Built;
		OnPieceBuilt(Piece);
		return;
	}
	ShowPieceState(Piece);
}

bool ARTSFieldTrench::TryPayForPiece(const FFieldTrenchPieceRuntime& Piece) const
{
	const TMap<ERTSResourceType, int32> PieceCost = GetCostOfLength(M_CostPerMetre, Piece.Placement.LengthCm);
	if (PieceCost.IsEmpty())
	{
		return true;
	}
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(ResourceManager))
	{
		return false;
	}
	// Checked first: paying reports every shortfall to the player, and a waiting trench asks twice a second.
	if (ResourceManager->GetCanPayForCost(PieceCost) != EPlayerError::Error_None)
	{
		return false;
	}
	return ResourceManager->PayForCosts(PieceCost);
}

void ARTSFieldTrench::RefundPiece(const FFieldTrenchPieceRuntime& Piece) const
{
	const TMap<ERTSResourceType, int32> PieceCost = GetCostOfLength(M_CostPerMetre, Piece.Placement.LengthCm);
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (PieceCost.IsEmpty() || not IsValid(ResourceManager))
	{
		return;
	}
	ResourceManager->RefundCosts(PieceCost);
}

void ARTSFieldTrench::OnPieceBuilt(FFieldTrenchPieceRuntime& Piece)
{
	ShowPieceState(Piece);
	ScheduleCoverRefresh();
}

void ARTSFieldTrench::ScheduleCoverRefresh()
{
	if (bM_CoverRefreshScheduled || not HasActorBegunPlay())
	{
		return;
	}
	bM_CoverRefreshScheduled = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &ARTSFieldTrench::RefreshCover);
}

void ARTSFieldTrench::RefreshCover()
{
	bM_CoverRefreshScheduled = false;
	const UWorld* World = GetWorld();
	URTSCoverFinderWorldSubsystem* CoverSubsystem = IsValid(World)
		? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		: nullptr;
	// No cover finder means a world without infantry cover, such as an editor preview.
	if (IsValid(CoverSubsystem))
	{
		CoverSubsystem->RefreshCoverSocketsOfActor(this);
	}
}
