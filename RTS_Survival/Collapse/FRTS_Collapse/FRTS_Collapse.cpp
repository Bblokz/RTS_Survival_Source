#include "FRTS_Collapse.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "Field/FieldSystemObjects.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

#include "RTS_Survival/Collapse/CollapseBySwapParameters.h"


void FRTS_Collapse::CollapseMesh(
	AActor* CollapseOwner,
	UGeometryCollectionComponent* GeoCollapseComp,
	TSoftObjectPtr<UGeometryCollection> GeoCollection,
	UMeshComponent* MeshToCollapse,
	FCollapseDuration CollapseDuration,
	FCollapseForce CollapseForce,
	FCollapseFX CollapseFX)
{
	if (!GetIsValidCollapseRequest(CollapseOwner, GeoCollapseComp, GeoCollection, MeshToCollapse))
	{
		return;
	}
	// Set geo to be visible instantly; mesh will be invisible once geo is loaded.
	GeoCollapseComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GeoCollapseComp->SetVisibility(true, false);
	MeshToCollapse->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GeoCollapseComp->SetSimulatePhysics(true);

	UWorld* World = CollapseOwner->GetWorld();
	if (!World)
	{
		return;
	}
	TSharedPtr<FCollapseTaskContext> Context = MakeShared<FCollapseTaskContext>(
		CollapseOwner, GeoCollapseComp, MeshToCollapse, GeoCollection, CollapseDuration, CollapseForce, CollapseFX
	);

	FTimerDelegate NextTickDelegate;
	NextTickDelegate.BindStatic(&FRTS_Collapse::HandleCollapseNextTick, Context);
	FTimerHandle NextTickHandle;
	World->GetTimerManager().SetTimer(NextTickHandle, NextTickDelegate, 0.1f, false);
}

void FRTS_Collapse::CollapseSwapMesh(AActor* CollapseOwner, FSwapToDestroyedMesh SwapToDestroyedMeshParameters)
{
	// Create a weak pointer for safe handling in async callbacks.
	TWeakObjectPtr<AActor> WeakOwner(CollapseOwner);

	// Validate that we have a valid owner and a valid component to swap on.
	if (!CollapseOwner || !SwapToDestroyedMeshParameters.ComponentToSwapOn)
	{
		const FString CollapseOwnerName = CollapseOwner ? CollapseOwner->GetName() : "null";
		const FString ComponentName = SwapToDestroyedMeshParameters.ComponentToSwapOn
			                              ? SwapToDestroyedMeshParameters.ComponentToSwapOn->GetName()
			                              : "null";
		const FString MeshName = SwapToDestroyedMeshParameters.MeshToSwapTo
			                         ? SwapToDestroyedMeshParameters.MeshToSwapTo.Get()->GetName()
			                         : "null";
		RTSFunctionLibrary::ReportError(
			"Could not start CollapseSwapMesh as either the owning actor, the mesh component or the mesh to swap to pointer is not valid:"
			"owner: " + CollapseOwnerName +
			"\n ComponentName: " + ComponentName +
			"\n MeshName: " + MeshName);
		return;
	}

	// Begin asynchronous loading of the mesh asset.
	LoadSwapMeshAsset(WeakOwner, SwapToDestroyedMeshParameters);
}


void FRTS_Collapse::LoadSwapMeshAsset(TWeakObjectPtr<AActor> WeakOwner, FSwapToDestroyedMesh SwapParams)
{
	if (!SwapParams.MeshToSwapTo.IsValid())
	{
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamableManager.RequestAsyncLoad(
			SwapParams.MeshToSwapTo.ToSoftObjectPath(),
			FStreamableDelegate::CreateStatic(&FRTS_Collapse::OnSwapMeshAssetLoaded, WeakOwner, SwapParams)
		);
	}
	else
	{
		// Already loaded; proceed immediately.
		OnSwapMeshAssetLoaded(WeakOwner, SwapParams);
	}
}

void FRTS_Collapse::OnSwapMeshAssetLoaded(TWeakObjectPtr<AActor> WeakOwner, FSwapToDestroyedMesh SwapParams)
{
	AActor* Owner = WeakOwner.Get();
	if (!Owner)
	{
		return;
	}
	UStaticMesh* LoadedMesh = SwapParams.MeshToSwapTo.Get();
	if (!LoadedMesh)
	{
		return;
	}

	if (SwapParams.ComponentToSwapOn)
	{
		if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(SwapParams.ComponentToSwapOn))
		{
			StaticMeshComp->SetStaticMesh(LoadedMesh);
		}
	}

	// Determine the effect location using the actor's location plus the provided offset.
	const FVector EffectLocation = Owner->GetActorLocation() + SwapParams.FXOffset;

	if (SwapParams.SwapSound)
	{
		UGameplayStatics::PlaySoundAtLocation(Owner, SwapParams.SwapSound, EffectLocation, FRotator::ZeroRotator, 1, 1,
		                                      0,
		                                      SwapParams.Attenuation, SwapParams.SoundConcurrency, nullptr, nullptr);
	}

	if (SwapParams.SwapFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(Owner->GetWorld(), SwapParams.SwapFX, EffectLocation);
	}
}


bool FRTS_Collapse::GetIsValidCollapseRequest(const AActor* CollapseOwner,
                                              const UGeometryCollectionComponent* GeoCollapseComp,
                                              const TSoftObjectPtr<UGeometryCollection>& GeoCollection,
                                              const UMeshComponent* MeshToCollapse)
{
	if (!IsValid(CollapseOwner) || !IsValid(GeoCollapseComp) || !IsValid(MeshToCollapse))
	{
		const FString OwnerName = IsValid(CollapseOwner) ? CollapseOwner->GetName() : "null";
		const FString GeoName = IsValid(GeoCollapseComp) ? GeoCollapseComp->GetName() : "null";
		FString CollectionName = "Invalid geometry class or null geo-collection";
		if (GeoCollection)
		{
			CollectionName = GeoCollection->GetClass()
				                 ? GeoCollection->GetClass()->GetName()
				                 : "no geo-collection class";
		}
		const FString MeshName = IsValid(MeshToCollapse) ? MeshToCollapse->GetName() : "null";
		RTSFunctionLibrary::ReportError(
			"At FRTS_Collapse::CollapseMesh: One pointer is null or physics is off.\nOwner: " + OwnerName
			+ ", Geo: " + GeoName + ", Collection soft pointer class: " + CollectionName + ", Mesh: " + MeshName +
			"\n Aborting collapse!");

		return false;
	}
	return true;
}

bool FRTS_Collapse::EnsureValidCollapseContext(const TSharedPtr<FCollapseTaskContext>& Context)
{
	if (!Context.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid TSharedPTr of FCOllapseTaskContext!");
		return false;
	}
	if (not Context->WeakOwner.IsValid() || not Context->WeakGeoComp.IsValid() || not Context->WeakMeshToCollapse.
		IsValid())
	{
		FString OwnerName = Context->WeakOwner.IsValid() ? Context->WeakOwner->GetName() : "null";
		FString GeoName = Context->WeakGeoComp.IsValid() ? Context->WeakGeoComp->GetName() : "null";
		const FString MeshName = Context->WeakMeshToCollapse.IsValid()
			                         ? Context->WeakMeshToCollapse->GetName()
			                         : "null";
		RTSFunctionLibrary::ReportError("Invalid owner or geometry component on FCollapseTaskContex:"
			"\n Owner : " + OwnerName + "\n GeoComp: " + GeoName + "\n Mesh: " + MeshName);
		return false;
	}
	return true;
}

void FRTS_Collapse::HandleCollapseNextTick(TSharedPtr<FCollapseTaskContext> Context)
{
	if (not EnsureValidCollapseContext(Context))
	{
		return;
	}

	const TSoftObjectPtr<UGeometryCollection>& GeoCollectionSoft = Context->GeoCollection;
	const FSoftObjectPath AssetPath = GeoCollectionSoft.ToSoftObjectPath();

	// No asset assigned ⇒ nothing to load.
	if (!AssetPath.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("HandleCollapseNextTick: GeoCollection soft reference has no asset path."));
		return;
	}

	// Already resident? Proceed immediately.
	if (GeoCollectionSoft.IsValid())
	{
		OnAssetLoadSucceeded(Context);
		return;
	}

	// Otherwise, async load and continue in the completion callback.
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateStatic(&FRTS_Collapse::OnAssetLoadSucceeded, Context)
	);
}

void FRTS_Collapse::OnAssetLoadSucceeded(TSharedPtr<FCollapseTaskContext> Context)
{
	if (!EnsureValidCollapseContext(Context))
	{
		return;
	}
	AActor* Owner = Context->WeakOwner.Get();


	UMeshComponent* MeshToCollapse = Context->WeakMeshToCollapse.Get();
	MeshToCollapse->SetVisibility(false, false);

	UGeometryCollectionComponent* GeoComp = Context->WeakGeoComp.Get();
	UGeometryCollection* LoadedAsset = Context->GeoCollection.Get();

	// If geometry is missing, bail out
	if (!IsValid(Owner) || !IsValid(GeoComp) || !IsValid(LoadedAsset))
	{
		RTSFunctionLibrary::ReportError("Collapse asset loaded but was invalid!");
		return;
	}
	MeshToCollapse->SetVisibility(false, false);

	GeoComp->SetRestCollection(LoadedAsset);
	GeoComp->SetSimulatePhysics(true);
	const FVector GeoCompLoation = GeoComp->GetComponentLocation();
	const FVector ForceLocation = GeoCompLoation + Context->CollapseForce.ForceLocationOffset;

	// Create a radial falloff (strain) to break the cluster
	URadialFalloff* StrainFalloff = NewObject<URadialFalloff>(GeoComp);
	StrainFalloff->Magnitude = Context->CollapseForce.Force;
	StrainFalloff->Radius = Context->CollapseForce.Radius;
	StrainFalloff->Position = ForceLocation;
	StrainFalloff->Falloff = EFieldFalloffType::Field_Falloff_Linear;
	StrainFalloff->MinRange = 0.0f;
	StrainFalloff->MaxRange = 1.0f;
	StrainFalloff->Default = 0.0f;

	GeoComp->ApplyPhysicsField(
		/*Enabled=*/true,
		            EGeometryCollectionPhysicsTypeEnum::Chaos_ExternalClusterStrain,
		            /*MetaData=*/nullptr,
		            StrainFalloff
	);

	SpawnCollapseSFX(Context->CollapseFX, Owner->GetWorld(), GeoCompLoation);

	SpawnCollapseVFX(Context->CollapseFX, Owner->GetWorld(), GeoCompLoation);

	FTimerHandle DestroyHandle;
	FTimerDelegate DestroyDelegate;
	DestroyDelegate.BindStatic(&FRTS_Collapse::HandleDestroyGeometry, Context);
	Owner->GetWorldTimerManager().SetTimer(
		DestroyHandle,
		DestroyDelegate,
		Context->CollapseDuration.CollapsedGeometryLifeTime,
		false
	);
}


void FRTS_Collapse::HandleDestroyGeometry(TSharedPtr<FCollapseTaskContext> Context)
{
	if (!Context.IsValid()) return;
	AActor* Owner = Context->WeakOwner.Get();
	UGeometryCollectionComponent* GeoComp = Context->WeakGeoComp.Get();
	const bool bKeepGeoComponent = Context->CollapseDuration.bKeepGeometryVisibleAfterLifeTime;
	if (not bKeepGeoComponent && IsValid(GeoComp))
	{
		GeoComp->DestroyComponent();
	}
	if (Context->CollapseDuration.bDestroyOwningActorAfterCollapse && IsValid(Owner))
	{
		Owner->Destroy();
	}
}

void FRTS_Collapse::OnDestroySpawnActors(AActor* Owner, const FDestroySpawnActorsParameters& SpawnParams,
                                         FCollapseFX CollapseFX)
{
	// Safety: ensure the owner is valid.
	if (!IsValid(Owner))
	{
		return;
	}

	// Create a context to track the async loading and spawn process.
	TSharedPtr<FOnDestroySpawnActorsContext> Context = MakeShared<FOnDestroySpawnActorsContext>();
	Context->SpawnParams = SpawnParams;
	Context->WeakOwner = Owner;
	Context->CollapseFX = CollapseFX;

	// Build an array of asset paths for all actors that need to be loaded.
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FDestroySpawnActor& ActorDef : SpawnParams.ActorsToSpawn)
	{
		// If the soft pointer is not valid (i.e. not already loaded), add its asset path.
		if (!ActorDef.ActorToSpawn.IsValid())
		{
			AssetsToLoad.Add(ActorDef.ActorToSpawn.ToSoftObjectPath());
		}
	}

	// If nothing needs loading, spawn immediately.
	if (AssetsToLoad.Num() == 0)
	{
		SpawnActorsFromContext(Context);
		if (FMath::IsNearlyZero(SpawnParams.DelayTillDestroyActor))
		{
			Owner->Destroy();
			return;
		}
		// Set a timer to destroy the owner after the specified delay.
		if (UWorld* World = Owner->GetWorld())
		{
			FTimerHandle DestroyTimer;
			FTimerDelegate TimerDelegate = FTimerDelegate::CreateStatic(&FRTS_Collapse::HandleDestroyOwner,
			                                                            TWeakObjectPtr<AActor>(Owner));
			World->GetTimerManager().SetTimer(DestroyTimer, TimerDelegate, SpawnParams.DelayTillDestroyActor, false);
		}
		return;
	}

	// Otherwise, asynchronously load all actor classes.
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(AssetsToLoad,
	                            FStreamableDelegate::CreateStatic(&FRTS_Collapse::OnSpawnActorsAssetsLoaded, Context));
}

void FRTS_Collapse::OnSpawnActorsAssetsLoaded(TSharedPtr<FOnDestroySpawnActorsContext> Context)
{
	// Ensure the context and owning actor are still valid.
	if (!Context.IsValid() || not Context->WeakOwner.IsValid())
	{
		return;
	}


	// Spawn the actors using the loaded classes.
	SpawnActorsFromContext(Context);
	if (FMath::IsNearlyZero(Context->SpawnParams.DelayTillDestroyActor))
	{
		Context->WeakOwner->Destroy();
		return;
	}

	// Schedule the destruction of the owner actor after the specified delay.
	if (UWorld* World = Context->WeakOwner->GetWorld())
	{
		FTimerHandle DestroyTimer;
		FTimerDelegate TimerDelegate = FTimerDelegate::CreateStatic(&FRTS_Collapse::HandleDestroyOwner,
		                                                            Context->WeakOwner);
		World->GetTimerManager().SetTimer(DestroyTimer, TimerDelegate, Context->SpawnParams.DelayTillDestroyActor,
		                                  false);
	}
}


void FRTS_Collapse::SpawnActorsFromContext(TSharedPtr<FOnDestroySpawnActorsContext> Context)
{
	if (!Context.IsValid() || !Context->WeakOwner.IsValid())
	{
		return;
	}
	AActor* Owner = Context->WeakOwner.Get();
	UWorld* World = Owner->GetWorld();
	if (!World)
	{
		return;
	}

	// Iterate over each actor to spawn.
	for (const FDestroySpawnActor& ActorDef : Context->SpawnParams.ActorsToSpawn)
	{
		// Retrieve the actor class (it should now be loaded).
		UClass* ActorClass = ActorDef.ActorToSpawn.Get();
		if (!ActorClass)
		{
			RTSFunctionLibrary::ReportError(TEXT("OnDestroySpawnActors: ActorToSpawn is invalid or failed to load."));
			continue;
		}

		// Compute the final spawn transform.
		// The location is the owner’s location plus the offset from the spawn transform.
		const FVector SpawnLocation = Owner->GetActorLocation() + ActorDef.SpawnTransform.GetLocation();
		const FRotator SpawnRotation = ActorDef.SpawnTransform.GetRotation().Rotator();
		const FVector SpawnScale = ActorDef.SpawnTransform.GetScale3D();
		FTransform FinalTransform(SpawnRotation, SpawnLocation, SpawnScale);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Spawn the actor.
		AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, FinalTransform, SpawnParameters);
		if (!SpawnedActor)
		{
			RTSFunctionLibrary::ReportError(TEXT("OnDestroySpawnActors: Failed to spawn actor."));
		}
	}
	// create vfx and sfx
	SpawnCollapseVFX(Context->CollapseFX, World, Owner->GetActorLocation());
	SpawnCollapseSFX(Context->CollapseFX, World, Owner->GetActorLocation());
}

void FRTS_Collapse::HandleDestroyOwner(TWeakObjectPtr<AActor> Owner)
{
	if (AActor* Actor = Owner.Get())
	{
		Actor->Destroy();
	}
}

void FRTS_Collapse::SpawnCollapseVFX(const FCollapseFX& CollapseFX, UWorld* World, const FVector& BaseLocationFX)
{
	if (IsValid(CollapseFX.CollapseVfx) && World)
	{
		const FVector VfxLocation = BaseLocationFX + CollapseFX.FxLocationOffset;
		const FTransform NiagTransform(CollapseFX.VfxRotation, VfxLocation, CollapseFX.VfxScale);
		auto Niagara = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, CollapseFX.CollapseVfx, NiagTransform.GetLocation(),
			NiagTransform.GetRotation().Rotator(), NiagTransform.GetScale3D(), true, true,
			ENCPoolMethod::None, true
		);
		CollapseFX.AdjustFX(Niagara);
	}
}

void FRTS_Collapse::SpawnCollapseSFX(const FCollapseFX& CollapseFX,
                                     UWorld* World, const FVector& BaseLocationFX)
{
	if (IsValid(CollapseFX.CollapseSfx))
	{
		const FVector SfxLocation = BaseLocationFX + CollapseFX.FxLocationOffset;
		USoundAttenuation* Attenuation = CollapseFX.SfxAttenuation;
		USoundConcurrency* Concurrency = CollapseFX.SfxConcurrency;
		if (!IsValid(Attenuation) || !IsValid(Concurrency))
		{
			RTSFunctionLibrary::ReportError("Invalid sound attenuation/concurrency in collapse effect");
		}
		UGameplayStatics::PlaySoundAtLocation(
			World, CollapseFX.CollapseSfx, SfxLocation,
			CollapseFX.VfxRotation, 1.f, 1.f, 0.f,
			Attenuation, Concurrency
		);
	}
}
