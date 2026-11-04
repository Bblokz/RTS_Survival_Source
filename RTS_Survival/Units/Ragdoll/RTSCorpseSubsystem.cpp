// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSCorpseSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "RTSCorpsePoseLibrary.h"
#include "RTSRagdollSettings.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCorpses, Log, All);

namespace RTSCorpseSubsystemStatics
{
	constexpr int32 DefaultCorpseCap = 100;
	// Written by the settings menu (Battlefield Corpses); read on every new corpse and by the trim check.
	TAutoConsoleVariable<int32> CVarCorpses(
		TEXT("rts.Corpses"),
		DefaultCorpseCap,
		TEXT("How many corpses stay on the battlefield at once; 0 lets every body sink away."),
		ECVF_Default);

	constexpr float TrimCheckSeconds = 1.f;
	const FName PelvisBone(TEXT("pelvis"));
	// Hidden instances wait in per-batch slots; once all batches together hold twice the cap they are compacted.
	constexpr int32 MaxInstancesPerCorpse = 2;
	constexpr float GroundTraceUpCm = 100.f;
	constexpr float GroundTraceDownCm = 400.f;
	// Hidden instances wait far below the map at zero scale until a new corpse takes them.
	const FTransform HiddenInstance(FQuat::Identity, FVector(0.0, 0.0, -100000.0), FVector::ZeroVector);
}

bool URTSCorpseSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void URTSCorpseSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadLibraries();
	InWorld.GetTimerManager().SetTimer(M_TrimTimer, this, &URTSCorpseSubsystem::TrimToCap,
	                                   RTSCorpseSubsystemStatics::TrimCheckSeconds, true);
}

void URTSCorpseSubsystem::LoadLibraries()
{
	const FString Folder = GetDefault<URTSRagdollSettings>()->CorpsePoseFolder.Path;
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
#if WITH_EDITOR
	// Editor builds scan content in the background at startup; a packaged game reads its registry ready made.
	AssetRegistry.ScanPathsSynchronous({Folder}, false);
#endif
	TArray<FAssetData> LibraryAssets;
	AssetRegistry.GetAssetsByPath(FName(*Folder), LibraryAssets, true);
	for (const FAssetData& LibraryAsset : LibraryAssets)
	{
		if (not LibraryAsset.IsInstanceOf(URTSCorpsePoseLibrary::StaticClass()))
		{
			continue;
		}
		URTSCorpsePoseLibrary* Library = Cast<URTSCorpsePoseLibrary>(LibraryAsset.GetAsset());
		if (not IsValid(Library) || Library->Poses.IsEmpty() || Library->SourceMesh.IsNull())
		{
			continue;
		}
		M_LibraryByMesh.Add(Library->SourceMesh.ToSoftObjectPath(), M_Libraries.Add(Library));
	}
	UE_LOG(LogRTSCorpses, Display, TEXT("RTS_CORPSE libraries=%d in %s"), M_Libraries.Num(), *Folder);
}

void URTSCorpseSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TrimTimer);
	}
	Super::Deinitialize();
}

int32 URTSCorpseSubsystem::GetCorpseCap()
{
	return FMath::Max(0, RTSCorpseSubsystemStatics::CVarCorpses.GetValueOnGameThread());
}

FTransform URTSCorpseSubsystem::GetPelvisTransform(const USkeletalMeshComponent* Mesh)
{
	if (not IsValid(Mesh))
	{
		return FTransform::Identity;
	}
	if (GetHasPelvisBody(Mesh))
	{
		return Mesh->GetBodyInstance(RTSCorpseSubsystemStatics::PelvisBone)->GetUnrealWorldTransform();
	}
	return Mesh->GetBoneIndex(RTSCorpseSubsystemStatics::PelvisBone) != INDEX_NONE
		       ? Mesh->GetBoneTransform(RTSCorpseSubsystemStatics::PelvisBone)
		       : Mesh->GetComponentTransform();
}

bool URTSCorpseSubsystem::GetHasPelvisBody(const USkeletalMeshComponent* Mesh)
{
	if (not IsValid(Mesh))
	{
		return false;
	}
	const FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RTSCorpseSubsystemStatics::PelvisBone);
	return PelvisBody != nullptr && PelvisBody->IsValidBodyInstance();
}

int32 URTSCorpseSubsystem::GetInstanceCount() const
{
	int32 Instances = 0;
	for (const FRTSCorpseBatch& Batch : M_Batches)
	{
		Instances += IsValid(Batch.Component) ? Batch.Component->GetInstanceCount() : 0;
	}
	return Instances;
}

bool URTSCorpseSubsystem::TryAddCorpse(const FRTSCorpseRequest& Request)
{
	const int32 Cap = GetCorpseCap();
	const int32 LibraryIndex = GetLibraryIndex(Request.SourceMesh);
	if (Cap == 0 || LibraryIndex == INDEX_NONE || not EnsureHolder())
	{
		return false;
	}
	while (M_Corpses.Num() >= Cap)
	{
		RemoveOldestCorpse();
	}
	const int32 PoseIndex = PickPose(LibraryIndex, Request);
	const int32 BatchIndex = FindOrCreateBatch(LibraryIndex, PoseIndex, Request.Materials);
	if (BatchIndex == INDEX_NONE)
	{
		return false;
	}
	FRTSCorpseBatch& Batch = M_Batches[BatchIndex];
	const FTransform CorpseTransform = GetCorpseTransform(M_Libraries[LibraryIndex]->Poses[PoseIndex], Request.Pelvis);
	int32 InstanceIndex = INDEX_NONE;
	if (Batch.FreeInstances.IsEmpty())
	{
		InstanceIndex = Batch.Component->AddInstance(CorpseTransform, true);
	}
	else
	{
		InstanceIndex = Batch.FreeInstances.Pop(EAllowShrinking::No);
		Batch.Component->UpdateInstanceTransform(InstanceIndex, CorpseTransform, true, true, true);
	}
	M_Corpses.Add({BatchIndex, InstanceIndex});
	if (GetInstanceCount() > Cap * RTSCorpseSubsystemStatics::MaxInstancesPerCorpse)
	{
		CompactBatches();
	}
	UE_LOG(LogRTSCorpses, Verbose, TEXT("RTS_CORPSE add mesh=%s pose=%d corpses=%d instances=%d"),
	       *GetNameSafe(Request.SourceMesh), PoseIndex, M_Corpses.Num(), GetInstanceCount());
	return true;
}

int32 URTSCorpseSubsystem::GetLibraryIndex(const USkeletalMesh* SourceMesh) const
{
	if (not IsValid(SourceMesh))
	{
		return INDEX_NONE;
	}
	const int32* LibraryIndex = M_LibraryByMesh.Find(FSoftObjectPath(SourceMesh));
	return LibraryIndex != nullptr ? *LibraryIndex : INDEX_NONE;
}

const URTSCorpsePoseLibrary* URTSCorpseSubsystem::GetLibraryFor(const USkeletalMesh* SourceMesh) const
{
	const int32 LibraryIndex = GetLibraryIndex(SourceMesh);
	return LibraryIndex == INDEX_NONE ? nullptr : M_Libraries[LibraryIndex].Get();
}

int32 URTSCorpseSubsystem::PickPose(const int32 LibraryIndex, const FRTSCorpseRequest& Request) const
{
	const URTSCorpsePoseLibrary& Library = *M_Libraries[LibraryIndex];
	// How the pelvis lies (face up, face down, on a side) shows in how high each of its axes points; a turn about
	// the vertical does not change that, and the corpse is turned to the body's heading afterwards.
	const FVector BodyAxes[] = {
		Request.Pelvis.GetUnitAxis(EAxis::X), Request.Pelvis.GetUnitAxis(EAxis::Y), Request.Pelvis.GetUnitAxis(EAxis::Z)
	};
	TArray<float> Scores;
	Scores.Reserve(Library.Poses.Num());
	float BestScore = TNumericLimits<float>::Max();
	for (const FRTSCorpsePose& Pose : Library.Poses)
	{
		// Without a body to match every pose scores the same, so the pick below is random.
		const float Score = not Request.bMatchPelvis
			                    ? 0.f
			                    : FMath::Square(Pose.PelvisAxisX.Z - BodyAxes[0].Z)
			                    + FMath::Square(Pose.PelvisAxisY.Z - BodyAxes[1].Z)
			                    + FMath::Square(Pose.PelvisAxisZ.Z - BodyAxes[2].Z);
		Scores.Add(Score);
		BestScore = FMath::Min(BestScore, Score);
	}
	TArray<int32, TInlineAllocator<16>> CloseMatches;
	const float Slack = GetDefault<URTSRagdollSettings>()->PoseMatchSlack;
	for (int32 PoseIndex = 0; PoseIndex < Scores.Num(); ++PoseIndex)
	{
		if (Scores[PoseIndex] <= BestScore + Slack)
		{
			CloseMatches.Add(PoseIndex);
		}
	}
	// A match whose batch holds a hidden instance reuses the slot an evicted corpse left.
	TArray<int32, TInlineAllocator<16>> MatchesWithFreeSlot;
	for (const int32 PoseIndex : CloseMatches)
	{
		const int32 BatchIndex = FindBatch(LibraryIndex, PoseIndex, Request.Materials);
		if (BatchIndex != INDEX_NONE && not M_Batches[BatchIndex].FreeInstances.IsEmpty())
		{
			MatchesWithFreeSlot.Add(PoseIndex);
		}
	}
	const TArray<int32, TInlineAllocator<16>>& Choices = MatchesWithFreeSlot.IsEmpty() ? CloseMatches : MatchesWithFreeSlot;
	return Choices[FMath::RandRange(0, Choices.Num() - 1)];
}

int32 URTSCorpseSubsystem::FindBatch(const int32 LibraryIndex, const int32 PoseIndex,
                                     const TArray<UMaterialInterface*>& Materials) const
{
	for (int32 BatchIndex = 0; BatchIndex < M_Batches.Num(); ++BatchIndex)
	{
		const FRTSCorpseBatch& Batch = M_Batches[BatchIndex];
		if (Batch.LibraryIndex != LibraryIndex || Batch.PoseIndex != PoseIndex
			|| Batch.Materials.Num() != Materials.Num())
		{
			continue;
		}
		bool bSameLook = true;
		for (int32 Slot = 0; Slot < Materials.Num() && bSameLook; ++Slot)
		{
			bSameLook = Batch.Materials[Slot] == Materials[Slot];
		}
		if (bSameLook)
		{
			return BatchIndex;
		}
	}
	return INDEX_NONE;
}

void URTSCorpseSubsystem::CompactBatches()
{
	for (int32 BatchIndex = 0; BatchIndex < M_Batches.Num(); ++BatchIndex)
	{
		CompactBatch(BatchIndex);
	}
}

void URTSCorpseSubsystem::CompactBatch(const int32 BatchIndex)
{
	FRTSCorpseBatch& Batch = M_Batches[BatchIndex];
	if (Batch.FreeInstances.IsEmpty() || not IsValid(Batch.Component))
	{
		return;
	}
	TArray<int32> Removed = Batch.FreeInstances;
	Removed.Sort(TGreater<int32>());
	Batch.Component->RemoveInstances(Removed, true);
	Batch.FreeInstances.Reset();
	// Removing shifts every later instance down; the corpses of this batch follow their instances.
	for (FRTSCorpseSlot& Corpse : M_Corpses)
	{
		if (Corpse.BatchIndex != BatchIndex)
		{
			continue;
		}
		int32 RemovedBelow = 0;
		for (const int32 RemovedIndex : Removed)
		{
			RemovedBelow += RemovedIndex < Corpse.InstanceIndex ? 1 : 0;
		}
		Corpse.InstanceIndex -= RemovedBelow;
	}
}

bool URTSCorpseSubsystem::GetCorpse(const int32 CorpseIndex, const URTSCorpsePoseLibrary*& OutLibrary,
                                    int32& OutPoseIndex, FTransform& OutTransform) const
{
	if (not M_Corpses.IsValidIndex(CorpseIndex) || not M_Batches.IsValidIndex(M_Corpses[CorpseIndex].BatchIndex))
	{
		return false;
	}
	const FRTSCorpseBatch& Batch = M_Batches[M_Corpses[CorpseIndex].BatchIndex];
	OutLibrary = M_Libraries.IsValidIndex(Batch.LibraryIndex) ? M_Libraries[Batch.LibraryIndex].Get() : nullptr;
	OutPoseIndex = Batch.PoseIndex;
	return IsValid(Batch.Component)
		&& Batch.Component->GetInstanceTransform(M_Corpses[CorpseIndex].InstanceIndex, OutTransform, true);
}

FTransform URTSCorpseSubsystem::GetCorpseTransform(const FRTSCorpsePose& Pose, const FTransform& Pelvis) const
{
	using namespace RTSCorpseSubsystemStatics;
	// The turn about the vertical that best lays the pose's pelvis axes over the body's, weighted by how flat each lies.
	const FVector PoseAxes[] = {Pose.PelvisAxisX, Pose.PelvisAxisY, Pose.PelvisAxisZ};
	const FVector BodyAxes[] = {
		Pelvis.GetUnitAxis(EAxis::X), Pelvis.GetUnitAxis(EAxis::Y), Pelvis.GetUnitAxis(EAxis::Z)
	};
	double Alignment = 0.0;
	double Turn = 0.0;
	for (int32 Axis = 0; Axis < UE_ARRAY_COUNT(PoseAxes); ++Axis)
	{
		Alignment += PoseAxes[Axis].X * BodyAxes[Axis].X + PoseAxes[Axis].Y * BodyAxes[Axis].Y;
		Turn += PoseAxes[Axis].X * BodyAxes[Axis].Y - PoseAxes[Axis].Y * BodyAxes[Axis].X;
	}
	const FQuat Heading(FVector::UpVector, FMath::Atan2(Turn, Alignment));

	const FVector PelvisLocation = Pelvis.GetLocation();
	FVector GroundPoint = PelvisLocation;
	FVector GroundNormal = FVector::UpVector;
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CorpseGround), false);
	if (GetWorld()->LineTraceSingleByObjectType(GroundHit, PelvisLocation + FVector(0.0, 0.0, GroundTraceUpCm),
	                                            PelvisLocation - FVector(0.0, 0.0, GroundTraceDownCm),
	                                            FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams))
	{
		GroundPoint = GroundHit.ImpactPoint;
		GroundNormal = GroundHit.ImpactNormal;
	}
	return FTransform(GetGroundTilt(GroundNormal) * Heading,
	                  FVector(PelvisLocation.X, PelvisLocation.Y, GroundPoint.Z));
}

FQuat URTSCorpseSubsystem::GetGroundTilt(const FVector& GroundNormal) const
{
	const FVector TiltAxis = FVector::CrossProduct(FVector::UpVector, GroundNormal);
	if (TiltAxis.IsNearlyZero())
	{
		return FQuat::Identity;
	}
	const float MaxTiltRadians = FMath::DegreesToRadians(GetDefault<URTSRagdollSettings>()->MaxCorpseTiltDegrees);
	const float TiltRadians = FMath::Min(FMath::Acos(FMath::Clamp(GroundNormal.Z, -1.0, 1.0)), MaxTiltRadians);
	return FQuat(TiltAxis.GetSafeNormal(), TiltRadians);
}

int32 URTSCorpseSubsystem::FindOrCreateBatch(const int32 LibraryIndex, const int32 PoseIndex,
                                             const TArray<UMaterialInterface*>& Materials)
{
	const int32 ExistingBatch = FindBatch(LibraryIndex, PoseIndex, Materials);
	if (ExistingBatch != INDEX_NONE)
	{
		return ExistingBatch;
	}
	const FRTSCorpsePose& Pose = M_Libraries[LibraryIndex]->Poses[PoseIndex];
	if (not IsValid(Pose.Mesh))
	{
		return INDEX_NONE;
	}
	UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(M_Holder);
	Component->SetMobility(EComponentMobility::Movable);
	// Compaction relies on removal shifting later instances down rather than swapping the last one in.
	Component->bSupportRemoveAtSwap = false;
	Component->SetStaticMesh(Pose.Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetGenerateOverlapEvents(false);
	Component->SetCastShadow(GetDefault<URTSRagdollSettings>()->bCorpsesCastShadows);
	// Pose mesh slots match the soldier mesh's slots, so the soldier's materials apply as they are.
	for (int32 Slot = 0; Slot < Materials.Num(); ++Slot)
	{
		if (IsValid(Materials[Slot]))
		{
			Component->SetMaterial(Slot, Materials[Slot]);
		}
	}
	Component->SetupAttachment(M_Holder->GetRootComponent());
	Component->RegisterComponent();
	M_Holder->AddInstanceComponent(Component);

	FRTSCorpseBatch Batch;
	Batch.Component = Component;
	Batch.Materials.Append(Materials);
	Batch.LibraryIndex = LibraryIndex;
	Batch.PoseIndex = PoseIndex;
	return M_Batches.Add(Batch);
}

bool URTSCorpseSubsystem::EnsureHolder()
{
	if (IsValid(M_Holder))
	{
		return true;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_Holder = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (not IsValid(M_Holder))
	{
		return false;
	}
	USceneComponent* Root = NewObject<USceneComponent>(M_Holder);
	Root->SetMobility(EComponentMobility::Movable);
	M_Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	M_Holder->AddInstanceComponent(Root);
	// Batches made before a holder was lost belong to the old one.
	M_Batches.Reset();
	M_Corpses.Reset();
	return true;
}

void URTSCorpseSubsystem::RemoveOldestCorpse()
{
	if (M_Corpses.IsEmpty())
	{
		return;
	}
	const FRTSCorpseSlot Oldest = M_Corpses[0];
	M_Corpses.RemoveAt(0, EAllowShrinking::No);
	if (not M_Batches.IsValidIndex(Oldest.BatchIndex))
	{
		return;
	}
	FRTSCorpseBatch& Batch = M_Batches[Oldest.BatchIndex];
	if (IsValid(Batch.Component))
	{
		Batch.Component->UpdateInstanceTransform(Oldest.InstanceIndex, RTSCorpseSubsystemStatics::HiddenInstance, true,
		                                         true, true);
		Batch.FreeInstances.Add(Oldest.InstanceIndex);
	}
}

void URTSCorpseSubsystem::TrimToCap()
{
	const int32 Cap = GetCorpseCap();
	if (M_Corpses.Num() <= Cap)
	{
		return;
	}
	while (M_Corpses.Num() > Cap)
	{
		RemoveOldestCorpse();
	}
	// A lowered setting should also give back the memory of the corpses it removed.
	CompactBatches();
}
