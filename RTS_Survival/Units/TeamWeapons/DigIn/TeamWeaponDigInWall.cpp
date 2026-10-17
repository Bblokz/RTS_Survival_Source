// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponDigInWall.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "NavAreas/NavArea_Null.h"
#include "TeamWeaponDigInComponent.h"
#include "RTS_Survival/RTSComponents/NavCollision/NavCollisionEnvObstacle/RTSNavCollisionEnvObstacle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

namespace TeamWeaponDigInWallStatics
{
	// Below this the wall counts as built at once.
	constexpr float MinBuildSeconds = 0.05f;
	// Nav boxes stand upright: tilted with a pit on a slope, the tall boxes beside the entrance lean into it and close
	// it. This much extra height keeps the ground under a tilted wall run inside its upright box.
	constexpr float UprightNavBoxExtraHalfHeightCm = 300.f;
}

ATeamWeaponDigInWall::ATeamWeaponDigInWall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	M_PitRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PitRoot"));
	SetRootComponent(M_PitRoot);
	M_PitRoot->SetMobility(EComponentMobility::Movable);
	M_WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	M_WallMesh->SetupAttachment(M_PitRoot);
	M_WallMesh->SetMobility(EComponentMobility::Movable);
	M_WallMesh->SetGenerateOverlapEvents(false);
	// The nav obstacles carve the wall runs; the mesh itself must not (it would also block the pit's inside).
	M_WallMesh->SetCanEverAffectNavigation(false);
}

void ATeamWeaponDigInWall::StartBuilding(UStaticMesh* WallMesh, UTeamWeaponDigInComponent* OwningDigIn,
                                         const uint8 OwningPlayer, const float BuildSeconds)
{
	M_OwningDigIn = OwningDigIn;
	if (not GetIsValidWallMesh() || not IsValid(WallMesh))
	{
		return;
	}
	M_WallMesh->SetStaticMesh(WallMesh);
	SetupWallCollision(OwningPlayer);
	CreateNavObstacles();
	M_BuildState.BuildSeconds = BuildSeconds;
	M_BuildState.ElapsedSeconds = 0.f;
	M_BuildState.SunkDepthCm = WallMesh->GetBoundingBox().Max.Z;
	if (BuildSeconds < TeamWeaponDigInWallStatics::MinBuildSeconds)
	{
		CompleteBuilding();
		return;
	}
	SetRiseProgress(0.f);
	SetActorTickEnabled(true);
}

float ATeamWeaponDigInWall::GetBuildProgress() const
{
	if (M_BuildState.bIsBuilt || M_BuildState.BuildSeconds <= 0.f)
	{
		return M_BuildState.bIsBuilt ? 1.f : 0.f;
	}
	return FMath::Clamp(M_BuildState.ElapsedSeconds / M_BuildState.BuildSeconds, 0.f, 1.f);
}

int32 ATeamWeaponDigInWall::GetGuardPostCount() const
{
	if (not GetIsValidWallMesh() || not IsValid(M_WallMesh->GetStaticMesh()))
	{
		return 0;
	}
	int32 GuardPostCount = 0;
	for (const UStaticMeshSocket* Socket : M_WallMesh->GetStaticMesh()->Sockets)
	{
		if (IsValid(Socket) && Socket->SocketName.ToString().StartsWith(TeamWeaponDigInWallSockets::GuardPostPrefix))
		{
			++GuardPostCount;
		}
	}
	return GuardPostCount;
}

bool ATeamWeaponDigInWall::TryGetGuardPostLocation(const int32 GuardPostIndex, FVector& OutLocation) const
{
	const FName SocketName(*FString::Printf(TEXT("%s%d"), TeamWeaponDigInWallSockets::GuardPostPrefix,
	                                        GuardPostIndex));
	return TryGetBuiltSocketLocation(SocketName, OutLocation);
}

bool ATeamWeaponDigInWall::TryGetBuiltSocketLocation(const FName SocketName, FVector& OutLocation) const
{
	FVector RelativeLocation;
	if (not TryGetSocketRelativeLocation(SocketName, RelativeLocation))
	{
		return false;
	}
	// Relative to the actor, not to the mesh: the mesh sits lower while the wall is still rising.
	OutLocation = GetActorTransform().TransformPosition(RelativeLocation);
	return true;
}

void ATeamWeaponDigInWall::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (M_BuildState.bIsBuilt)
	{
		SetActorTickEnabled(false);
		return;
	}
	M_BuildState.ElapsedSeconds += DeltaSeconds;
	if (M_BuildState.ElapsedSeconds >= M_BuildState.BuildSeconds)
	{
		CompleteBuilding();
		return;
	}
	SetRiseProgress(GetBuildProgress());
}

float ATeamWeaponDigInWall::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent,
                                       AController* EventInstigator, AActor* DamageCauser)
{
	if (not GetIsValidOwningDigIn())
	{
		return DamageAmount;
	}
	AActor* ProtectedWeapon = M_OwningDigIn->GetOwner();
	if (not IsValid(ProtectedWeapon))
	{
		return DamageAmount;
	}
	// The gun's own result: zero when the hit killed it, so the shooter is credited with the kill.
	return ProtectedWeapon->TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

bool ATeamWeaponDigInWall::GetIsValidWallMesh() const
{
	if (IsValid(M_WallMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_WallMesh", "GetIsValidWallMesh", this);
	return false;
}

bool ATeamWeaponDigInWall::GetIsValidOwningDigIn() const
{
	if (M_OwningDigIn.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_OwningDigIn", "GetIsValidOwningDigIn", this);
	return false;
}

void ATeamWeaponDigInWall::SetupWallCollision(const uint8 OwningPlayer) const
{
	FRTS_CollisionSetup::SetupWallActorMeshCollision(M_WallMesh, false, OwningPlayer);
	// Clicks go through to the gun and its crew; the walls are not a unit the player can select.
	M_WallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

void ATeamWeaponDigInWall::CreateNavObstacles()
{
	const UStaticMesh* WallMesh = M_WallMesh->GetStaticMesh();
	const FTransform& WallTransform = GetActorTransform();
	const FQuat UprightRotation = FRotator(0.f, GetActorRotation().Yaw, 0.f).Quaternion();
	for (const UStaticMeshSocket* Socket : WallMesh->Sockets)
	{
		if (not IsValid(Socket) || not Socket->SocketName.ToString().StartsWith(TeamWeaponDigInWallSockets::NavBoxPrefix))
		{
			continue;
		}
		URTSNavCollisionEnvObstacle* NavObstacle = NewObject<URTSNavCollisionEnvObstacle>(this);
		NavObstacle->SetAreaClass(UNavArea_Null::StaticClass());
		AddInstanceComponent(NavObstacle);
		NavObstacle->RegisterComponent();
		const FVector WorldCentre = WallTransform.TransformPosition(Socket->RelativeLocation);
		FVector LocalExtent = Socket->RelativeScale * TeamWeaponDigInWallSockets::NavBoxScale;
		LocalExtent.Z += TeamWeaponDigInWallStatics::UprightNavBoxExtraHalfHeightCm;
		NavObstacle->SetUpNavModifierVolume(WorldCentre, LocalExtent, UprightRotation);
		M_NavObstacles.Add(NavObstacle);
	}
	if (M_NavObstacles.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Team weapon dig-in wall mesh has no NavBox sockets; the pit will not block "
			"navigation.\nMesh: " + WallMesh->GetName());
	}
}

void ATeamWeaponDigInWall::SetRiseProgress(const float Progress) const
{
	// Eased so the last courses settle in slowly instead of stopping hard.
	const float EasedProgress = FMath::InterpEaseOut(0.f, 1.f, Progress, 2.f);
	M_WallMesh->SetRelativeLocation(FVector(0.f, 0.f, -M_BuildState.SunkDepthCm * (1.f - EasedProgress)));
}

void ATeamWeaponDigInWall::CompleteBuilding()
{
	M_BuildState.bIsBuilt = true;
	M_BuildState.ElapsedSeconds = M_BuildState.BuildSeconds;
	SetActorTickEnabled(false);
	if (GetIsValidWallMesh())
	{
		M_WallMesh->SetRelativeLocation(FVector::ZeroVector);
	}
	if (GetIsValidOwningDigIn())
	{
		M_OwningDigIn->OnWallBuilt(*this);
	}
}

bool ATeamWeaponDigInWall::TryGetSocketRelativeLocation(const FName SocketName, FVector& OutRelativeLocation) const
{
	if (not GetIsValidWallMesh() || not IsValid(M_WallMesh->GetStaticMesh()))
	{
		return false;
	}
	const UStaticMeshSocket* Socket = M_WallMesh->GetStaticMesh()->FindSocket(SocketName);
	if (not IsValid(Socket))
	{
		return false;
	}
	OutRelativeLocation = Socket->RelativeLocation;
	return true;
}
