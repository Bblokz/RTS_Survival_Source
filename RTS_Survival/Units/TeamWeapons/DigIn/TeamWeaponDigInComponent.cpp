// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TeamWeaponDigInComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Misc/PackageName.h"
#include "TeamWeaponDigInWall.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInUnit/DigInUnit.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TeamWeaponDigInStatics
{
	const TCHAR* const WallMeshFolder = TEXT("/Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn/");
	const TCHAR* const WallMeshPrefix = TEXT("SM_TWDigIn_");
	// Faction copies of a gun Blueprint share the original's pit.
	const TCHAR* const FactionCopySuffix = TEXT("_Soviet");
	const TCHAR* const BlueprintClassSuffix = TEXT("_C");
	// Ground samples sit this far inside the pit's outer edge, under the walls.
	constexpr float GroundSampleInsetCm = 40.f;
	constexpr int32 GroundSamplesPerSide = 3;
	constexpr int32 MinGroundSamples = 3;
	// Samples start this far above the gun: the uphill side of a pit on a steep slope is well above it.
	constexpr float GroundSampleRaiseCm = 500.f;
	constexpr float MaxGroundTiltDegrees = 20.f;
	// Where the ground falls away under the fitted plane, the pit sinks into it up to this far (its footing reaches
	// 80 cm down): a pit sunk into a slope reads as dug in, one standing proud of it shows a gap under the bags.
	// A slit pit sinks less: its slit has to stay at the gun's barrel.
	constexpr float MaxSinkCm = 75.f;
	constexpr float MaxSlitPitSinkCm = 30.f;
	// Only slit pits carry this socket (the slit's test probe).
	const FName SlitSocketName(TEXT("ProbeSlit"));
	// Guards beyond the pit's posts stand this far from the post they share.
	constexpr float SharedGuardPostOffsetCm = 70.f;
}

UTeamWeaponDigInComponent::UTeamWeaponDigInComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UTeamWeaponDigInComponent::StartDigIn()
{
	if (GetIsDigInActive())
	{
		return true;
	}
	const FSoftObjectPath WallMeshPath = FindWallMeshPath();
	if (WallMeshPath.IsNull())
	{
		RTSFunctionLibrary::ReportError("No dig-in pit mesh for team weapon " + GetNameSafe(GetOwner()) +
			"\nExpected " + FString(TeamWeaponDigInStatics::WallMeshFolder) + TeamWeaponDigInStatics::WallMeshPrefix +
			"<gun Blueprint> or a WallMeshOverride on its TeamWeaponDigIn component.");
		return false;
	}
	M_Status = ETeamWeaponDigInStatus::LoadingWall;
	TWeakObjectPtr<UTeamWeaponDigInComponent> WeakThis(this);
	M_WallMeshLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		WallMeshPath, FStreamableDelegate::CreateLambda([WeakThis, WallMeshPath]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnWallMeshLoaded(WallMeshPath);
			}
		}));
	return true;
}

void UTeamWeaponDigInComponent::RemoveDigIn()
{
	if (M_WallMeshLoadHandle.IsValid())
	{
		M_WallMeshLoadHandle->CancelHandle();
		M_WallMeshLoadHandle.Reset();
	}
	if (ATeamWeaponDigInWall* Wall = M_Wall.Get())
	{
		Wall->Destroy();
	}
	M_Wall.Reset();
	RemoveProtection();
	M_Status = ETeamWeaponDigInStatus::None;
}

bool UTeamWeaponDigInComponent::TryGetGuardPostLocation(const int32 GuardIndex, FVector& OutLocation) const
{
	const ATeamWeaponDigInWall* Wall = M_Wall.Get();
	if (not IsValid(Wall) || GuardIndex < 0)
	{
		return false;
	}
	const int32 GuardPostCount = Wall->GetGuardPostCount();
	if (GuardPostCount <= 0 || not Wall->TryGetGuardPostLocation(GuardIndex % GuardPostCount, OutLocation))
	{
		return false;
	}
	const int32 SharingRound = GuardIndex / GuardPostCount;
	if (SharingRound > 0)
	{
		// Along the pit's length, towards the gun's front: the posts stand along the side walls.
		OutLocation += Wall->GetActorForwardVector() * (TeamWeaponDigInStatics::SharedGuardPostOffsetCm * SharingRound);
	}
	return true;
}

void UTeamWeaponDigInComponent::OnWallBuilt(ATeamWeaponDigInWall& Wall)
{
	if (M_Wall.Get() != &Wall || M_Status != ETeamWeaponDigInStatus::Building)
	{
		return;
	}
	M_Status = ETeamWeaponDigInStatus::DugIn;
	ApplyProtection();
	NotifyControllerDigInCompleted();
}

void UTeamWeaponDigInComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveDigIn();
	Super::EndPlay(EndPlayReason);
}

ATeamWeapon* UTeamWeaponDigInComponent::GetTeamWeapon() const
{
	ATeamWeapon* TeamWeapon = Cast<ATeamWeapon>(GetOwner());
	if (IsValid(TeamWeapon))
	{
		return TeamWeapon;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "TeamWeapon owner", "GetTeamWeapon", this);
	return nullptr;
}

UHealthComponent* UTeamWeaponDigInComponent::GetHealthComponent() const
{
	const AActor* Owner = GetOwner();
	UHealthComponent* HealthComponent = IsValid(Owner) ? Owner->FindComponentByClass<UHealthComponent>() : nullptr;
	if (IsValid(HealthComponent))
	{
		return HealthComponent;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "HealthComponent", "GetHealthComponent", this);
	return nullptr;
}

FSoftObjectPath UTeamWeaponDigInComponent::FindWallMeshPath() const
{
	using namespace TeamWeaponDigInStatics;
	if (not WallMeshOverride.IsNull())
	{
		return WallMeshOverride.ToSoftObjectPath();
	}
	// The gun's own Blueprint first, then its parents: a Blueprint child of a gun uses that gun's pit.
	for (const UClass* GunClass = GetOwner() ? GetOwner()->GetClass() : nullptr;
	     GunClass && GunClass != ATeamWeapon::StaticClass(); GunClass = GunClass->GetSuperClass())
	{
		FString GunName = GunClass->GetName();
		GunName.RemoveFromEnd(BlueprintClassSuffix);
		GunName.RemoveFromEnd(FactionCopySuffix);
		const FString AssetName = FString(WallMeshPrefix) + GunName;
		const FString PackageName = FString(WallMeshFolder) + AssetName;
		if (FPackageName::DoesPackageExist(PackageName))
		{
			return FSoftObjectPath(PackageName + TEXT(".") + AssetName);
		}
	}
	return FSoftObjectPath();
}

void UTeamWeaponDigInComponent::OnWallMeshLoaded(const FSoftObjectPath WallMeshPath)
{
	M_WallMeshLoadHandle.Reset();
	if (M_Status != ETeamWeaponDigInStatus::LoadingWall)
	{
		// Removed while the mesh was loading.
		return;
	}
	UStaticMesh* WallMesh = Cast<UStaticMesh>(WallMeshPath.ResolveObject());
	if (not IsValid(WallMesh))
	{
		FailDigIn("The pit mesh did not load: " + WallMeshPath.ToString());
		return;
	}
	if (not SpawnWall(*WallMesh))
	{
		FailDigIn("The pit could not be spawned.");
		return;
	}
	M_Status = ETeamWeaponDigInStatus::Building;
	ATeamWeaponDigInWall* Wall = M_Wall.Get();
	Wall->StartBuilding(WallMesh, this, GetOwningPlayer(), BuildSeconds);
}

bool UTeamWeaponDigInComponent::SpawnWall(UStaticMesh& WallMesh)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(GetOwner()))
	{
		return false;
	}
	FTransform WallTransform;
	(void)TryGetWallTransform(WallMesh, WallTransform);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Wall = World->SpawnActor<ATeamWeaponDigInWall>(ATeamWeaponDigInWall::StaticClass(), WallTransform,
	                                                 SpawnParameters);
	return M_Wall.IsValid();
}

bool UTeamWeaponDigInComponent::TryGetWallTransform(const UStaticMesh& WallMesh, FTransform& OutTransform) const
{
	using namespace TeamWeaponDigInStatics;
	const ATeamWeapon* TeamWeapon = GetTeamWeapon();
	if (not IsValid(TeamWeapon))
	{
		OutTransform = GetOwner()->GetActorTransform();
		return false;
	}
	const FVector GunLocation = TeamWeapon->GetActorLocation();
	const FRotator Heading(0.f, TeamWeapon->GetActorRotation().Yaw, 0.f);
	OutTransform = FTransform(Heading, GunLocation);

	TArray<FVector> GroundPoints;
	if (not TrySampleGroundUnderPit(*TeamWeapon, Heading, WallMesh.GetBoundingBox(), GroundPoints))
	{
		return false;
	}
	// Least-squares plane z = Height + SlopeX * x + SlopeY * y; the samples lie on a grid, so x and y separate.
	FVector Mean = FVector::ZeroVector;
	for (const FVector& Point : GroundPoints)
	{
		Mean += Point;
	}
	Mean /= GroundPoints.Num();
	double CovarianceX = 0.0, VarianceX = 0.0, CovarianceY = 0.0, VarianceY = 0.0;
	for (const FVector& Point : GroundPoints)
	{
		CovarianceX += (Point.X - Mean.X) * (Point.Z - Mean.Z);
		VarianceX += FMath::Square(Point.X - Mean.X);
		CovarianceY += (Point.Y - Mean.Y) * (Point.Z - Mean.Z);
		VarianceY += FMath::Square(Point.Y - Mean.Y);
	}
	const double MaxSlope = FMath::Tan(FMath::DegreesToRadians(MaxGroundTiltDegrees));
	const double SlopeX = FMath::Clamp(VarianceX > 0.0 ? CovarianceX / VarianceX : 0.0, -MaxSlope, MaxSlope);
	const double SlopeY = FMath::Clamp(VarianceY > 0.0 ? CovarianceY / VarianceY : 0.0, -MaxSlope, MaxSlope);
	const double HeightAtOrigin = Mean.Z - SlopeX * Mean.X - SlopeY * Mean.Y;
	double WorstFloatCm = 0.0;
	for (const FVector& Point : GroundPoints)
	{
		WorstFloatCm = FMath::Max(WorstFloatCm, HeightAtOrigin + SlopeX * Point.X + SlopeY * Point.Y - Point.Z);
	}
	const float SinkLimitCm = IsValid(WallMesh.FindSocket(SlitSocketName)) ? MaxSlitPitSinkCm : MaxSinkCm;
	const double SinkCm = FMath::Min(WorstFloatCm, static_cast<double>(SinkLimitCm));

	const FVector AlongLength(1.0, 0.0, SlopeX);
	const FVector AlongWidth(0.0, 1.0, SlopeY);
	const FVector Up = FVector::CrossProduct(AlongLength, AlongWidth).GetSafeNormal();
	const FQuat Tilt = FRotationMatrix::MakeFromXZ(AlongLength, Up).ToQuat();
	const FVector PitOrigin(GunLocation.X, GunLocation.Y, GunLocation.Z + HeightAtOrigin - SinkCm);
	OutTransform = FTransform(Heading.Quaternion() * Tilt, PitOrigin);
	return true;
}

bool UTeamWeaponDigInComponent::TrySampleGroundUnderPit(const ATeamWeapon& TeamWeapon, const FRotator& Heading,
                                                        const FBox& Bounds, TArray<FVector>& OutLocalGroundPoints) const
{
	using namespace TeamWeaponDigInStatics;
	const FVector GunLocation = TeamWeapon.GetActorLocation();
	const FVector2D Low(Bounds.Min.X + GroundSampleInsetCm, Bounds.Min.Y + GroundSampleInsetCm);
	const FVector2D High(Bounds.Max.X - GroundSampleInsetCm, Bounds.Max.Y - GroundSampleInsetCm);
	for (int32 SampleIndex = 0; SampleIndex < GroundSamplesPerSide * GroundSamplesPerSide; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex / GroundSamplesPerSide) / (GroundSamplesPerSide - 1);
		const float Beta = static_cast<float>(SampleIndex % GroundSamplesPerSide) / (GroundSamplesPerSide - 1);
		const FVector Local(FMath::Lerp(Low.X, High.X, Alpha), FMath::Lerp(Low.Y, High.Y, Beta), 0.f);
		const FVector WorldSample = GunLocation + Heading.RotateVector(Local) + FVector(0.f, 0.f, GroundSampleRaiseCm);
		float GroundZ = 0.f;
		if (TeamWeapon.TryGetGroundHeightAt(WorldSample, GroundZ))
		{
			OutLocalGroundPoints.Add(FVector(Local.X, Local.Y, GroundZ - GunLocation.Z));
		}
	}
	return OutLocalGroundPoints.Num() >= MinGroundSamples;
}

void UTeamWeaponDigInComponent::ApplyProtection()
{
	UHealthComponent* HealthComponent = GetHealthComponent();
	if (M_Protection.bIsApplied || not IsValid(HealthComponent))
	{
		return;
	}
	// Max health scales with current health kept in proportion; removing the pit scales both back.
	HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() * MaxHealthMultiplier);
	HealthComponent->SetDamageReductionMultiplier(HealthComponent->GetDamageReductionMultiplier() * DamageTakenMultiplier);
	M_Protection.AppliedMaxHealthMultiplier = MaxHealthMultiplier;
	M_Protection.AppliedDamageTakenMultiplier = DamageTakenMultiplier;
	M_Protection.bIsApplied = true;
}

void UTeamWeaponDigInComponent::RemoveProtection()
{
	if (not M_Protection.bIsApplied)
	{
		return;
	}
	M_Protection.bIsApplied = false;
	UHealthComponent* HealthComponent = GetHealthComponent();
	if (not IsValid(HealthComponent))
	{
		return;
	}
	HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() / M_Protection.AppliedMaxHealthMultiplier);
	HealthComponent->SetDamageReductionMultiplier(
		HealthComponent->GetDamageReductionMultiplier() / M_Protection.AppliedDamageTakenMultiplier);
}

void UTeamWeaponDigInComponent::FailDigIn(const FString& Reason)
{
	RTSFunctionLibrary::ReportError("Team weapon dig-in failed for " + GetNameSafe(GetOwner()) + ": " + Reason);
	RemoveDigIn();
	const ATeamWeapon* TeamWeapon = GetTeamWeapon();
	if (IDigInUnit* DigInUnit = IsValid(TeamWeapon) ? Cast<IDigInUnit>(TeamWeapon->GetTeamWeaponController()) : nullptr)
	{
		// Breaks cover on the controller: the command card and the hull lock go back to normal.
		DigInUnit->WallGotDestroyedForceBreakCover();
	}
}

void UTeamWeaponDigInComponent::NotifyControllerDigInCompleted() const
{
	const ATeamWeapon* TeamWeapon = GetTeamWeapon();
	if (IDigInUnit* DigInUnit = IsValid(TeamWeapon) ? Cast<IDigInUnit>(TeamWeapon->GetTeamWeaponController()) : nullptr)
	{
		DigInUnit->OnDigInCompleted();
	}
}

uint8 UTeamWeaponDigInComponent::GetOwningPlayer() const
{
	const AActor* Owner = GetOwner();
	const URTSComponent* RTSComponent = IsValid(Owner) ? Owner->FindComponentByClass<URTSComponent>() : nullptr;
	return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
}
