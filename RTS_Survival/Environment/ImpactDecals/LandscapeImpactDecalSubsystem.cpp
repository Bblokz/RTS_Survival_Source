// Copyright (C) Bas Blokzijl - All rights reserved.

#include "LandscapeImpactDecalSubsystem.h"

#include "Algo/Rotate.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInterface.h"
#include "RTSImpactDecalSettings.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSImpactDecals, Log, All);

namespace LandscapeImpactDecalPrivate
{
	constexpr int32 DefaultMaxDecals = 100;
	constexpr int32 HardMaxDecals = 2000;
	// Written by the settings menu (Impact Decals); 0 turns craters off and removes the ones on the map.
	TAutoConsoleVariable<int32> CVarImpactDecals(
		TEXT("rts.ImpactDecals"),
		DefaultMaxDecals,
		TEXT("How many shell craters (impact decals on the landscape) stay at once; the oldest makes way. 0 = off."),
		ECVF_Default);

	constexpr float AtlasCellsTotal = 16.f;
	// A crater this much larger than an existing one is not merged into it, even at the same spot.
	constexpr float MergeMaxRadiusRatio = 1.05f;
	constexpr int32 SortOrderWrap = 1000000;
	constexpr float BelowTraceLeadCm = 100.f;

#if RTS_WITH_MAP_TESTS
	// Read by the impact decal benchmark (-ImpactDecalBenchLive) to prove only landscape hits leave craters.
	int32 GMapTestLandscapeImpacts = 0;
	int32 GMapTestOtherImpacts = 0;
	int32 GMapTestBombLandscapeImpacts = 0;
	TMap<FName, int32> GMapTestOtherHitClasses;
#endif
}

int32 ULandscapeImpactDecalSubsystem::GetMapTestBombLandscapeImpacts()
{
#if RTS_WITH_MAP_TESTS
	return LandscapeImpactDecalPrivate::GMapTestBombLandscapeImpacts;
#else
	return 0;
#endif
}

void ULandscapeImpactDecalSubsystem::GetMapTestImpactCounts(int32& OutLandscapeImpacts, int32& OutOtherImpacts)
{
#if RTS_WITH_MAP_TESTS
	OutLandscapeImpacts = LandscapeImpactDecalPrivate::GMapTestLandscapeImpacts;
	OutOtherImpacts = LandscapeImpactDecalPrivate::GMapTestOtherImpacts;
#else
	OutLandscapeImpacts = 0;
	OutOtherImpacts = 0;
#endif
}

void ULandscapeImpactDecalSubsystem::ResetMapTestImpactCounts()
{
#if RTS_WITH_MAP_TESTS
	LandscapeImpactDecalPrivate::GMapTestLandscapeImpacts = 0;
	LandscapeImpactDecalPrivate::GMapTestOtherImpacts = 0;
	LandscapeImpactDecalPrivate::GMapTestBombLandscapeImpacts = 0;
	LandscapeImpactDecalPrivate::GMapTestOtherHitClasses.Reset();
#endif
}

FString ULandscapeImpactDecalSubsystem::DescribeMapTestOtherHitClasses()
{
#if RTS_WITH_MAP_TESTS
	TArray<TPair<FName, int32>> Sorted = LandscapeImpactDecalPrivate::GMapTestOtherHitClasses.Array();
	Sorted.Sort([](const TPair<FName, int32>& A, const TPair<FName, int32>& B) { return A.Value > B.Value; });
	TArray<FString> Parts;
	for (const TPair<FName, int32>& Entry : Sorted)
	{
		Parts.Add(FString::Printf(TEXT("%s x%d"), *Entry.Key.ToString(), Entry.Value));
	}
	return FString::Join(Parts, TEXT(", "));
#else
	return FString();
#endif
}

bool ULandscapeImpactDecalSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void ULandscapeImpactDecalSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	IConsoleVariable* Variable = LandscapeImpactDecalPrivate::CVarImpactDecals.AsVariable();
	M_CVarChangedHandle = Variable->OnChangedDelegate().AddUObject(
		this, &ULandscapeImpactDecalSubsystem::OnMaxDecalsChanged);
	M_AppliedMaxDecals = GetMaxDecalsSetting();
}

void ULandscapeImpactDecalSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Load the material now rather than on the first impact (a hitch mid battle).
	if (GetMaxDecalsSetting() > 0)
	{
		GetIsReadyToSpawn();
		GetIsBombMaterialReady();
	}
}

void ULandscapeImpactDecalSubsystem::Deinitialize()
{
	LandscapeImpactDecalPrivate::CVarImpactDecals.AsVariable()->OnChangedDelegate().Remove(M_CVarChangedHandle);
	M_CVarChangedHandle.Reset();
	// The world is going away and takes the owner actor and its decals with it.
	M_Decals.Reset();
	M_Centres.Reset();
	M_Radii.Reset();
	M_SizeRanks.Reset();
	M_DecalOwner = nullptr;
	Super::Deinitialize();
}

void ULandscapeImpactDecalSubsystem::RequestImpactDecal(const UObject* WorldContext, const float CalibreMm,
                                                        const FVector& ImpactLocation, const FVector& ImpactNormal,
                                                        const AActor* HitActor)
{
	// Cheapest rejections first: this runs for every shell impact in the game.
	if (CalibreMm <= URTSImpactDecalSettings::Get()->MinCalibreMm || GetMaxDecalsSetting() <= 0)
	{
		return;
	}
	FVector CraterLocation;
	ULandscapeImpactDecalSubsystem* Subsystem = GetSubsystemForLandscapeHit(WorldContext, HitActor, ImpactLocation,
	                                                                        false, CraterLocation);
	if (IsValid(Subsystem))
	{
		Subsystem->AddImpactDecal(CalibreMm, CraterLocation, ImpactNormal);
	}
}

void ULandscapeImpactDecalSubsystem::RequestBombImpactDecal(const UObject* WorldContext, const float TntGrams,
                                                            const FVector& ImpactLocation, const FVector& ImpactNormal,
                                                            const AActor* HitActor)
{
	if (TntGrams <= 0.f || GetMaxDecalsSetting() <= 0)
	{
		return;
	}
	FVector CraterLocation;
	ULandscapeImpactDecalSubsystem* Subsystem = GetSubsystemForLandscapeHit(WorldContext, HitActor, ImpactLocation,
	                                                                        true, CraterLocation);
	if (IsValid(Subsystem))
	{
		Subsystem->AddBombImpactDecal(TntGrams, CraterLocation, ImpactNormal);
	}
}

ULandscapeImpactDecalSubsystem* ULandscapeImpactDecalSubsystem::GetSubsystemForLandscapeHit(
	const UObject* WorldContext, const AActor* HitActor, const FVector& ImpactLocation, const bool bIsBomb,
	FVector& OutCraterLocation)
{
	const bool bIsLandscapeHit = GetIsLandscapeActor(HitActor);
#if RTS_WITH_MAP_TESTS
	++(bIsLandscapeHit ? LandscapeImpactDecalPrivate::GMapTestLandscapeImpacts
	                   : LandscapeImpactDecalPrivate::GMapTestOtherImpacts);
	if (bIsLandscapeHit && bIsBomb)
	{
		++LandscapeImpactDecalPrivate::GMapTestBombLandscapeImpacts;
	}
	if (not bIsLandscapeHit)
	{
		const FName HitClass = IsValid(HitActor) ? HitActor->GetClass()->GetFName() : FName(TEXT("None"));
		++LandscapeImpactDecalPrivate::GMapTestOtherHitClasses.FindOrAdd(HitClass);
	}
#endif
	if (not bIsLandscapeHit || not IsValid(WorldContext))
	{
		return nullptr;
	}
	UWorld* World = WorldContext->GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	ULandscapeImpactDecalSubsystem* Subsystem = World->GetSubsystem<ULandscapeImpactDecalSubsystem>();
	if (not IsValid(Subsystem))
	{
		return nullptr;
	}
	// Traces hit the landscape's simple collision, which can sit most of a metre off the drawn surface on a coarse
	// collision mip; a thin crater box centred there would miss the ground. Snap onto the real heightfield (one map
	// lookup and one sample).
	OutCraterLocation = ImpactLocation;
	const TOptional<float> SurfaceHeight = CastChecked<ALandscapeProxy>(HitActor)->GetHeightAtLocation(ImpactLocation);
	if (SurfaceHeight.IsSet())
	{
		OutCraterLocation.Z = SurfaceHeight.GetValue();
	}
	return Subsystem;
}

void ULandscapeImpactDecalSubsystem::RequestImpactDecalBelowExplosion(const UObject* WorldContext,
                                                                      const float CalibreMm,
                                                                      const FVector& ExplosionLocation)
{
	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	if (CalibreMm <= Settings->MinCalibreMm || GetMaxDecalsSetting() <= 0 ||
		Settings->MaxTimedExplosionGroundGapCm <= 0.f || not IsValid(WorldContext))
	{
		return;
	}
	UWorld* World = WorldContext->GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Start a little above: a shell that already dipped under the surface between two traces still finds it.
	const FVector TraceStart = ExplosionLocation + FVector(0.f, 0.f, LandscapeImpactDecalPrivate::BelowTraceLeadCm);
	const FVector TraceEnd = ExplosionLocation - FVector(0.f, 0.f, Settings->MaxTimedExplosionGroundGapCm);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ImpactDecalBelowExplosion), false);
	FHitResult Hit;
	if (not World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return;
	}
	RequestImpactDecal(WorldContext, CalibreMm, Hit.ImpactPoint, Hit.ImpactNormal, Hit.GetActor());
}

bool ULandscapeImpactDecalSubsystem::GetIsLandscapeActor(const AActor* Actor)
{
	return IsValid(Actor) && Actor->IsA<ALandscapeProxy>();
}

int32 ULandscapeImpactDecalSubsystem::GetMaxDecalsSetting()
{
	return FMath::Clamp(LandscapeImpactDecalPrivate::CVarImpactDecals.GetValueOnGameThread(), 0,
	                    LandscapeImpactDecalPrivate::HardMaxDecals);
}

bool ULandscapeImpactDecalSubsystem::AddImpactDecal(const float CalibreMm, const FVector& ImpactLocation,
                                                    const FVector& ImpactNormal)
{
	const uint64 StartCycles = FPlatformTime::Cycles64();
	++M_Stats.Requests;
	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	int32 BandIndex = INDEX_NONE;
	const FRTSImpactDecalBand* Band = Settings->FindBand(CalibreMm, BandIndex);
	if (Band == nullptr || M_AppliedMaxDecals <= 0 || not GetIsReadyToSpawn())
	{
		return false;
	}
	const float CalibreScale = FMath::Clamp(CalibreMm / Band->DesignCalibreMm, Settings->CalibreScaleClamp.Min,
	                                        Settings->CalibreScaleClamp.Max);
	const float RandomScale = FMath::FRandRange(Settings->RandomSizeScale.Min, Settings->RandomSizeScale.Max);
	FCraterSpec Spec;
	Spec.Material = M_DecalMaterial;
	Spec.Radius = 0.5f * Band->DesignDiameterCm * CalibreScale * RandomScale * Settings->GlobalSizeMultiplier;
	Spec.HalfDepth = 0.5f * Band->ProjectionDepthCm;
	Spec.FirstAtlasCell = Band->FirstAtlasCell;
	Spec.AtlasCellCount = Band->AtlasCellCount;
	Spec.SizeRank = static_cast<uint8>(BandIndex);
	return AddCrater(Spec, ImpactLocation, ImpactNormal, StartCycles);
}

bool ULandscapeImpactDecalSubsystem::AddBombImpactDecal(const float TntGrams, const FVector& ImpactLocation,
                                                        const FVector& ImpactNormal)
{
	const uint64 StartCycles = FPlatformTime::Cycles64();
	++M_Stats.Requests;
	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	int32 BandIndex = INDEX_NONE;
	const FRTSBombCraterBand* Band = Settings->FindBombBand(TntGrams, BandIndex);
	if (Band == nullptr || M_AppliedMaxDecals <= 0 || not GetIsBombMaterialReady())
	{
		return false;
	}
	// Crater size follows the cube root of the charge.
	const float ChargeScale = FMath::Clamp(FMath::Pow(TntGrams / Band->DesignTntGrams, 1.f / 3.f),
	                                       Settings->BombScaleClamp.Min, Settings->BombScaleClamp.Max);
	const float RandomScale = FMath::FRandRange(Settings->RandomSizeScale.Min, Settings->RandomSizeScale.Max);
	FCraterSpec Spec;
	Spec.Material = M_BombDecalMaterial;
	Spec.Radius = 0.5f * Band->DesignDiameterCm * ChargeScale * RandomScale * Settings->GlobalSizeMultiplier;
	Spec.HalfDepth = 0.5f * Band->ProjectionDepthCm;
	Spec.FirstAtlasCell = Band->FirstAtlasCell;
	Spec.AtlasCellCount = Band->AtlasCellCount;
	// Bomb craters rank above every shell crater: shells landing in one merge, a bomb never merges into a shell pit.
	Spec.SizeRank = static_cast<uint8>(Settings->Bands.Num() + BandIndex);
	const bool bPlaced = AddCrater(Spec, ImpactLocation, ImpactNormal, StartCycles);
	if (bPlaced)
	{
		++M_Stats.BombCraters;
	}
	return bPlaced;
}

bool ULandscapeImpactDecalSubsystem::AddCrater(const FCraterSpec& Spec, const FVector& ImpactLocation,
                                               const FVector& ImpactNormal, const uint64 StartCycles)
{
	if (GetIsCoveredByExistingCrater(ImpactLocation, Spec.Radius, Spec.SizeRank))
	{
		++M_Stats.MergedIntoExisting;
		M_Stats.AddSeconds += FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartCycles);
		return true;
	}

	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	const FTransform DecalTransform = MakeDecalTransform(ImpactLocation, ImpactNormal, Spec.Radius, Spec.HalfDepth);
	const FLinearColor DecalColor = MakeDecalColor(Spec.FirstAtlasCell, Spec.AtlasCellCount);
	// Newer craters on top; the offset wraps long before int overflow (a wrap only reorders overlaps once).
	const int32 SortOrder = Settings->SortOrder + M_NextSortOffset;
	M_NextSortOffset = (M_NextSortOffset + 1) % LandscapeImpactDecalPrivate::SortOrderWrap;
	int32 Slot = M_NextSlot;
	if (M_Decals.Num() < M_AppliedMaxDecals)
	{
		UDecalComponent* Decal = CreateDecalComponent(Spec.Material);
		if (not IsValid(Decal))
		{
			return false;
		}
		Decal->DecalColor = DecalColor;
		Decal->SortOrder = SortOrder;
		Decal->SetWorldTransform(DecalTransform);
		Decal->RegisterComponent();
		Slot = M_Decals.Add(Decal);
		M_Centres.Add(ImpactLocation);
		M_Radii.Add(Spec.Radius);
		M_SizeRanks.Add(Spec.SizeRank);
		M_NextSlot = M_Decals.Num() % M_AppliedMaxDecals;
		++M_Stats.CreatedComponents;
	}
	else
	{
		// Full ring: the oldest crater moves to the new impact. Colour, transform and (between the shell and bomb
		// sets) material all land in one render state update at the end of the frame.
		UDecalComponent* Decal = M_Decals[Slot];
		if (not IsValid(Decal))
		{
			Decal = CreateDecalComponent(Spec.Material);
			if (not IsValid(Decal))
			{
				return false;
			}
			Decal->RegisterComponent();
			M_Decals[Slot] = Decal;
		}
		if (Decal->GetDecalMaterial() != Spec.Material)
		{
			Decal->SetDecalMaterial(Spec.Material);
		}
		Decal->DecalColor = DecalColor;
		Decal->SortOrder = SortOrder;
		Decal->SetWorldTransform(DecalTransform);
		Decal->MarkRenderStateDirty();
		M_Centres[Slot] = ImpactLocation;
		M_Radii[Slot] = Spec.Radius;
		M_SizeRanks[Slot] = Spec.SizeRank;
		M_NextSlot = (Slot + 1) % M_AppliedMaxDecals;
		++M_Stats.RecycledComponents;
	}
	M_DecalCount = M_Decals.Num();
	++M_Stats.AddedDecals;
	M_Stats.AddSeconds += FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartCycles);
	return true;
}

bool ULandscapeImpactDecalSubsystem::GetIsBombCrater(const int32 Slot) const
{
	return M_SizeRanks.IsValidIndex(Slot) && M_SizeRanks[Slot] >= URTSImpactDecalSettings::Get()->Bands.Num();
}

void ULandscapeImpactDecalSubsystem::ClearDecals()
{
	DestroyAllDecals();
}

void ULandscapeImpactDecalSubsystem::OnMaxDecalsChanged(IConsoleVariable* Variable)
{
	ApplyMaxDecals(GetMaxDecalsSetting());
}

void ULandscapeImpactDecalSubsystem::ApplyMaxDecals(const int32 NewMaxDecals)
{
	if (NewMaxDecals == M_AppliedMaxDecals)
	{
		return;
	}
	UE_LOG(LogRTSImpactDecals, Display, TEXT("RTS_IMPACT_DECALS max %d -> %d (%d on the map)"),
	       M_AppliedMaxDecals, NewMaxDecals, M_Decals.Num());
	if (NewMaxDecals <= 0)
	{
		DestroyAllDecals();
		M_AppliedMaxDecals = 0;
		return;
	}
	ReorderOldestFirst();
	const int32 ExcessDecals = M_Decals.Num() - NewMaxDecals;
	if (ExcessDecals > 0)
	{
		for (int32 Index = 0; Index < ExcessDecals; ++Index)
		{
			if (IsValid(M_Decals[Index]))
			{
				M_Decals[Index]->DestroyComponent();
			}
		}
		M_Decals.RemoveAt(0, ExcessDecals, EAllowShrinking::No);
		M_Centres.RemoveAt(0, ExcessDecals, EAllowShrinking::No);
		M_Radii.RemoveAt(0, ExcessDecals, EAllowShrinking::No);
		M_SizeRanks.RemoveAt(0, ExcessDecals, EAllowShrinking::No);
	}
	M_AppliedMaxDecals = NewMaxDecals;
	// Oldest first: a full ring overwrites slot 0 next, a ring with room appends.
	M_NextSlot = M_Decals.Num() % M_AppliedMaxDecals;
	M_DecalCount = M_Decals.Num();
	if (not GetIsReadyToSpawn())
	{
		UE_LOG(LogRTSImpactDecals, Warning, TEXT("RTS_IMPACT_DECALS on but the decal material did not load."));
	}
}

void ULandscapeImpactDecalSubsystem::ReorderOldestFirst()
{
	// Only a full ring wraps; with room left the slots are already in age order and M_NextSlot is the end.
	if (M_NextSlot == 0 || M_NextSlot >= M_Decals.Num())
	{
		return;
	}
	auto RotateLeft = [Shift = M_NextSlot](auto& Array)
	{
		Algo::Rotate(Array, Shift);
	};
	RotateLeft(M_Decals);
	RotateLeft(M_Centres);
	RotateLeft(M_Radii);
	RotateLeft(M_SizeRanks);
	M_NextSlot = 0;
}

void ULandscapeImpactDecalSubsystem::DestroyAllDecals()
{
	for (UDecalComponent* Decal : M_Decals)
	{
		if (IsValid(Decal))
		{
			Decal->DestroyComponent();
		}
	}
	M_Decals.Reset();
	M_Centres.Reset();
	M_Radii.Reset();
	M_SizeRanks.Reset();
	M_NextSlot = 0;
	M_DecalCount = 0;
	if (IsValid(M_DecalOwner))
	{
		M_DecalOwner->Destroy();
	}
	M_DecalOwner = nullptr;
}

bool ULandscapeImpactDecalSubsystem::GetIsReadyToSpawn()
{
	if (IsValid(M_DecalMaterial))
	{
		return true;
	}
	if (bM_HasReportedMissingMaterial)
	{
		return false;
	}
	M_DecalMaterial = URTSImpactDecalSettings::Get()->DecalMaterial.LoadSynchronous();
	if (IsValid(M_DecalMaterial))
	{
		return true;
	}
	bM_HasReportedMissingMaterial = true;
	UE_LOG(LogRTSImpactDecals, Warning, TEXT("RTS_IMPACT_DECALS decal material %s did not load; no craters."),
	       *URTSImpactDecalSettings::Get()->DecalMaterial.ToString());
	return false;
}

bool ULandscapeImpactDecalSubsystem::GetIsBombMaterialReady()
{
	if (IsValid(M_BombDecalMaterial))
	{
		return true;
	}
	if (bM_HasReportedMissingBombMaterial)
	{
		return false;
	}
	M_BombDecalMaterial = URTSImpactDecalSettings::Get()->BombDecalMaterial.LoadSynchronous();
	if (IsValid(M_BombDecalMaterial))
	{
		return true;
	}
	bM_HasReportedMissingBombMaterial = true;
	UE_LOG(LogRTSImpactDecals, Warning, TEXT("RTS_IMPACT_DECALS bomb decal material %s did not load; no bomb craters."),
	       *URTSImpactDecalSettings::Get()->BombDecalMaterial.ToString());
	return false;
}

bool ULandscapeImpactDecalSubsystem::EnsureDecalOwner()
{
	if (IsValid(M_DecalOwner))
	{
		return true;
	}
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(),
	                                            TEXT("LandscapeImpactDecals"));
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (not IsValid(Owner))
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Owner, TEXT("Root"), RF_Transient);
	Root->SetMobility(EComponentMobility::Static);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	M_DecalOwner = Owner;
	return true;
}

bool ULandscapeImpactDecalSubsystem::GetIsCoveredByExistingCrater(const FVector& Centre, const float Radius,
                                                                  const int32 SizeRank) const
{
	const float MergeFraction = URTSImpactDecalSettings::Get()->MergeRadiusFraction;
	if (MergeFraction <= 0.f)
	{
		return false;
	}
	const int32 Count = M_Centres.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float ExistingRadius = M_Radii[Index];
		if (M_SizeRanks[Index] < SizeRank ||
			Radius > ExistingRadius * LandscapeImpactDecalPrivate::MergeMaxRadiusRatio)
		{
			continue;
		}
		const float MergeDistance = ExistingRadius * MergeFraction;
		if (FVector::DistSquared(M_Centres[Index], Centre) < MergeDistance * MergeDistance)
		{
			return true;
		}
	}
	return false;
}

UDecalComponent* ULandscapeImpactDecalSubsystem::CreateDecalComponent(UMaterialInterface* Material)
{
	if (not EnsureDecalOwner())
	{
		return nullptr;
	}
	const URTSImpactDecalSettings* Settings = URTSImpactDecalSettings::Get();
	UDecalComponent* Decal = NewObject<UDecalComponent>(M_DecalOwner, NAME_None, RF_Transient);
	// Unregistered: these set fields only, no render state churn.
	Decal->SetDecalMaterial(Material);
	Decal->DecalSize = FVector::OneVector;
	Decal->SortOrder = Settings->SortOrder;
	Decal->FadeScreenSize = Settings->FadeScreenSize;
	Decal->SetUsingAbsoluteLocation(true);
	Decal->SetUsingAbsoluteRotation(true);
	Decal->SetUsingAbsoluteScale(true);
	Decal->SetMobility(EComponentMobility::Movable);
	Decal->SetupAttachment(M_DecalOwner->GetRootComponent());
	return Decal;
}

FTransform ULandscapeImpactDecalSubsystem::MakeDecalTransform(const FVector& Location, const FVector& ImpactNormal,
                                                              const float Radius, const float HalfDepth)
{
	const FVector Normal = ImpactNormal.IsNearlyZero() ? FVector::UpVector : ImpactNormal.GetSafeNormal();
	// The decal projects along its X axis, into the ground; a random spin about that axis hides repetition.
	const FVector ProjectionAxis = -Normal;
	const FQuat Facing = FRotationMatrix::MakeFromX(ProjectionAxis).ToQuat();
	const FQuat Spin(ProjectionAxis, FMath::FRandRange(0.f, UE_TWO_PI));
	// DecalSize is (1,1,1), so the scale is the box half extent: depth along X, radius across.
	return FTransform(Spin * Facing, Location, FVector(HalfDepth, Radius, Radius));
}

FLinearColor ULandscapeImpactDecalSubsystem::MakeDecalColor(const int32 FirstAtlasCell, const int32 AtlasCellCount)
{
	const int32 CellCount = FMath::Max(1, AtlasCellCount);
	const int32 Cell = FMath::Clamp(FirstAtlasCell + FMath::RandRange(0, CellCount - 1), 0,
	                                static_cast<int32>(LandscapeImpactDecalPrivate::AtlasCellsTotal) - 1);
	// R picks the atlas cell, G varies the look, A is the opacity (see URTSImpactDecalSettings).
	return FLinearColor((Cell + 0.5f) / LandscapeImpactDecalPrivate::AtlasCellsTotal, FMath::FRand(), 0.f, 1.f);
}
