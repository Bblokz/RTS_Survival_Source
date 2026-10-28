#include "WreckDebrisBurstComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/Tanks/WreckTurretToss/WreckDebrisSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckDebris, Log, All);

namespace WreckDebrisConstants
{
	constexpr float SettleCheckInterval = 0.1f;
	// Pieces have no velocity before the launch tick and still overlap the hull right after it.
	constexpr float MinFlightTimeBeforeSettle = 0.5f;
	constexpr float SpawnHeightAboveRing = 40.f;
	constexpr float FlyingAngularDamping = 0.4f;
	// A piece this far below the ring when it settles fell through the world and is hidden.
	constexpr float FellThroughWorldDepth = 5000.f;
	constexpr float FullCircleDegrees = 360.f;
}

UWreckDebrisBurstComponent::UWreckDebrisBurstComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWreckDebrisBurstComponent::StartBurst(const FVector& Origin, UStaticMeshComponent* HullMesh, const float CountScale)
{
	UWorld* World = GetWorld();
	if (not IsValid(HullMesh) || not IsValid(World) || GetSettings().PieceTypes.IsEmpty())
	{
		return;
	}
	M_Origin = Origin;
	M_FireSubsystem = World->GetSubsystem<URTSFireSubsystem>();

	UMaterialInterface* HullMaterial = HullMesh->GetMaterial(0);
	const int32 PieceCount = GetPieceCount(*HullMesh, CountScale);
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const FWreckDebrisPieceType* PieceType = PickPieceType();
		UStaticMeshComponent* Piece = PieceType ? SpawnPiece(*PieceType, HullMaterial) : nullptr;
		if (IsValid(Piece))
		{
			FWreckDebrisFlyingPiece& FlyingPiece = M_Pieces.AddDefaulted_GetRef();
			FlyingPiece.Mesh = Piece;
		}
	}
	UE_LOG(LogWreckDebris, Verbose, TEXT("Debris burst on %s: %d of %d pieces"), *GetNameSafe(GetOwner()),
	       M_Pieces.Num(), PieceCount);

	// Same as the turret: a velocity set in the frame physics was switched on is dropped.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		LaunchPieces();
	}));
	IgniteSomePieces();
	StartSettleChecks();
}

void UWreckDebrisBurstComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SettleCheckTimerHandle);
	}
	StopFires();
	Super::EndPlay(EndPlayReason);
}

int32 UWreckDebrisBurstComponent::GetPieceCount(const UStaticMeshComponent& HullMesh, const float CountScale) const
{
	const float CountForHull = FMath::GetMappedRangeValueClamped(
		GetSettings().HullRadiusRange,
		FVector2D(GetSettings().PieceCountRange.X, GetSettings().PieceCountRange.Y),
		HullMesh.Bounds.SphereRadius);
	return FMath::Max(0, FMath::RoundToInt(CountForHull * CountScale));
}

const FWreckDebrisPieceType* UWreckDebrisBurstComponent::PickPieceType() const
{
	float TotalWeight = 0.f;
	for (const FWreckDebrisPieceType& PieceType : GetSettings().PieceTypes)
	{
		TotalWeight += FMath::Max(PieceType.Weight, 0.f);
	}
	float Pick = FMath::FRandRange(0.f, TotalWeight);
	for (const FWreckDebrisPieceType& PieceType : GetSettings().PieceTypes)
	{
		Pick -= FMath::Max(PieceType.Weight, 0.f);
		if (Pick <= 0.f)
		{
			return &PieceType;
		}
	}
	return GetSettings().PieceTypes.IsEmpty() ? nullptr : &GetSettings().PieceTypes.Last();
}

UStaticMeshComponent* UWreckDebrisBurstComponent::SpawnPiece(const FWreckDebrisPieceType& PieceType,
                                                              UMaterialInterface* HullMaterial)
{
	using namespace WreckDebrisConstants;
	AActor* Wreck = GetOwner();
	// Small meshes, loaded once and then cached by the engine.
	UStaticMesh* Mesh = PieceType.Mesh.LoadSynchronous();
	if (not IsValid(Wreck) || not IsValid(Mesh) || not IsValid(Mesh->GetBodySetup())
		|| Mesh->GetBodySetup()->AggGeom.GetElementCount() == 0)
	{
		return nullptr;
	}

	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Wreck);
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetStaticMesh(Mesh);
	const FVector Scatter = FVector(FMath::RandPointInCircle(GetSettings().SpawnScatterRadius), SpawnHeightAboveRing);
	const FRotator RandomRotation(FMath::FRandRange(0.f, FullCircleDegrees), FMath::FRandRange(0.f, FullCircleDegrees),
	                              FMath::FRandRange(0.f, FullCircleDegrees));
	Piece->SetWorldTransform(FTransform(RandomRotation, M_Origin + Scatter,
	                                    FVector(FMath::FRandRange(PieceType.ScaleRange.X, PieceType.ScaleRange.Y))));
	if (PieceType.bUseWreckHullMaterial && IsValid(HullMaterial))
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Piece->GetNumMaterials(); ++MaterialIndex)
		{
			Piece->SetMaterial(MaterialIndex, HullMaterial);
		}
	}
	// Only the world blocks the pieces: no shoving units, no bumping each other, no navigation changes.
	Piece->SetCanEverAffectNavigation(false);
	Piece->SetGenerateOverlapEvents(false);
	Piece->SetCollisionObjectType(ECC_PhysicsBody);
	Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Piece->SetCollisionResponseToAllChannels(ECR_Ignore);
	Piece->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Piece->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Wreck->AddInstanceComponent(Piece);
	Piece->RegisterComponent();
	Piece->SetAngularDamping(FlyingAngularDamping);
	Piece->SetEnableGravity(true);
	Piece->SetSimulatePhysics(true);
	return Piece;
}

void UWreckDebrisBurstComponent::LaunchPieces() const
{
	const UWreckDebrisSettings& Settings = GetSettings();
	for (const FWreckDebrisFlyingPiece& FlyingPiece : M_Pieces)
	{
		UStaticMeshComponent* Piece = FlyingPiece.Mesh.Get();
		if (not IsValid(Piece))
		{
			continue;
		}
		const FVector SideDirection = FRotator(
			0.f, FMath::FRandRange(0.f, WreckDebrisConstants::FullCircleDegrees), 0.f).Vector();
		const float SideSpeed = FMath::FRandRange(Settings.SideSpeedRange.X, Settings.SideSpeedRange.Y);
		const float UpSpeed = FMath::FRandRange(Settings.UpSpeedRange.X, Settings.UpSpeedRange.Y);
		const FVector LaunchVelocity = SideDirection * SideSpeed + FVector::UpVector * UpSpeed;
		Piece->WakeRigidBody();
		Piece->SetPhysicsLinearVelocity(LaunchVelocity);
		Piece->SetPhysicsAngularVelocityInDegrees(
			FMath::VRand() * FMath::FRandRange(Settings.SpinDegreesRange.X, Settings.SpinDegreesRange.Y));
	}
}

void UWreckDebrisBurstComponent::IgniteSomePieces()
{
	if (not M_FireSubsystem.IsValid() || GetSettings().BurningPieceFireType == ERTSFireType::None)
	{
		return;
	}
	const int32 BurningCount = FMath::Min(GetSettings().BurningPieceCount, M_Pieces.Num());
	for (int32 PieceIndex = 0; PieceIndex < BurningCount; ++PieceIndex)
	{
		UStaticMeshComponent* Piece = M_Pieces[PieceIndex].Mesh.Get();
		if (not IsValid(Piece))
		{
			continue;
		}
		M_FireHandles.Add(M_FireSubsystem->SpawnFireAttachedToComponent(
			GetOwner(), Piece, NAME_None, FVector::ZeroVector, GetSettings().BurningPieceFireType,
			GetSettings().BurningPieceFireLifeTime, GetSettings().BurningPieceFireScale, FRTSFireEffectParams()));
	}
}

void UWreckDebrisBurstComponent::StartSettleChecks()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || M_Pieces.IsEmpty())
	{
		return;
	}
	M_LaunchWorldTime = World->GetTimeSeconds();
	FTimerDelegate SettleCheckDelegate;
	SettleCheckDelegate.BindWeakLambda(this, [this]()
	{
		CheckPiecesSettled();
	});
	World->GetTimerManager().SetTimer(M_SettleCheckTimerHandle, SettleCheckDelegate,
	                                  WreckDebrisConstants::SettleCheckInterval, true);
}

void UWreckDebrisBurstComponent::CheckPiecesSettled()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const float FlightTime = World->GetTimeSeconds() - M_LaunchWorldTime;
	bool bAllSettled = true;
	for (FWreckDebrisFlyingPiece& FlyingPiece : M_Pieces)
	{
		if (FlyingPiece.bIsSettled)
		{
			continue;
		}
		if (GetShouldSettle(FlyingPiece, FlightTime))
		{
			SettlePiece(FlyingPiece);
			continue;
		}
		bAllSettled = false;
	}
	if (not bAllSettled)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_SettleCheckTimerHandle);
	UE_LOG(LogWreckDebris, Verbose, TEXT("Debris on %s settled after %.1fs"), *GetNameSafe(GetOwner()), FlightTime);
}

bool UWreckDebrisBurstComponent::GetShouldSettle(FWreckDebrisFlyingPiece& Piece, const float FlightTime) const
{
	UStaticMeshComponent* Mesh = Piece.Mesh.Get();
	if (not IsValid(Mesh) || FlightTime >= GetSettings().MaxFlightTime)
	{
		return true;
	}
	if (FlightTime < WreckDebrisConstants::MinFlightTimeBeforeSettle)
	{
		return false;
	}
	if (not Mesh->IsAnyRigidBodyAwake())
	{
		return true;
	}
	const bool bIsSlow = Mesh->GetPhysicsLinearVelocity().Size() <= GetSettings().SettleLinearSpeed
		&& Mesh->GetPhysicsAngularVelocityInDegrees().Size() <= GetSettings().SettleAngularSpeed;
	Piece.TimeBelowSettleSpeed = bIsSlow ? Piece.TimeBelowSettleSpeed + WreckDebrisConstants::SettleCheckInterval : 0.f;
	return Piece.TimeBelowSettleSpeed >= GetSettings().SettleDuration;
}

void UWreckDebrisBurstComponent::SettlePiece(FWreckDebrisFlyingPiece& Piece) const
{
	Piece.bIsSettled = true;
	UStaticMeshComponent* Mesh = Piece.Mesh.Get();
	if (not IsValid(Mesh))
	{
		return;
	}
	// Landed debris is decoration: no simulation and no collision, so it costs nothing and blocks nothing.
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Mesh->GetComponentLocation().Z < M_Origin.Z - WreckDebrisConstants::FellThroughWorldDepth)
	{
		Mesh->SetHiddenInGame(true);
	}
}

void UWreckDebrisBurstComponent::StopFires()
{
	if (M_FireSubsystem.IsValid())
	{
		for (const int32 FireHandle : M_FireHandles)
		{
			if (FireHandle != INDEX_NONE)
			{
				M_FireSubsystem->StopFireByHandle(FireHandle);
			}
		}
	}
	M_FireHandles.Reset();
}

const UWreckDebrisSettings& UWreckDebrisBurstComponent::GetSettings()
{
	return *GetDefault<UWreckDebrisSettings>();
}
