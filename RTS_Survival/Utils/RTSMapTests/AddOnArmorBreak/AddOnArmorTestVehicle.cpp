// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorTestVehicle.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorTestVehicle, Log, All);

namespace AddOnArmorTestVehicleConstants
{
	const TCHAR* const HullMeshPath = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Meshes/SM_PanzerIV_Hull.SM_PanzerIV_Hull");
	const TCHAR* const TracksMeshPath = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Meshes/SK_PanzerIV_Tracks.SK_PanzerIV_Tracks");
	const TCHAR* const TurretMeshPath = TEXT("/Game/RTS_Survival/Blueprints/Turrets/German/Medium/PanzerIVTurret/Meshes/SK_PanzerIV_Turret.SK_PanzerIV_Turret");
	const FName TurretSocket(TEXT("Turret"));

	// Far above the leaves' damage threshold: an external strain this high always releases the piece.
	constexpr float ReleaseStrain = 1.0e9f;
	// A leaf's rest position must sit this close to the layout's piece centre to be matched to it.
	constexpr float LeafMatchTolerance = 1.f;

	// Spread of the push around the shell's direction, and the upward kick that throws plates off the rail.
	constexpr float MinPushFraction = 0.55f;
	constexpr float LateralSpreadFraction = 0.3f;
	constexpr float ShellFollowFraction = 0.35f;
	constexpr float OutwardThrowFraction = 0.75f;
	constexpr float MinUpKick = 120.f;
	constexpr float MaxUpKick = 320.f;
	// Pieces further from the shell's line get less of the push, down to this fraction.
	constexpr float PushFalloffFloor = 0.5f;
	constexpr float MinSpinSpeed = 4.f;
	constexpr float MaxSpinSpeed = 14.f;
	// A hit counts only when the nearest intact piece's edge is this close to the shell's line.
	constexpr float MissMargin = 10.f;
	constexpr float MinDeltaSeconds = 1.e-4f;
	// Spike: a landed piece rests within this height of the ground; further below it fell through.
	constexpr float LandedHeightTolerance = 30.f;
	constexpr float FellThroughDepth = 50.f;
	// A released piece closer than this to where it hung did not leave the hull.
	constexpr float StuckOnHullDistance = 10.f;
	// A burst's particles are counted this long after it started.
	constexpr float BurstCheckAge = 0.1f;
	constexpr int32 RightSide = 0;
	constexpr int32 LeftSide = 1;
}

AAddOnArmorTestVehicle::AAddOnArmorTestVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	USceneComponent* const Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
	CreateVehicleMeshes();
}

void AAddOnArmorTestVehicle::CreateVehicleMeshes()
{
	M_Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	M_Hull->SetupAttachment(GetRootComponent());
	M_Tracks = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Tracks"));
	M_Tracks->SetupAttachment(GetRootComponent());
	M_Turret = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Turret"));
	M_Turret->SetupAttachment(M_Hull, AddOnArmorTestVehicleConstants::TurretSocket);
	for (UPrimitiveComponent* const Mesh : TArray<UPrimitiveComponent*>{M_Hull, M_Tracks, M_Turret})
	{
		// A stand-in: nothing hits or walks on it, and the skirts' cost is what is measured.
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetGenerateOverlapEvents(false);
	}
}

bool AAddOnArmorTestVehicle::SetupSkirts(const EAddOnArmorTestTechnique Technique,
                                         const EAddOnArmorTestGcCollision GcCollision, const float GroundHeight)
{
	using namespace AddOnArmorTestVehicleConstants;
	M_Technique = Technique;
	M_GcCollision = GcCollision;
	M_GroundHeight = GroundHeight;
	M_Hull->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, HullMeshPath));
	M_Tracks->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TracksMeshPath));
	M_Turret->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TurretMeshPath));
	M_Turret->AttachToComponent(M_Hull, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TurretSocket);
	if (Technique == EAddOnArmorTestTechnique::None)
	{
		return true;
	}
	if (not LoadPieceLayout() || not LoadSkirtAssets())
	{
		return false;
	}
	if (GetIsBakedTechnique(Technique))
	{
		return SetupBakedHull() && GetIsValidDebrisSystem();
	}
	CreateRails();
	if (Technique == EAddOnArmorTestTechnique::Niagara)
	{
		CreateShardInstances();
		return GetIsValidDebrisSystem();
	}
	return CreateSideCollections();
}

bool AAddOnArmorTestVehicle::LoadPieceLayout()
{
	using namespace AddOnArmorBreakTest;
	const FString LayoutPath = FPaths::ProjectDir() / LayoutFile;
	FString LayoutText;
	TSharedPtr<FJsonObject> Layout;
	if (not FFileHelper::LoadFileToString(LayoutText, *LayoutPath)
		|| not FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LayoutText), Layout) || not Layout.IsValid())
	{
		UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR cannot read the layout %s"), *LayoutPath);
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>& Shards = Layout->GetArrayField(TEXT("shards"));
	if (Shards.Num() != ShardCount)
	{
		UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR layout has %d shards, expected %d"),
		       Shards.Num(), ShardCount);
		return false;
	}
	M_Pieces.Reset();
	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		const FTransform SideTransform = GetSideTransform(SideIndex);
		for (int32 PanelIndex = 0; PanelIndex < PanelsPerSide; ++PanelIndex)
		{
			for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
			{
				const TSharedPtr<FJsonObject> Shard = Shards[ShardIndex]->AsObject();
				const TArray<TSharedPtr<FJsonValue>>& Centroid = Shard->GetArrayField(TEXT("centroid"));
				const FVector CentroidInPanel(Centroid[0]->AsNumber(), Centroid[1]->AsNumber(), Centroid[2]->AsNumber());
				float Radius = 0.f;
				for (const TSharedPtr<FJsonValue>& Corner : Shard->GetArrayField(TEXT("outline")))
				{
					const TArray<TSharedPtr<FJsonValue>>& CornerXZ = Corner->AsArray();
					const FVector CornerInPanel(CornerXZ[0]->AsNumber(), 0.f, CornerXZ[1]->AsNumber());
					Radius = FMath::Max(Radius, static_cast<float>(FVector::Dist(CornerInPanel, CentroidInPanel)));
				}
				FAddOnArmorTestPiece& Piece = M_Pieces.AddDefaulted_GetRef();
				Piece.SideIndex = SideIndex;
				Piece.PanelIndex = PanelIndex;
				Piece.ShardIndex = ShardIndex;
				Piece.Radius = Radius;
				const FTransform PieceInSide(GetPanelTopCentre(PanelIndex) + CentroidInPanel);
				Piece.VehicleTransform = PieceInSide * SideTransform;
			}
		}
	}
	return true;
}

bool AAddOnArmorTestVehicle::LoadSkirtAssets()
{
	using namespace AddOnArmorBreakTest;
	M_ShardMeshes.Reset();
	for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
	{
		UStaticMesh* const ShardMesh = LoadObject<UStaticMesh>(
			nullptr, *GetShardMeshPath(ShardIndex));
		if (not IsValid(ShardMesh))
		{
			UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR missing shard mesh %d"), ShardIndex);
			return false;
		}
		M_ShardMeshes.Add(ShardMesh);
	}
	if (GetUsesNiagaraDebris(M_Technique))
	{
		M_DebrisSystem = LoadObject<UNiagaraSystem>(nullptr, NiagaraSystemPath);
	}
	return true;
}

bool AAddOnArmorTestVehicle::SetupBakedHull()
{
	using namespace AddOnArmorBreakTest;
	UStaticMesh* const BakedHull = LoadObject<UStaticMesh>(nullptr, BakedHullMeshPath);
	const TCHAR* MaterialPath = OpaqueSkirtMaterialPath;
	if (M_Technique == EAddOnArmorTestTechnique::BakedMask)
	{
		MaterialPath = MaskSkirtMaterialPath;
	}
	else if (M_Technique == EAddOnArmorTestTechnique::BakedWpo)
	{
		MaterialPath = WpoSkirtMaterialPath;
	}
	UMaterialInterface* const SkirtMaterial = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (not IsValid(BakedHull) || not IsValid(SkirtMaterial))
	{
		UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR missing baked hull or skirt material %s"), MaterialPath);
		return false;
	}
	M_Hull->SetStaticMesh(BakedHull);
	M_Turret->AttachToComponent(M_Hull, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
	                            AddOnArmorTestVehicleConstants::TurretSocket);
	const int32 SkirtSlot = M_Hull->GetMaterialIndex(SkirtMaterialSlot);
	if (SkirtSlot == INDEX_NONE)
	{
		UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR baked hull has no %s slot"), *SkirtMaterialSlot.ToString());
		return false;
	}
	M_Hull->SetMaterial(SkirtSlot, SkirtMaterial);
	for (int32 WordIndex = 0; WordIndex < MaskWordCount; ++WordIndex)
	{
		M_Hull->SetCustomPrimitiveDataFloat(WordIndex, 0.f);
	}
	return true;
}

void AAddOnArmorTestVehicle::CreateRails()
{
	using namespace AddOnArmorBreakTest;
	UStaticMesh* const RailMesh = LoadObject<UStaticMesh>(nullptr, RailMeshPath);
	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		UStaticMeshComponent* const Rail = NewObject<UStaticMeshComponent>(this);
		Rail->SetStaticMesh(RailMesh);
		Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rail->SetCanEverAffectNavigation(false);
		Rail->SetReceivesDecals(false);
		Rail->SetupAttachment(GetRootComponent());
		Rail->SetRelativeTransform(FTransform(FVector(0.f, -RailInsetY, 0.f)) * GetSideTransform(SideIndex));
		Rail->RegisterComponent();
		M_Rails.Add(Rail);
	}
}

void AAddOnArmorTestVehicle::CreateShardInstances()
{
	M_ShardInstances.Reset();
	for (UStaticMesh* const ShardMesh : M_ShardMeshes)
	{
		UInstancedStaticMeshComponent* const Instances = NewObject<UInstancedStaticMeshComponent>(this);
		Instances->SetStaticMesh(ShardMesh);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetCanEverAffectNavigation(false);
		Instances->SetReceivesDecals(false);
		Instances->SetGenerateOverlapEvents(false);
		Instances->SetupAttachment(GetRootComponent());
		Instances->RegisterComponent();
		M_ShardInstances.Add(Instances);
	}
	M_PieceInstanceIndices.Reset();
	for (const FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		UInstancedStaticMeshComponent* const Instances = M_ShardInstances[Piece.ShardIndex];
		M_PieceInstanceIndices.Add(Instances->AddInstance(Piece.VehicleTransform, false));
	}
}

bool AAddOnArmorTestVehicle::CreateSideCollections()
{
	using namespace AddOnArmorBreakTest;
	UGeometryCollection* const Collection = LoadObject<UGeometryCollection>(nullptr, CollectionPath);
	if (not IsValid(Collection))
	{
		UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR missing geometry collection %s"), CollectionPath);
		return false;
	}
	if (not MapCollectionLeavesToPieces(*Collection))
	{
		return false;
	}
	M_SideCollections.Reset();
	M_SideCollisionEnabled.Init(M_GcCollision == EAddOnArmorTestGcCollision::FromStart, SideCount);
	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		M_SideCollections.Add(CreateSideCollection(SideIndex, *Collection));
	}
	return true;
}

UGeometryCollectionComponent* AAddOnArmorTestVehicle::CreateSideCollection(const int32 SideIndex,
                                                                           UGeometryCollection& Collection)
{
	using namespace AddOnArmorTestVehicleConstants;
	UGeometryCollectionComponent* const CollectionComponent = NewObject<UGeometryCollectionComponent>(this);
	// Dynamic with every leaf anchored: the root then stays kinematic and follows the hull, and a leaf that is
	// unanchored and broken off falls. A kinematic collection keeps its released leaves kinematic.
	CollectionComponent->ObjectType = EObjectStateTypeEnum::Chaos_Object_Dynamic;
	CollectionComponent->bEnableDamageFromCollision = false;
	// The plates are 0.8 cm thin: without swept collision most released pieces tunnel through the landscape
	// (spike, 2026-11-04: 9 of 18 with async physics, 16 of 18 with 33 ms synchronous steps).
	CollectionComponent->BodyInstance.bUseCCD = true;
	CollectionComponent->SetCanEverAffectNavigation(false);
	CollectionComponent->SetReceivesDecals(false);
	CollectionComponent->SetGenerateOverlapEvents(false);
	// Collision must be set before the physics state is made; it is read when the proxy creates its particles.
	ConfigureCollectionCollision(*CollectionComponent, M_GcCollision == EAddOnArmorTestGcCollision::FromStart);
	CollectionComponent->SetRestCollection(&Collection);
	CollectionComponent->SetNotifyBreaks(true);
	if (SideIndex == RightSide)
	{
		CollectionComponent->OnChaosBreakEvent.AddDynamic(this, &AAddOnArmorTestVehicle::OnRightSideBreak);
	}
	else
	{
		CollectionComponent->OnChaosBreakEvent.AddDynamic(this, &AAddOnArmorTestVehicle::OnLeftSideBreak);
	}
	CollectionComponent->SetupAttachment(GetRootComponent());
	CollectionComponent->SetRelativeTransform(AddOnArmorBreakTest::GetSideTransform(SideIndex));
	CollectionComponent->RegisterComponent();
	return CollectionComponent;
}

void AAddOnArmorTestVehicle::ConfigureCollectionCollision(UGeometryCollectionComponent& CollectionComponent,
                                                          const bool bEnable) const
{
	if (not bEnable)
	{
		CollectionComponent.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	CollectionComponent.SetCollisionObjectType(ECC_Destructible);
	CollectionComponent.SetCollisionResponseToAllChannels(ECR_Ignore);
	// Released pieces land on the ground only; shells, traces and units never see them. Pieces ignore each other:
	// a released plate touches the anchored ones at its cut edges, and pushing it out of them drove it into the ground.
	CollectionComponent.SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollectionComponent.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

bool AAddOnArmorTestVehicle::MapCollectionLeavesToPieces(const UGeometryCollection& Collection)
{
	using namespace AddOnArmorTestVehicleConstants;
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> Data = Collection.GetGeometryCollection();
	if (not Data.IsValid())
	{
		return false;
	}
	TArray<FTransform> GlobalTransforms;
	GeometryCollectionAlgo::GlobalMatrices(Data->Transform, Data->Parent, GlobalTransforms);
	int32 MatchedLeaves = 0;
	for (int32 TransformIndex = 0; TransformIndex < GlobalTransforms.Num(); ++TransformIndex)
	{
		if (Data->Children[TransformIndex].Num() > 0)
		{
			continue;
		}
		const FVector LeafInSide = GlobalTransforms[TransformIndex].GetLocation();
		for (FAddOnArmorTestPiece& Piece : M_Pieces)
		{
			const FVector PieceInSide = AddOnArmorBreakTest::GetSideTransform(Piece.SideIndex).InverseTransformPosition(
				Piece.VehicleTransform.GetLocation());
			if (FVector::Dist(LeafInSide, PieceInSide) > LeafMatchTolerance)
			{
				continue;
			}
			// Both sides use the same collection, so a leaf belongs to the piece of that panel and shard on each side.
			Piece.CollectionTransformIndex = TransformIndex;
			++MatchedLeaves;
		}
	}
	const int32 ExpectedMatches = M_Pieces.Num();
	UE_LOG(LogRTSAddOnArmorTestVehicle, Display, TEXT("RTS_ADDON_ARMOR collection leaves matched=%d expected=%d transforms=%d"),
	       MatchedLeaves, ExpectedMatches, GlobalTransforms.Num());
	return MatchedLeaves == ExpectedMatches;
}

void AAddOnArmorTestVehicle::StartDriving(const FVector& CircleCentre, const float CircleRadius, const float Speed,
                                          const float StartAngle)
{
	M_Drive.bIsDriving = CircleRadius > 0.f;
	M_Drive.CircleCentre = CircleCentre;
	M_Drive.CircleRadius = CircleRadius;
	M_Drive.AngularSpeed = CircleRadius > 0.f ? Speed / CircleRadius : 0.f;
	M_Drive.Angle = StartAngle;
}

void AAddOnArmorTestVehicle::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not M_Drive.bIsDriving || DeltaSeconds < AddOnArmorTestVehicleConstants::MinDeltaSeconds)
	{
		M_Velocity = FVector::ZeroVector;
		return;
	}
	M_Drive.Angle += M_Drive.AngularSpeed * DeltaSeconds;
	const FVector Offset(FMath::Cos(M_Drive.Angle), FMath::Sin(M_Drive.Angle), 0.f);
	const FVector NewLocation = M_Drive.CircleCentre + Offset * M_Drive.CircleRadius;
	// Driving counter-clockwise: the vehicle faces along the circle's tangent.
	const float HeadingDegrees = FMath::RadiansToDegrees(M_Drive.Angle) + 90.f;
	M_Velocity = (NewLocation - GetActorLocation()) / DeltaSeconds;
	SetActorLocationAndRotation(NewLocation, FRotator(0.f, HeadingDegrees, 0.f));
}

int32 AAddOnArmorTestVehicle::ApplyTestHit(const FAddOnArmorTestHit& Hit)
{
	++M_Stats.HitCount;
	if (M_Technique == EAddOnArmorTestTechnique::None)
	{
		return 0;
	}
	TArray<int32> PieceIndices;
	SelectPiecesForHit(Hit, PieceIndices);
	if (PieceIndices.IsEmpty())
	{
		++M_Stats.HitsThatMissed;
		return 0;
	}
	const TArray<FAddOnArmorTestPieceRelease> Releases = MakeReleases(Hit, PieceIndices);
	for (const FAddOnArmorTestPieceRelease& Release : Releases)
	{
		M_Pieces[Release.PieceIndex].bIsBroken = true;
		M_Pieces[Release.PieceIndex].ReleaseSeconds = GetGameSeconds();
	}
	if (GetUsesNiagaraDebris(M_Technique))
	{
		ReleaseWithNiagara(Releases);
	}
	else
	{
		ReleaseWithGeometryCollection(Releases);
	}
	M_Stats.PiecesReleased += Releases.Num();
	return Releases.Num();
}

void AAddOnArmorTestVehicle::SelectPiecesForHit(const FAddOnArmorTestHit& Hit, TArray<int32>& OutPieceIndices) const
{
	struct FCandidate
	{
		int32 PieceIndex;
		float DistanceToLine;
	};
	TArray<FCandidate> Candidates;
	float NearestEdgeDistance = TNumericLimits<float>::Max();
	int32 NearestPiece = INDEX_NONE;
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		const FAddOnArmorTestPiece& Piece = M_Pieces[PieceIndex];
		if (Piece.bIsBroken)
		{
			continue;
		}
		const FVector Centre = GetPieceWorldTransform(Piece).GetLocation();
		const float DistanceToLine = static_cast<float>(FMath::PointDistToLine(Centre, Hit.WorldDirection, Hit.WorldLocation));
		const float EdgeDistance = DistanceToLine - Piece.Radius;
		if (EdgeDistance < NearestEdgeDistance)
		{
			NearestEdgeDistance = EdgeDistance;
			NearestPiece = PieceIndex;
		}
		if (DistanceToLine <= Hit.BreakRadius)
		{
			Candidates.Add({PieceIndex, DistanceToLine});
		}
	}
	if (NearestPiece == INDEX_NONE || NearestEdgeDistance > AddOnArmorTestVehicleConstants::MissMargin)
	{
		return;
	}
	if (Candidates.IsEmpty())
	{
		// A small shell through the middle of a big shard still breaks that shard.
		Candidates.Add({NearestPiece, 0.f});
	}
	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		return Left.DistanceToLine < Right.DistanceToLine;
	});
	const int32 PieceCount = FMath::Min(Candidates.Num(), Hit.MaxPieces);
	for (int32 CandidateIndex = 0; CandidateIndex < PieceCount; ++CandidateIndex)
	{
		OutPieceIndices.Add(Candidates[CandidateIndex].PieceIndex);
	}
}

TArray<FAddOnArmorTestPieceRelease> AAddOnArmorTestVehicle::MakeReleases(const FAddOnArmorTestHit& Hit,
                                                                         const TArray<int32>& PieceIndices) const
{
	using namespace AddOnArmorTestVehicleConstants;
	TArray<FAddOnArmorTestPieceRelease> Releases;
	const FVector Lateral = FVector::CrossProduct(Hit.WorldDirection, FVector::UpVector).GetSafeNormal();
	for (const int32 PieceIndex : PieceIndices)
	{
		const FAddOnArmorTestPiece& Piece = M_Pieces[PieceIndex];
		// The same seed and piece always give the same push, whichever technique breaks the piece.
		FRandomStream Stream(HashCombine(GetTypeHash(Hit.Seed), GetTypeHash(PieceIndex)));
		const FVector Centre = GetPieceWorldTransform(Piece).GetLocation();
		const float DistanceToLine = static_cast<float>(FMath::PointDistToLine(Centre, Hit.WorldDirection, Hit.WorldLocation));
		const float Falloff = FMath::Lerp(1.f, PushFalloffFloor,
		                                  FMath::Clamp(DistanceToLine / FMath::Max(Hit.BreakRadius, 1.f), 0.f, 1.f));
		const float Push = Hit.PushSpeed * Falloff * Stream.FRandRange(MinPushFraction, 1.f);
		FAddOnArmorTestPieceRelease& Release = Releases.AddDefaulted_GetRef();
		Release.PieceIndex = PieceIndex;
		// Torn skirt plates leave the hull: a little of the push follows the shell, most throws them off the face.
		Release.PushVelocity = Hit.WorldDirection * Push * ShellFollowFraction
			+ GetSideOutwardNormal(Piece.SideIndex) * Push * OutwardThrowFraction
			+ Lateral * Hit.PushSpeed * LateralSpreadFraction * Stream.FRandRange(-1.f, 1.f)
			+ FVector::UpVector * Stream.FRandRange(MinUpKick, MaxUpKick);
		Release.SpinAxis = Stream.GetUnitVector();
		Release.SpinSpeed = Stream.FRandRange(MinSpinSpeed, MaxSpinSpeed);
	}
	return Releases;
}

void AAddOnArmorTestVehicle::ReleaseWithNiagara(const TArray<FAddOnArmorTestPieceRelease>& Releases)
{
	using namespace AddOnArmorBreakTest;
	if (not GetIsValidDebrisSystem())
	{
		return;
	}
	TArray<FVector4> Starts;
	TArray<FVector4> Velocities;
	TArray<FVector4> Spins;
	TArray<FVector4> Orientations;
	FVector BurstCentre = FVector::ZeroVector;
	for (const FAddOnArmorTestPieceRelease& Release : Releases)
	{
		const FAddOnArmorTestPiece& Piece = M_Pieces[Release.PieceIndex];
		const FTransform PieceWorld = GetPieceWorldTransform(Piece);
		const FQuat Rotation = PieceWorld.GetRotation();
		Starts.Add(FVector4(PieceWorld.GetLocation(), static_cast<float>(Piece.ShardIndex)));
		// The pieces leave with the vehicle's own velocity, as the released geometry collection leaves do.
		Velocities.Add(FVector4(Release.PushVelocity + M_Velocity, 0.f));
		Spins.Add(FVector4(Release.SpinAxis, Release.SpinSpeed));
		Orientations.Add(FVector4(Rotation.X, Rotation.Y, Rotation.Z, Rotation.W));
		BurstCentre += PieceWorld.GetLocation();
		HideIntactPiece(Release.PieceIndex);
	}
	BurstCentre /= Releases.Num();
	UNiagaraComponent* const Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), M_DebrisSystem, BurstCentre, FRotator::ZeroRotator, FVector::OneVector, false, false,
		ENCPoolMethod::AutoRelease, false);
	if (not IsValid(Burst))
	{
		return;
	}
	for (int32 ShardIndex = 0; ShardIndex < M_ShardMeshes.Num(); ++ShardIndex)
	{
		// The user parameter picks the piece meshes; another armour kind would pass its own pieces here.
		Burst->SetVariableStaticMesh(GetDebrisMeshParameter(ShardIndex), M_ShardMeshes[ShardIndex]);
	}
	Burst->SetVariableInt(PieceCountParameter, FMath::Min(Releases.Num(), MaxPiecesPerBurst));
	Burst->SetVariableFloat(GroundHeightParameter, M_GroundHeight);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceStartParameter, Starts);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceVelocityParameter, Velocities);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceSpinParameter, Spins);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceOrientationParameter, Orientations);
	Burst->Activate(true);
	++M_Stats.NiagaraBursts;
	const float Now = GetGameSeconds();
	M_RecentBursts.RemoveAll([Now](const FRecentBurst& Recent)
	{
		return Now - Recent.SpawnSeconds > AddOnArmorBreakTest::DebrisSecondsBeforeRemoval
			+ AddOnArmorBreakTest::DebrisRemovalSeconds;
	});
	FRecentBurst& Recent = M_RecentBursts.AddDefaulted_GetRef();
	Recent.Component = Burst;
	Recent.ExpectedParticles = FMath::Min(Releases.Num(), MaxPiecesPerBurst);
	Recent.SpawnSeconds = Now;
}

void AAddOnArmorTestVehicle::HideIntactPiece(const int32 PieceIndex)
{
	if (GetIsBakedTechnique(M_Technique))
	{
		MarkPieceBrokenInMask(PieceIndex);
		return;
	}
	HideShardInstance(PieceIndex);
}

void AAddOnArmorTestVehicle::MarkPieceBrokenInMask(const int32 PieceIndex)
{
	using namespace AddOnArmorBreakTest;
	// The baked mesh's shard ids follow the piece order (side, panel, shard).
	const int32 WordIndex = PieceIndex / ShardsPerMaskWord;
	if (WordIndex < 0 || WordIndex >= MaskWordCount)
	{
		return;
	}
	M_BrokenMask[WordIndex] |= 1u << (PieceIndex % ShardsPerMaskWord);
	M_Hull->SetCustomPrimitiveDataFloat(WordIndex, static_cast<float>(M_BrokenMask[WordIndex]));
}

void AAddOnArmorTestVehicle::HideShardInstance(const int32 PieceIndex)
{
	const FAddOnArmorTestPiece& Piece = M_Pieces[PieceIndex];
	if (not M_ShardInstances.IsValidIndex(Piece.ShardIndex) || not M_PieceInstanceIndices.IsValidIndex(PieceIndex))
	{
		return;
	}
	// Zero scale keeps every other instance's index stable.
	FTransform Hidden = Piece.VehicleTransform;
	Hidden.SetScale3D(FVector::ZeroVector);
	M_ShardInstances[Piece.ShardIndex]->UpdateInstanceTransform(M_PieceInstanceIndices[PieceIndex], Hidden, false, true, true);
}

void AAddOnArmorTestVehicle::ReleaseWithGeometryCollection(const TArray<FAddOnArmorTestPieceRelease>& Releases)
{
	using namespace AddOnArmorTestVehicleConstants;
	for (const FAddOnArmorTestPieceRelease& Release : Releases)
	{
		const FAddOnArmorTestPiece& Piece = M_Pieces[Release.PieceIndex];
		UGeometryCollectionComponent* const Collection = M_SideCollections.IsValidIndex(Piece.SideIndex)
			                                                 ? M_SideCollections[Piece.SideIndex].Get()
			                                                 : nullptr;
		if (not IsValid(Collection) || Piece.CollectionTransformIndex == INDEX_NONE)
		{
			continue;
		}
		if (not M_SideCollisionEnabled[Piece.SideIndex])
		{
			ConfigureCollectionCollision(*Collection, true);
			M_SideCollisionEnabled[Piece.SideIndex] = true;
		}
		const FVector Centre = GetPieceWorldTransform(Piece).GetLocation();
		Collection->SetAnchoredByIndex(Piece.CollectionTransformIndex, false);
		Collection->ApplyExternalStrain(Piece.CollectionTransformIndex, Centre, 0.f, 0, 1.f, ReleaseStrain);
		// A leaf inside its cluster takes no velocity; the push is applied once its break event arrives.
		M_PendingCollectionReleases.Add(Release.PieceIndex, Release);
	}
}

void AAddOnArmorTestVehicle::OnRightSideBreak(const FChaosBreakEvent& BreakEvent)
{
	HandleBreakEvent(AddOnArmorTestVehicleConstants::RightSide, BreakEvent);
}

void AAddOnArmorTestVehicle::OnLeftSideBreak(const FChaosBreakEvent& BreakEvent)
{
	HandleBreakEvent(AddOnArmorTestVehicleConstants::LeftSide, BreakEvent);
}

void AAddOnArmorTestVehicle::HandleBreakEvent(const int32 SideIndex, const FChaosBreakEvent& BreakEvent)
{
	const int32 PieceIndex = FindPieceByCollectionLeaf(SideIndex, BreakEvent.Index);
	FAddOnArmorTestPieceRelease Release;
	if (PieceIndex == INDEX_NONE || not M_PendingCollectionReleases.RemoveAndCopyValue(PieceIndex, Release))
	{
		++M_Stats.UnexpectedBreakEvents;
		UE_LOG(LogRTSAddOnArmorTestVehicle, Warning, TEXT("RTS_ADDON_ARMOR unexpected break side=%d transform=%d crumble=%d"),
		       SideIndex, BreakEvent.Index, BreakEvent.bFromCrumble ? 1 : 0);
		return;
	}
	M_Pieces[PieceIndex].bHasBreakEvent = true;
	++M_Stats.MatchingBreakEvents;
	UGeometryCollectionComponent* const Collection = M_SideCollections[SideIndex].Get();
	if (not IsValid(Collection))
	{
		return;
	}
	// The released leaf already carries the hull's velocity from its kinematic parent; only the push is added.
	Collection->ApplyLinearVelocity(BreakEvent.Index, Release.PushVelocity);
	Collection->ApplyAngularVelocity(BreakEvent.Index, Release.SpinAxis * Release.SpinSpeed);
}

int32 AAddOnArmorTestVehicle::FindPieceByCollectionLeaf(const int32 SideIndex, const int32 TransformIndex) const
{
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		const FAddOnArmorTestPiece& Piece = M_Pieces[PieceIndex];
		if (Piece.SideIndex == SideIndex && Piece.CollectionTransformIndex == TransformIndex)
		{
			return PieceIndex;
		}
	}
	return INDEX_NONE;
}

int32 AAddOnArmorTestVehicle::CountReleasesWithoutBreakEvent(const float GameSecondsToWait) const
{
	if (M_Technique != EAddOnArmorTestTechnique::GeometryCollection)
	{
		return 0;
	}
	int32 MissingCount = 0;
	const float Now = GetGameSeconds();
	for (const FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		if (Piece.bIsBroken && not Piece.bHasBreakEvent && Now - Piece.ReleaseSeconds > GameSecondsToWait)
		{
			++MissingCount;
		}
	}
	return MissingCount;
}

void AAddOnArmorTestVehicle::CheckReleasedPieces(const float LandingAge, FAddOnArmorTestSpikeCounts& InOutCounts)
{
	using namespace AddOnArmorTestVehicleConstants;
	const float Now = GetGameSeconds();
	for (FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		if (not Piece.bIsBroken || Piece.bIsLandingChecked || Now - Piece.ReleaseSeconds < LandingAge)
		{
			continue;
		}
		Piece.bIsLandingChecked = true;
		FVector LeafLocation;
		if (M_Technique != EAddOnArmorTestTechnique::GeometryCollection || not TryGetCollectionLeafWorldLocation(Piece, LeafLocation))
		{
			continue;
		}
		const FVector HungLocation = GetPieceWorldTransform(Piece).GetLocation();
		const float HeightAboveGround = LeafLocation.Z - M_GroundHeight;
		const float DistanceFromHull = FVector::Dist(LeafLocation, HungLocation);
		if (DistanceFromHull < StuckOnHullDistance)
		{
			++InOutCounts.StuckOnHull;
		}
		else if (HeightAboveGround < -FellThroughDepth)
		{
			++InOutCounts.FellThroughGround;
		}
		else if (HeightAboveGround > LandedHeightTolerance)
		{
			++InOutCounts.StillAirborne;
		}
		else
		{
			++InOutCounts.Landed;
		}
		UE_LOG(LogRTSAddOnArmorTestVehicle, Display, TEXT("RTS_ADDON_ARMOR spike piece side=%d panel=%d shard=%d height=%.1f from_hull=%.1f"),
		       Piece.SideIndex, Piece.PanelIndex, Piece.ShardIndex, HeightAboveGround, DistanceFromHull);
	}
	for (FRecentBurst& Recent : M_RecentBursts)
	{
		if (Recent.bIsChecked || Now - Recent.SpawnSeconds < BurstCheckAge || not Recent.Component.IsValid())
		{
			continue;
		}
		Recent.bIsChecked = true;
		const int32 Particles = CountParticles(*Recent.Component);
		const bool bHasAllPieces = Particles == Recent.ExpectedParticles;
		InOutCounts.BurstsWithAllPieces += bHasAllPieces ? 1 : 0;
		InOutCounts.BurstsWithWrongCount += bHasAllPieces ? 0 : 1;
		LogFirstParticle(*Recent.Component);
		const TArray<FVector4> StoredStarts = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(
			Recent.Component.Get(), AddOnArmorBreakTest::PieceStartParameter);
		UE_LOG(LogRTSAddOnArmorTestVehicle, Display,
		       TEXT("RTS_ADDON_ARMOR spike burst particles=%d expected=%d stored_starts=%d bounds_origin=%s vehicle=%s"),
		       Particles, Recent.ExpectedParticles, StoredStarts.Num(), *Recent.Component->Bounds.Origin.ToCompactString(),
		       *GetActorLocation().ToCompactString());
	}
}

int32 AAddOnArmorTestVehicle::CountDisplacedIntactPieces(const float Tolerance) const
{
	if (M_Technique != EAddOnArmorTestTechnique::GeometryCollection)
	{
		return 0;
	}
	int32 DisplacedCount = 0;
	for (const FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		FVector LeafLocation;
		if (Piece.bIsBroken || not TryGetCollectionLeafWorldLocation(Piece, LeafLocation))
		{
			continue;
		}
		const float Offset = FVector::Dist(LeafLocation, GetPieceWorldTransform(Piece).GetLocation());
		if (Offset > Tolerance)
		{
			++DisplacedCount;
			UE_LOG(LogRTSAddOnArmorTestVehicle, Warning, TEXT("RTS_ADDON_ARMOR intact piece off the hull side=%d panel=%d shard=%d offset=%.1f"),
			       Piece.SideIndex, Piece.PanelIndex, Piece.ShardIndex, Offset);
		}
	}
	return DisplacedCount;
}

int32 AAddOnArmorTestVehicle::CountLiveDebrisPieces() const
{
	const float Now = GetGameSeconds();
	const float Lifetime = AddOnArmorBreakTest::DebrisSecondsBeforeRemoval + AddOnArmorBreakTest::DebrisRemovalSeconds;
	int32 LiveCount = 0;
	for (const FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		LiveCount += Piece.bIsBroken && Now - Piece.ReleaseSeconds < Lifetime ? 1 : 0;
	}
	return LiveCount;
}

void AAddOnArmorTestVehicle::AppendActiveBursts(TSet<const UNiagaraComponent*>& InOutBursts) const
{
	const float Lifetime = AddOnArmorBreakTest::DebrisSecondsBeforeRemoval + AddOnArmorBreakTest::DebrisRemovalSeconds;
	for (const FRecentBurst& Recent : M_RecentBursts)
	{
		// A pooled component may already carry another vehicle's burst; only the newest owner counts it.
		if (Recent.Component.IsValid() && Recent.Component->IsActive() && GetGameSeconds() - Recent.SpawnSeconds < Lifetime)
		{
			InOutBursts.Add(Recent.Component.Get());
		}
	}
}

void AAddOnArmorTestVehicle::LogFirstParticle(const UNiagaraComponent& Burst)
{
	const FNiagaraSystemInstanceControllerConstPtr Controller = Burst.GetSystemInstanceController();
	const FNiagaraSystemInstance* const Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (Instance == nullptr || Instance->GetEmitters().IsEmpty())
	{
		return;
	}
	const FNiagaraDataSet& DataSet = Instance->GetEmitters()[0]->GetParticleData();
	if (DataSet.GetCurrentData() == nullptr || DataSet.GetCurrentData()->GetNumInstances() == 0)
	{
		return;
	}
	const FVector3f Scale = FNiagaraDataSetAccessor<FVector3f>::CreateReader(DataSet, TEXT("Scale")).GetSafe(0, FVector3f(-1.f));
	const int32 MeshIndex = FNiagaraDataSetAccessor<int32>::CreateReader(DataSet, TEXT("MeshIndex")).GetSafe(0, -1);
	const FQuat4f Orientation = FNiagaraDataSetAccessor<FQuat4f>::CreateReader(DataSet, TEXT("MeshOrientation")).GetSafe(0, FQuat4f(0.f, 0.f, 0.f, 0.f));
	const float Lifetime = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, TEXT("Lifetime")).GetSafe(0, -1.f);
	const float Age = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, TEXT("Age")).GetSafe(0, -1.f);
	UE_LOG(LogRTSAddOnArmorTestVehicle, Display, TEXT("RTS_ADDON_ARMOR spike particle scale=%s mesh_index=%d orientation=%s lifetime=%.2f age=%.2f"),
	       *Scale.ToString(), MeshIndex, *Orientation.ToString(), Lifetime, Age);
}

int32 AAddOnArmorTestVehicle::CountParticles(const UNiagaraComponent& Burst)
{
	const FNiagaraSystemInstanceControllerConstPtr Controller = Burst.GetSystemInstanceController();
	const FNiagaraSystemInstance* const Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (Instance == nullptr)
	{
		return 0;
	}
	int32 ParticleCount = 0;
	for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
	{
		ParticleCount += Emitter->GetNumParticles();
	}
	return ParticleCount;
}

bool AAddOnArmorTestVehicle::TryGetCollectionLeafWorldLocation(const FAddOnArmorTestPiece& Piece, FVector& OutLocation) const
{
	UGeometryCollectionComponent* const Collection = M_SideCollections.IsValidIndex(Piece.SideIndex)
		                                                 ? M_SideCollections[Piece.SideIndex].Get()
		                                                 : nullptr;
	if (not IsValid(Collection) || Piece.CollectionTransformIndex == INDEX_NONE)
	{
		return false;
	}
	const TArray<FTransform3f>& LeafTransforms = Collection->GetComponentSpaceTransforms3f();
	if (not LeafTransforms.IsValidIndex(Piece.CollectionTransformIndex))
	{
		return false;
	}
	const FVector LeafInComponent(LeafTransforms[Piece.CollectionTransformIndex].GetLocation());
	OutLocation = Collection->GetComponentTransform().TransformPosition(LeafInComponent);
	return true;
}

bool AAddOnArmorTestVehicle::PickIntactPieceLocation(const int32 SideIndex, FRandomStream& Stream,
                                                      FVector& OutLocation) const
{
	TArray<int32> IntactPieces;
	for (int32 PieceIndex = 0; PieceIndex < M_Pieces.Num(); ++PieceIndex)
	{
		if (M_Pieces[PieceIndex].SideIndex == SideIndex && not M_Pieces[PieceIndex].bIsBroken)
		{
			IntactPieces.Add(PieceIndex);
		}
	}
	if (IntactPieces.IsEmpty())
	{
		return false;
	}
	const int32 PickedPiece = IntactPieces[Stream.RandRange(0, IntactPieces.Num() - 1)];
	OutLocation = GetPieceWorldTransform(M_Pieces[PickedPiece]).GetLocation();
	return true;
}

FVector AAddOnArmorTestVehicle::GetSideOutwardNormal(const int32 SideIndex) const
{
	return GetActorRotation().RotateVector(SideIndex == AddOnArmorTestVehicleConstants::RightSide
		                                       ? FVector::RightVector
		                                       : FVector::LeftVector);
}

int32 AAddOnArmorTestVehicle::GetIntactPieceCount() const
{
	int32 IntactCount = 0;
	for (const FAddOnArmorTestPiece& Piece : M_Pieces)
	{
		IntactCount += Piece.bIsBroken ? 0 : 1;
	}
	return IntactCount;
}

FTransform AAddOnArmorTestVehicle::GetPieceWorldTransform(const FAddOnArmorTestPiece& Piece) const
{
	return Piece.VehicleTransform * GetActorTransform();
}

float AAddOnArmorTestVehicle::GetGameSeconds() const
{
	const UWorld* const World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

bool AAddOnArmorTestVehicle::GetIsValidDebrisSystem() const
{
	if (IsValid(M_DebrisSystem))
	{
		return true;
	}
	UE_LOG(LogRTSAddOnArmorTestVehicle, Error, TEXT("RTS_ADDON_ARMOR M_DebrisSystem not loaded (%s)"),
	       AddOnArmorBreakTest::NiagaraSystemPath);
	return false;
}

#else

AAddOnArmorTestVehicle::AAddOnArmorTestVehicle()
{
}

void AAddOnArmorTestVehicle::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

bool AAddOnArmorTestVehicle::SetupSkirts(EAddOnArmorTestTechnique, EAddOnArmorTestGcCollision, float)
{
	return false;
}

void AAddOnArmorTestVehicle::StartDriving(const FVector&, float, float, float)
{
}

int32 AAddOnArmorTestVehicle::ApplyTestHit(const FAddOnArmorTestHit&)
{
	return 0;
}

bool AAddOnArmorTestVehicle::PickIntactPieceLocation(int32, FRandomStream&, FVector&) const
{
	return false;
}

FVector AAddOnArmorTestVehicle::GetSideOutwardNormal(int32) const
{
	return FVector::ZeroVector;
}

int32 AAddOnArmorTestVehicle::GetIntactPieceCount() const
{
	return 0;
}

int32 AAddOnArmorTestVehicle::CountReleasesWithoutBreakEvent(float) const
{
	return 0;
}

void AAddOnArmorTestVehicle::CheckReleasedPieces(float, FAddOnArmorTestSpikeCounts&)
{
}

int32 AAddOnArmorTestVehicle::CountDisplacedIntactPieces(float) const
{
	return 0;
}

int32 AAddOnArmorTestVehicle::CountLiveDebrisPieces() const
{
	return 0;
}

void AAddOnArmorTestVehicle::AppendActiveBursts(TSet<const UNiagaraComponent*>&) const
{
}

int32 AAddOnArmorTestVehicle::CountParticles(const UNiagaraComponent&)
{
	return 0;
}

void AAddOnArmorTestVehicle::OnRightSideBreak(const FChaosBreakEvent&)
{
}

void AAddOnArmorTestVehicle::OnLeftSideBreak(const FChaosBreakEvent&)
{
}

#endif // RTS_WITH_MAP_TESTS
