#include "DestroyedBuildingGenerator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GeneratedRotationBounds/GeneratedRotationBounds.h"
#include "Math/UnrealMathUtility.h"

ADestroyedBuildingGenerator::ADestroyedBuildingGenerator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADestroyedBuildingGenerator::BeginPlay()
{
	Super::BeginPlay();
}

void ADestroyedBuildingGenerator::EnsureAllComponentsAreCleared()
{
	for(auto EachHism : Hism_Components)
	{
		if(IsValid(EachHism))
		{
			EachHism->ClearInstances();
		}
	}
}

void ADestroyedBuildingGenerator::GenerateSquareRoom(
	const FVector LocalRoomCenter /*= FVector(0,0,0)*/,
	const float IndividualWallXLength /*= 150.0f*/,
	const float RoomTotalWallLength  /*= 500.0f*/,
	const bool bHasRoof /*= true*/,
	FGeneratedRotationBounds RoofRotationBounds /*= {}*/,
	const float IndividualRoofXLength /*= 300.0f*/,
	const float IndividualRoofYLength /*= 100.0f*/,
	const float RoofHeight /*= 300.0f*/
)
{
	// 1) Generate the four walls
	GenerateWalls(LocalRoomCenter, IndividualWallXLength, RoomTotalWallLength);

	// 2) If requested, generate the roof
	if (bHasRoof && RoofMeshes.Num() > 0)
	{
		GenerateRoof(
			LocalRoomCenter,
			RoomTotalWallLength,
			IndividualRoofXLength,
			IndividualRoofYLength,
			RoofHeight,
			RoofRotationBounds
		);
	}
}

UHierarchicalInstancedStaticMeshComponent* ADestroyedBuildingGenerator::GetOrCreateHISM(UStaticMesh* InMesh)
{
	if (!InMesh)
	{
		return nullptr;
	}

	// Check if we already have a HISM for this mesh
	for (UHierarchicalInstancedStaticMeshComponent* HISM : Hism_Components)
	{
		if (HISM && HISM->GetStaticMesh() == InMesh)
		{
			return HISM;
		}
	}

	// Not found -> create a new one
	UHierarchicalInstancedStaticMeshComponent* NewHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	if (NewHISM)
	{
		NewHISM->SetupAttachment(RootComponent);
		NewHISM->SetStaticMesh(InMesh);
		NewHISM->RegisterComponent();
		Hism_Components.Add(NewHISM);
	}

	return NewHISM;
}

void ADestroyedBuildingGenerator::GenerateWalls(
	const FVector& LocalCenter,
	const float IndividualWallXLength,
	const float RoomTotalWallLength
)
{
	const float Half = RoomTotalWallLength * 0.5f;

	// FRONT wall: +X
	{
		const FVector  WallLocation(LocalCenter.X + Half, LocalCenter.Y, LocalCenter.Z);
		const FRotator WallRotation(0.f, 0.f, 0.f);
		GenerateWallSegments(WallLocation, WallRotation, IndividualWallXLength, RoomTotalWallLength);
	}

	// BACK wall: -X
	{
		const FVector  WallLocation(LocalCenter.X - Half, LocalCenter.Y, LocalCenter.Z);
		const FRotator WallRotation(0.f, 180.f, 0.f);
		GenerateWallSegments(WallLocation, WallRotation, IndividualWallXLength, RoomTotalWallLength);
	}

	// LEFT wall: -Y (note: keep rotation the same, but change location to Y - Half)
	{
		const FVector  WallLocation(LocalCenter.X, LocalCenter.Y - Half, LocalCenter.Z);
		const FRotator WallRotation(0.f, -90.f, 0.f);
		GenerateWallSegments(WallLocation, WallRotation, IndividualWallXLength, RoomTotalWallLength);
	}

	// RIGHT wall: +Y (note: keep rotation the same, but change location to Y + Half)
	{
		const FVector  WallLocation(LocalCenter.X, LocalCenter.Y + Half, LocalCenter.Z);
		const FRotator WallRotation(0.f, 90.f, 0.f);
		GenerateWallSegments(WallLocation, WallRotation, IndividualWallXLength, RoomTotalWallLength);
	}
}

void ADestroyedBuildingGenerator::GenerateWallSegments(
	const FVector& BaseLocation,
	const FRotator& BaseRotation,
	const float IndividualWallXLength,
	const float WallTotalLength
)
{
	if (WallMeshes.Num() == 0 || IndividualWallXLength <= 0.f)
	{
		return;
	}

	// Number of segments to cover the total length
	const int32 NumSegments = FMath::CeilToInt(WallTotalLength / IndividualWallXLength);
	if (NumSegments < 1)
	{
		return;
	}

	const float TotalCoverageX = NumSegments * IndividualWallXLength;
	const float HalfCoverageX  = TotalCoverageX * 0.5f;

	for (int32 i = 0; i < NumSegments; i++)
	{
		UStaticMesh* RandomWallMesh = WallMeshes[FMath::RandRange(0, WallMeshes.Num() - 1)];
		if (!RandomWallMesh)
		{
			continue;
		}

		UHierarchicalInstancedStaticMeshComponent* WallHISM = GetOrCreateHISM(RandomWallMesh);
		if (!WallHISM)
		{
			continue;
		}

		const float XOffset = (i * IndividualWallXLength) + (IndividualWallXLength * 0.5f) - HalfCoverageX;

		FTransform SegmentTransform =
			FTransform(BaseRotation, BaseLocation) *
			FTransform(FVector(XOffset, 0.f, 0.f));

		WallHISM->AddInstance(SegmentTransform);
	}
}

void ADestroyedBuildingGenerator::GenerateRoof(
	const FVector& LocalCenter,
	const float RoomTotalWallLength,
	const float IndividualRoofXLength,
	const float IndividualRoofYLength,
	const float RoofHeight,
	const FGeneratedRotationBounds& RotationBounds
)
{
	if (RoofMeshes.Num() == 0)
	{
		return;
	}

	UStaticMesh* RoofMesh = RoofMeshes[0];
	if (!RoofMesh || IndividualRoofXLength <= 0.f || IndividualRoofYLength <= 0.f)
	{
		return;
	}

	UHierarchicalInstancedStaticMeshComponent* RoofHISM = GetOrCreateHISM(RoofMesh);
	if (!RoofHISM)
	{
		return;
	}

	const int32 NumTilesX = FMath::CeilToInt(RoomTotalWallLength / IndividualRoofXLength);
	const int32 NumTilesY = FMath::CeilToInt(RoomTotalWallLength / IndividualRoofYLength);

	const float TotalCoverageX = NumTilesX * IndividualRoofXLength;
	const float TotalCoverageY = NumTilesY * IndividualRoofYLength;

	const float HalfCoverageX = TotalCoverageX * 0.5f;
	const float HalfCoverageY = TotalCoverageY * 0.5f;

	for (int32 ix = 0; ix < NumTilesX; ix++)
	{
		for (int32 iy = 0; iy < NumTilesY; iy++)
		{
			const float XOffset = (ix * IndividualRoofXLength) + (IndividualRoofXLength * 0.5f) - HalfCoverageX;
			const float YOffset = (iy * IndividualRoofYLength) + (IndividualRoofYLength * 0.5f) - HalfCoverageY;

			const FVector TileLocation(
				LocalCenter.X + XOffset,
				LocalCenter.Y + YOffset,
				LocalCenter.Z + RoofHeight
			);

			const FRotator TileRotation = GenerateRandomRotation(RotationBounds);
			const FTransform TileTransform(TileRotation, TileLocation);

			RoofHISM->AddInstance(TileTransform);
		}
	}
}

FRotator ADestroyedBuildingGenerator::GenerateRandomRotation(const FGeneratedRotationBounds& Bounds) const
{
	FRotator Result;
	Result.Pitch = FMath::RandRange(Bounds.MinRotationXY.Pitch, Bounds.MaxRotationXY.Pitch);
	Result.Roll  = FMath::RandRange(Bounds.MinRotationXY.Roll,  Bounds.MaxRotationXY.Roll);
	Result.Yaw   = FMath::RandRange(Bounds.MinRotationZ.Yaw,    Bounds.MaxRotationZ.Yaw);

	return Result;
}
