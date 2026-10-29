// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteBlightSubsystem.h"

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/World.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#if WITH_EDITOR
#include "Containers/Ticker.h"
#include "Editor.h"
#include "LandscapeProxy.h"
#include "LandscapeSubsystem.h"
#include "Materials/MaterialInterface.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogRTSRadixiteBlight, Log, All);

namespace RadixiteBlightPrivate
{
	const TCHAR* const ParameterCollectionPath =
		TEXT("/Game/RTS_Survival/Materials/ground/Landscape/MPC_RadixiteBlight.MPC_RadixiteBlight");

	// Must match MAX_BLOBS in Source/Docs/Landscape/ls_blight_build.py: the circles the shader tests per pixel.
	constexpr int32 MaxBlobs = 8;

	// Dead ground around a crystal, from its static meshes' footprint: a few crystal widths, within sensible limits
	// (cm). Only the static meshes: the destruction geometry collection's bounds span tens of metres and differ
	// between editor sessions, commandlets and game runs, which would change the grass fingerprint at random.
	constexpr float RadiusPerFootprint = 3.0f;
	constexpr float MinCrystalRadius = 500.f;
	constexpr float MaxCrystalRadius = 1600.f;

	// The shader's edge noise pushes the rim out by up to this fraction of a circle's radius; the bounds keep room.
	constexpr float BoundsMarginFraction = 0.5f;

	// Below this a crystal sits on its neighbour's centre and the merged circle just takes the larger one.
	constexpr float CoincidentDistance = 1.f;

	// The grass fingerprint rounds circles to this many cm, so float noise never reads as a changed blight.
	constexpr float FingerprintStep = 10.f;

	// Only meshes the class itself defines: URadixiteGrowthComponent spawns extra meshes at random on every load, and a
	// construction script may randomise too, which would make the circles (and the grass fingerprint) differ each time.
	bool IsDesignedComponent(const UActorComponent& Component)
	{
		return Component.CreationMethod == EComponentCreationMethod::Native
			|| Component.CreationMethod == EComponentCreationMethod::SimpleConstructionScript;
	}

	// Footprint of the actor's designed static meshes: the largest mesh asset's bounding sphere times the actor's own
	// (saved) scale. The editor reruns construction scripts on every load, and the crystals' randomise their meshes'
	// rotation and scale there, so world-space component bounds would change the circles from load to load.
	float GetStaticMeshFootprint(const AActor& Actor)
	{
		float SphereRadius = 0.f;
		TInlineComponentArray<UStaticMeshComponent*> Meshes(&Actor);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->GetStaticMesh() && IsDesignedComponent(*Mesh))
			{
				SphereRadius = FMath::Max(SphereRadius, Mesh->GetStaticMesh()->GetBounds().SphereRadius);
			}
		}
		return SphereRadius * Actor.GetActorScale3D().GetAbsMax();
	}

	const FName CountParameter = TEXT("RadixiteBlightCount");
	const FName BoundsParameter = TEXT("RadixiteBlightBounds");

	FName BlobParameter(const int32 Index)
	{
		return FName(*FString::Printf(TEXT("RadixiteBlight%d"), Index));
	}

	bool IsRadixiteActor(const AActor& Actor)
	{
		TInlineComponentArray<UResourceComponent*> ResourceComponents(&Actor);
		for (const UResourceComponent* ResourceComponent : ResourceComponents)
		{
			if (IsValid(ResourceComponent) && ResourceComponent->GetResourceType() == ERTSResourceType::Resource_Radixite)
			{
				return true;
			}
		}
		return false;
	}

#if WITH_EDITOR
	// Landscape actor tag that records the blight its grass maps were baked with: prefix + fingerprint in hex.
	const FString BakedFingerprintTagPrefix = TEXT("RadixiteBlightGrass_");

	// The landscape master's static switches: only a landscape that shows both can grow grass on blighted ground.
	const FName BlightSwitch = TEXT("LS_RadixiteBlight");
	const FName GrassSwitch = TEXT("LS_GroundGrass");

	const TCHAR* const GrassWarningText =
		TEXT("RADIXITE GRASS NEEDS TO BE REBUILT (radixite crystals changed): run RTS.Radixite.RebuildGrass, then "
			"save the map");

	bool GetStaticSwitch(const UMaterialInterface& Material, const FName SwitchName)
	{
		bool bValue = false;
		FGuid ExpressionGuid;
		return Material.GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(SwitchName), bValue, ExpressionGuid)
			&& bValue;
	}

	FAutoConsoleCommandWithWorld RebuildGrassCommand(
		TEXT("RTS.Radixite.RebuildGrass"),
		TEXT("Rebuilds the landscape grass maps with the current radixite blight and records it; save the map after."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const bool bRebuilt = URadixiteBlightSubsystem::RebuildBlightGrass(World);
			UE_LOG(LogRTSRadixiteBlight, Log, TEXT("RebuildGrass: %s"), bRebuilt ? TEXT("done") : TEXT("failed"));
		}));
#endif

	FAutoConsoleCommandWithWorld RefreshBlightCommand(
		TEXT("RTS.Radixite.RefreshBlight"),
		TEXT("Recomputes the dead ground around radixite crystals (MPC_RadixiteBlight) for this world."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const int32 BlobCount = URadixiteBlightSubsystem::RefreshBlight(World);
			UE_LOG(LogRTSRadixiteBlight, Log, TEXT("RefreshBlight: %d circles"), BlobCount);
		}));
}

bool URadixiteBlightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || IsRunningDedicatedServer())
	{
		return false;
	}
	// Editor worlds too: the editor view and the landscape grass bake read the collection there.
	return OuterWorld->WorldType == EWorldType::Game || OuterWorld->WorldType == EWorldType::PIE
		|| OuterWorld->WorldType == EWorldType::Editor;
}

void URadixiteBlightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	M_ParameterCollection = LoadObject<UMaterialParameterCollection>(
		nullptr, RadixiteBlightPrivate::ParameterCollectionPath);
#if WITH_EDITOR
	if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		Initialize_RegisterEditorDelegates();
	}
#endif
}

void URadixiteBlightSubsystem::Deinitialize()
{
#if WITH_EDITOR
	Deinitialize_UnregisterEditorDelegates();
#endif
	Super::Deinitialize();
}

void URadixiteBlightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Refresh();
}

int32 URadixiteBlightSubsystem::RefreshBlight(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	URadixiteBlightSubsystem* Subsystem = World ? World->GetSubsystem<URadixiteBlightSubsystem>() : nullptr;
	if (not IsValid(Subsystem))
	{
		return -1;
	}
	return Subsystem->Refresh();
}

bool URadixiteBlightSubsystem::RebuildBlightGrass(const UObject* WorldContextObject)
{
#if WITH_EDITOR
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	URadixiteBlightSubsystem* Subsystem = World ? World->GetSubsystem<URadixiteBlightSubsystem>() : nullptr;
	return IsValid(Subsystem) && Subsystem->RebuildGrass();
#else
	return false;
#endif
}

int32 URadixiteBlightSubsystem::Refresh()
{
	if (not GetIsValidParameterCollection())
	{
		return 0;
	}
	TArray<FRadixiteBlightBlob> Blobs = GatherCrystalBlobs();
	const int32 CrystalCount = Blobs.Num();
	SortByPosition(Blobs);
	MergeToShaderLimit(Blobs);
	// Merging swaps circles around: sort again so the collection and the fingerprint never depend on actor order.
	SortByPosition(Blobs);
	WriteBlobs(Blobs);
#if WITH_EDITOR
	M_EditorState.CurrentFingerprint = ComputeFingerprint(Blobs);
	M_EditorState.Landscape = FindLandscape();
#endif
	UE_LOG(LogRTSRadixiteBlight, Log, TEXT("%s: %d radixite crystals in %d blight circles (fingerprint %08x)"),
	       *GetWorld()->GetMapName(), CrystalCount, Blobs.Num(), ComputeFingerprint(Blobs));
	for (const FRadixiteBlightBlob& Blob : Blobs)
	{
		UE_LOG(LogRTSRadixiteBlight, Verbose, TEXT("  circle %s radius %.1f"), *Blob.Centre.ToString(), Blob.Radius);
	}
	return Blobs.Num();
}

bool URadixiteBlightSubsystem::GetIsValidParameterCollection() const
{
	if (IsValid(M_ParameterCollection))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Radixite blight: parameter collection %s is missing."
			"\n at function: URadixiteBlightSubsystem::GetIsValidParameterCollection"),
		RadixiteBlightPrivate::ParameterCollectionPath));
	return false;
}

TArray<FRadixiteBlightBlob> URadixiteBlightSubsystem::GatherCrystalBlobs() const
{
	using namespace RadixiteBlightPrivate;
	TArray<FRadixiteBlightBlob> Blobs;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Actor = *It;
		if (not IsValid(Actor) || not IsRadixiteActor(*Actor))
		{
			continue;
		}
		const float Footprint = GetStaticMeshFootprint(*Actor);
		FRadixiteBlightBlob& Blob = Blobs.AddDefaulted_GetRef();
		Blob.Centre = FVector2D(Actor->GetActorLocation());
		Blob.Radius = FMath::Clamp(Footprint * RadiusPerFootprint, MinCrystalRadius, MaxCrystalRadius);
	}
	return Blobs;
}

void URadixiteBlightSubsystem::SortByPosition(TArray<FRadixiteBlightBlob>& Blobs)
{
	Blobs.Sort([](const FRadixiteBlightBlob& First, const FRadixiteBlightBlob& Second)
	{
		return First.Centre.X != Second.Centre.X ? First.Centre.X < Second.Centre.X : First.Centre.Y < Second.Centre.Y;
	});
}

void URadixiteBlightSubsystem::MergeToShaderLimit(TArray<FRadixiteBlightBlob>& Blobs)
{
	while (Blobs.Num() > RadixiteBlightPrivate::MaxBlobs)
	{
		int32 BestFirst = 0;
		int32 BestSecond = 1;
		float BestRadius = TNumericLimits<float>::Max();
		for (int32 First = 0; First < Blobs.Num(); ++First)
		{
			for (int32 Second = First + 1; Second < Blobs.Num(); ++Second)
			{
				const float MergedRadius = EnclosingBlob(Blobs[First], Blobs[Second]).Radius;
				if (MergedRadius < BestRadius)
				{
					BestRadius = MergedRadius;
					BestFirst = First;
					BestSecond = Second;
				}
			}
		}
		Blobs[BestFirst] = EnclosingBlob(Blobs[BestFirst], Blobs[BestSecond]);
		Blobs.RemoveAtSwap(BestSecond);
	}
}

FRadixiteBlightBlob URadixiteBlightSubsystem::EnclosingBlob(const FRadixiteBlightBlob& First,
                                                            const FRadixiteBlightBlob& Second)
{
	const FVector2D ToSecond = Second.Centre - First.Centre;
	const float Distance = ToSecond.Size();
	if (Distance + Second.Radius <= First.Radius)
	{
		return First;
	}
	if (Distance + First.Radius <= Second.Radius || Distance < RadixiteBlightPrivate::CoincidentDistance)
	{
		return First.Radius >= Second.Radius ? First : Second;
	}
	FRadixiteBlightBlob Merged;
	Merged.Radius = (Distance + First.Radius + Second.Radius) * 0.5f;
	Merged.Centre = First.Centre + ToSecond / Distance * (Merged.Radius - First.Radius);
	return Merged;
}

void URadixiteBlightSubsystem::WriteBlobs(const TArray<FRadixiteBlightBlob>& Blobs) const
{
	using namespace RadixiteBlightPrivate;
	UWorld* World = GetWorld();
	FBox2D Bounds(ForceInit);
	for (int32 Index = 0; Index < MaxBlobs; ++Index)
	{
		FLinearColor Value(0.f, 0.f, 0.f, 0.f);
		if (Blobs.IsValidIndex(Index))
		{
			const FRadixiteBlightBlob& Blob = Blobs[Index];
			// W = 1 / radius^2: the shader compares squared distances and takes one square root.
			Value = FLinearColor(Blob.Centre.X, Blob.Centre.Y, Blob.Radius, 1.f / FMath::Square(Blob.Radius));
			const FVector2D Reach(Blob.Radius * (1.f + BoundsMarginFraction));
			Bounds += FBox2D(Blob.Centre - Reach, Blob.Centre + Reach);
		}
		UKismetMaterialLibrary::SetVectorParameterValue(World, M_ParameterCollection, BlobParameter(Index), Value);
	}
	const FLinearColor BoundsValue = Bounds.bIsValid
		                                 ? FLinearColor(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y)
		                                 : FLinearColor(0.f, 0.f, 0.f, 0.f);
	UKismetMaterialLibrary::SetVectorParameterValue(World, M_ParameterCollection, BoundsParameter, BoundsValue);
	UKismetMaterialLibrary::SetScalarParameterValue(World, M_ParameterCollection, CountParameter,
	                                                static_cast<float>(Blobs.Num()));
}

uint32 URadixiteBlightSubsystem::ComputeFingerprint(const TArray<FRadixiteBlightBlob>& Blobs)
{
	using RadixiteBlightPrivate::FingerprintStep;
	uint32 Fingerprint = 0;
	for (const FRadixiteBlightBlob& Blob : Blobs)
	{
		Fingerprint = HashCombine(Fingerprint, GetTypeHash(FMath::RoundToInt(Blob.Centre.X / FingerprintStep)));
		Fingerprint = HashCombine(Fingerprint, GetTypeHash(FMath::RoundToInt(Blob.Centre.Y / FingerprintStep)));
		Fingerprint = HashCombine(Fingerprint, GetTypeHash(FMath::RoundToInt(Blob.Radius / FingerprintStep)));
	}
	return Fingerprint;
}

#if WITH_EDITOR
void URadixiteBlightSubsystem::Initialize_RegisterEditorDelegates()
{
	M_EditorState.MapOpenedHandle = FEditorDelegates::OnMapOpened.AddUObject(this,
		&URadixiteBlightSubsystem::OnEditorMapOpened);
	M_EditorState.OnScreenMessagesHandle = FCoreDelegates::OnGetOnScreenMessages.AddUObject(this,
		&URadixiteBlightSubsystem::AddGrassWarning);
	if (not GEngine)
	{
		return;
	}
	M_EditorState.ActorAddedHandle = GEngine->OnLevelActorAdded().AddUObject(this,
		&URadixiteBlightSubsystem::OnEditorActorChanged);
	M_EditorState.ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this,
		&URadixiteBlightSubsystem::OnEditorActorChanged);
	M_EditorState.ActorMovedHandle = GEngine->OnActorMoved().AddUObject(this,
		&URadixiteBlightSubsystem::OnEditorActorChanged);
}

void URadixiteBlightSubsystem::Deinitialize_UnregisterEditorDelegates()
{
	FEditorDelegates::OnMapOpened.Remove(M_EditorState.MapOpenedHandle);
	FCoreDelegates::OnGetOnScreenMessages.Remove(M_EditorState.OnScreenMessagesHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(M_EditorState.DeferredRefreshHandle);
	if (not GEngine)
	{
		return;
	}
	GEngine->OnLevelActorAdded().Remove(M_EditorState.ActorAddedHandle);
	GEngine->OnLevelActorDeleted().Remove(M_EditorState.ActorDeletedHandle);
	GEngine->OnActorMoved().Remove(M_EditorState.ActorMovedHandle);
}

void URadixiteBlightSubsystem::OnEditorMapOpened(const FString& Filename, bool bAsTemplate)
{
	Refresh();
}

void URadixiteBlightSubsystem::OnEditorActorChanged(AActor* Actor)
{
	if (not IsValid(Actor) || Actor->GetWorld() != GetWorld() || not RadixiteBlightPrivate::IsRadixiteActor(*Actor))
	{
		return;
	}
	RequestDeferredRefresh();
}

void URadixiteBlightSubsystem::RequestDeferredRefresh()
{
	if (M_EditorState.DeferredRefreshHandle.IsValid())
	{
		return;
	}
	TWeakObjectPtr<URadixiteBlightSubsystem> WeakThis(this);
	M_EditorState.DeferredRefreshHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakThis](float)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->M_EditorState.DeferredRefreshHandle.Reset();
				WeakThis->Refresh();
			}
			return false;
		}));
}

bool URadixiteBlightSubsystem::RebuildGrass()
{
	Refresh();
	ULandscapeSubsystem* LandscapeSubsystem = GetWorld()->GetSubsystem<ULandscapeSubsystem>();
	ALandscapeProxy* Landscape = M_EditorState.Landscape.Get();
	if (not IsValid(LandscapeSubsystem) || not IsValid(Landscape))
	{
		RTSFunctionLibrary::ReportError(TEXT("Radixite blight: this world has no landscape to rebuild grass for."
			"\n at function: URadixiteBlightSubsystem::RebuildGrass"));
		return false;
	}
	LandscapeSubsystem->BuildGrassMaps();
	StoreBakedFingerprint(*Landscape);
	return true;
}

void URadixiteBlightSubsystem::AddGrassWarning(FCoreDelegates::FSeverityMessageMap& OutMessages) const
{
	const ALandscapeProxy* Landscape = M_EditorState.Landscape.Get();
	if (not IsValid(Landscape) || ReadBakedFingerprint(*Landscape) == M_EditorState.CurrentFingerprint
		|| not GetDoesLandscapeGrowBlightedGrass(*Landscape))
	{
		return;
	}
	OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Error,
	                FText::FromString(RadixiteBlightPrivate::GrassWarningText));
}

bool URadixiteBlightSubsystem::GetDoesLandscapeGrowBlightedGrass(const ALandscapeProxy& Landscape) const
{
	using namespace RadixiteBlightPrivate;
	const UMaterialInterface* Material = Landscape.GetLandscapeMaterial();
	return IsValid(Material) && GetStaticSwitch(*Material, BlightSwitch) && GetStaticSwitch(*Material, GrassSwitch);
}

ALandscapeProxy* URadixiteBlightSubsystem::FindLandscape() const
{
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

uint32 URadixiteBlightSubsystem::ReadBakedFingerprint(const ALandscapeProxy& Landscape)
{
	for (const FName& Tag : Landscape.Tags)
	{
		const FString TagText = Tag.ToString();
		if (TagText.StartsWith(RadixiteBlightPrivate::BakedFingerprintTagPrefix))
		{
			return FParse::HexNumber(*TagText.RightChop(RadixiteBlightPrivate::BakedFingerprintTagPrefix.Len()));
		}
	}
	// Never baked: matches a map without radixite (fingerprint 0), so only maps with crystals warn.
	return 0;
}

void URadixiteBlightSubsystem::StoreBakedFingerprint(ALandscapeProxy& Landscape) const
{
	using namespace RadixiteBlightPrivate;
	Landscape.Modify();
	Landscape.Tags.RemoveAll([](const FName& Tag)
	{
		return Tag.ToString().StartsWith(BakedFingerprintTagPrefix);
	});
	const FString FingerprintText = FString::Printf(TEXT("%08x"), M_EditorState.CurrentFingerprint);
	Landscape.Tags.Add(FName(BakedFingerprintTagPrefix + FingerprintText));
}
#endif
