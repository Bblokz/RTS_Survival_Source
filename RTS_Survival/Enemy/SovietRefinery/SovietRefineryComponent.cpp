// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietRefineryComponent.h"

#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "SovietRefineryDropOff.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/UnloadStation/ResourceUnloadStation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovietRefinery, Log, All);

namespace SovietRefineryPrivate
{
	const FName ExitTag = TEXT("SovRefHarvesterExit");
	const FString FillTagPrefix = TEXT("SovRefFill_");
	const FString BarTagPrefix = TEXT("SovRefBar_");
	const FName AdoptedTag = TEXT("SovRefHarvester");
	constexpr float WaitForStandingSeconds = 1.f;
	constexpr float HarvesterThinkSeconds = 2.f;
	constexpr float ExitProjectionExtent = 600.f;
	constexpr float WideExitProjectionExtent = 2500.f;

	bool ParseStockTag(const FName Tag, const FString& Prefix, ERTSResourceType& OutResource, int32& OutOrder)
	{
		FString Text = Tag.ToString();
		if (not Text.StartsWith(Prefix))
		{
			return false;
		}
		Text.RightChopInline(Prefix.Len());
		FString ResourceText;
		FString OrderText;
		if (not Text.Split(TEXT("_"), &ResourceText, &OrderText))
		{
			ResourceText = Text;
			OrderText = TEXT("0");
		}
		if (ResourceText.Equals(TEXT("Radixite"), ESearchCase::IgnoreCase))
		{
			OutResource = ERTSResourceType::Resource_Radixite;
		}
		else if (ResourceText.Equals(TEXT("Metal"), ESearchCase::IgnoreCase))
		{
			OutResource = ERTSResourceType::Resource_Metal;
		}
		else
		{
			return false;
		}
		OutOrder = FCString::Atoi(*OrderText);
		return true;
	}
}

USovietRefineryComponent::USovietRefineryComponent()
{
	// Only ticks to move a tripper car; enabled when one is found.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.033f;
}

void USovietRefineryComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	USceneComponent* Tripper = M_Tripper.Get();
	if (not IsValid(Tripper))
	{
		SetComponentTickEnabled(false);
		return;
	}
	const bool bHasStock = GetStock(ERTSResourceType::Resource_Radixite) + GetStock(ERTSResourceType::Resource_Metal) > 0;
	// Finish the current run before parking at the start.
	const bool bAtStart = FMath::Fmod(M_TripperPhase, 2.f) < KINDA_SMALL_NUMBER;
	if (not bHasStock && bAtStart)
	{
		return;
	}
	M_TripperPhase = FMath::Fmod(M_TripperPhase + DeltaTime / FMath::Max(TripperRunSeconds, 1.f), 2.f);
	const float Alpha = 0.5f - 0.5f * FMath::Cos(M_TripperPhase * PI);
	Tripper->SetRelativeLocation(FMath::Lerp(TripperRailStart, TripperRailEnd, Alpha));
}

void USovietRefineryComponent::BeginPlay()
{
	Super::BeginPlay();
	TryActivate();
}

void USovietRefineryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	// The station is attached, not owned by the building's lifetime: take it down with the refinery (stock is lost).
	if (AResourceUnloadStation* Station = M_UnloadStation.Get())
	{
		Station->Destroy();
	}
	M_UnloadStation.Reset();
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------------------------------------------------ setup

void USovietRefineryComponent::TryActivate()
{
	if (bM_IsActive)
	{
		return;
	}
	if (GetIsOwnerStanding())
	{
		ActivateRefinery();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_WaitForStandingTimer, this, &USovietRefineryComponent::TryActivate,
		                                  SovietRefineryPrivate::WaitForStandingSeconds, false);
	}
}

bool USovietRefineryComponent::GetIsOwnerStanding() const
{
	const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(GetOwner());
	return not IsValid(Expansion) || Expansion->GetIsStanding();
}

void USovietRefineryComponent::ActivateRefinery()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	// The expansion's BuildingMeshComponent (protected): found by its component name.
	TArray<UStaticMeshComponent*> StaticMeshes;
	Owner->GetComponents<UStaticMeshComponent>(StaticMeshes);
	for (UStaticMeshComponent* StaticMesh : StaticMeshes)
	{
		if (StaticMesh->GetFName() == TEXT("BuildingMeshComponent"))
		{
			M_BuildingMesh = StaticMesh;
			break;
		}
	}
	if (not M_BuildingMesh.IsValid())
	{
		M_BuildingMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	}
	if (not CreateDropOff())
	{
		return;
	}
	bM_IsActive = true;
	SpawnUnloadStation();
	CollectStockVisuals();
	UpdateStockVisuals();
	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component->ComponentHasTag(TEXT("SovRefTripper")) && not TripperRailStart.Equals(TripperRailEnd))
		{
			M_Tripper = Component;
			SetComponentTickEnabled(true);
			break;
		}
	}
	AdoptNearbyHarvesters();

	UWorld* World = GetWorld();
	World->GetTimerManager().SetTimer(M_ConversionTimer, this, &USovietRefineryComponent::ConvertStock,
	                                  ConversionIntervalSeconds, true);
	World->GetTimerManager().SetTimer(M_HarvesterTimer, this, &USovietRefineryComponent::HarvesterThink,
	                                  SovietRefineryPrivate::HarvesterThinkSeconds, true, 0.1f);
	UE_LOG(LogSovietRefinery, Log, TEXT("SOVREF_ACTIVE refinery=%s owner=%d max_harvesters=%d station=%s visuals=%d"),
	       *GetOwnerNameForLog(), FRTSPlayers::GetOwningPlayerOfActor(Owner), GetMaxHarvesters(),
	       *GetNameSafe(M_UnloadStation.Get()), M_StockVisuals.Num());
}

bool USovietRefineryComponent::CreateDropOff()
{
	AActor* Owner = GetOwner();
	if (not M_BuildingMesh.IsValid())
	{
		RTSFunctionLibrary::ReportError("Soviet refinery " + GetOwnerNameForLog() + " has no building mesh.");
		return false;
	}
	// The name keeps UResourceDropOff from asking its owner for IResourceStorageOwner (buildings don't have it).
	M_DropOff = NewObject<USovietRefineryDropOff>(Owner, TEXT("ResourceDropOff"));
	M_DropOff->RegisterComponent();
	Owner->AddInstanceComponent(M_DropOff);
	TMap<ERTSResourceType, int32> Capacity;
	Capacity.Add(ERTSResourceType::Resource_Radixite, RadixiteStockCapacity);
	Capacity.Add(ERTSResourceType::Resource_Metal, MetalStockCapacity);
	M_DropOff->InitRefineryDropOff(Capacity, M_BuildingMesh.Get(), TEXT("DropOff"));
	return true;
}

void USovietRefineryComponent::SpawnUnloadStation()
{
	UWorld* World = GetWorld();
	UStaticMeshComponent* BuildingMesh = M_BuildingMesh.Get();
	if (not UnloadStationClass || not IsValid(World) || not IsValid(BuildingMesh) || not IsValid(M_DropOff))
	{
		UE_LOG(LogSovietRefinery, Warning, TEXT("SOVREF refinery=%s has no unload station class: harvesters unload "
			       "the plain way."), *GetOwnerNameForLog());
		return;
	}
	const FTransform MeshTransform = BuildingMesh->GetComponentTransform();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AResourceUnloadStation* Station = World->SpawnActor<AResourceUnloadStation>(
		UnloadStationClass, MeshTransform.GetLocation(), MeshTransform.Rotator(), SpawnParameters);
	if (not IsValid(Station))
	{
		RTSFunctionLibrary::ReportError("Soviet refinery " + GetOwnerNameForLog() + " failed to spawn its crane.");
		return;
	}
	// Its frame is the building mesh's frame: dock, drops and crane are authored in building centimetres.
	Station->AttachToComponent(BuildingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Station->InitUnloadStation(M_DropOff, BuildingMesh);
	M_DropOff->SetUnloadStation(Station);
	M_UnloadStation = Station;
}

void USovietRefineryComponent::CollectStockVisuals()
{
	M_StockVisuals.Reset();
	TArray<UStaticMeshComponent*> Meshes;
	GetOwner()->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		for (const FName Tag : Mesh->ComponentTags)
		{
			FSovietRefineryStockVisual Visual;
			if (SovietRefineryPrivate::ParseStockTag(Tag, SovietRefineryPrivate::FillTagPrefix, Visual.Resource,
			                                         Visual.Order)
				|| SovietRefineryPrivate::ParseStockTag(Tag, SovietRefineryPrivate::BarTagPrefix, Visual.Resource,
				                                        Visual.Order))
			{
				Visual.Component = Mesh;
				Visual.RestScale = Mesh->GetRelativeScale3D();
				M_StockVisuals.Add(Visual);
				break;
			}
		}
	}
}

void USovietRefineryComponent::UpdateStockVisuals() const
{
	for (const ERTSResourceType Resource : {ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal})
	{
		const int32 Capacity = Resource == ERTSResourceType::Resource_Radixite ? RadixiteStockCapacity : MetalStockCapacity;
		const float Fraction = FMath::Clamp(static_cast<float>(GetStock(Resource)) / FMath::Max(1, Capacity), 0.f, 1.f);
		// Cells of one resource fill one after the other in their order: count the distinct orders.
		TArray<int32> Orders;
		for (const FSovietRefineryStockVisual& Visual : M_StockVisuals)
		{
			if (Visual.Resource == Resource)
			{
				Orders.AddUnique(Visual.Order);
			}
		}
		Orders.Sort();
		const int32 CellCount = Orders.Num();
		for (const FSovietRefineryStockVisual& Visual : M_StockVisuals)
		{
			UStaticMeshComponent* Component = Visual.Component.Get();
			if (Visual.Resource != Resource || not IsValid(Component))
			{
				continue;
			}
			const int32 CellIndex = Orders.IndexOfByKey(Visual.Order);
			const float CellFill = FMath::Clamp(Fraction * CellCount - CellIndex, 0.f, 1.f);
			Component->SetVisibility(CellFill > 0.f);
			const float ScaleZ = FMath::Lerp(MinFillScale, 1.f, CellFill);
			Component->SetRelativeScale3D(FVector(Visual.RestScale.X, Visual.RestScale.Y, Visual.RestScale.Z * ScaleZ));
		}
	}
}

// ------------------------------------------------------------------------------------------------------------ stock

int32 USovietRefineryComponent::GetStock(const ERTSResourceType Resource) const
{
	return IsValid(M_DropOff) ? M_DropOff->GetDropOffAmountStored(Resource) : 0;
}

UResourceDropOff* USovietRefineryComponent::GetRefineryDropOff() const
{
	return M_DropOff;
}

UEnemyStrategicAIComponent* USovietRefineryComponent::GetStrategicAI() const
{
	const AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerOfUnit(GetOwner());
	return IsValid(EnemyController) ? EnemyController->GetEnemyStrategicAIComponent() : nullptr;
}

void USovietRefineryComponent::ConvertStock()
{
	if (not IsValid(M_DropOff))
	{
		return;
	}
	const int32 Radixite = FMath::Min(GetStock(ERTSResourceType::Resource_Radixite), RadixiteConvertedPerInterval);
	const int32 Metal = FMath::Min(GetStock(ERTSResourceType::Resource_Metal), MetalConvertedPerInterval);
	if (Radixite <= 0 && Metal <= 0)
	{
		return;
	}
	if (Radixite > 0)
	{
		M_DropOff->RemoveResources(ERTSResourceType::Resource_Radixite, Radixite);
	}
	if (Metal > 0)
	{
		M_DropOff->RemoveResources(ERTSResourceType::Resource_Metal, Metal);
	}
	M_TrainingPointRemainder += Radixite * TrainingPointsPerRadixite;
	M_BuildingPointRemainder += Metal * BuildingPointsPerMetal;
	const int32 TrainingPoints = FMath::FloorToInt(M_TrainingPointRemainder);
	const int32 BuildingPoints = FMath::FloorToInt(M_BuildingPointRemainder);
	M_TrainingPointRemainder -= TrainingPoints;
	M_BuildingPointRemainder -= BuildingPoints;

	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAI();
	if (IsValid(StrategicAI))
	{
		if (TrainingPoints > 0)
		{
			StrategicAI->AddBonusTrainingPoints(TrainingPoints);
		}
		if (BuildingPoints > 0)
		{
			StrategicAI->AddBuildingPoints(BuildingPoints);
		}
	}
	M_TotalConvertedRadixite += Radixite;
	M_TotalConvertedMetal += Metal;
	M_TotalTrainingPoints += TrainingPoints;
	M_TotalBuildingPoints += BuildingPoints;
	UpdateStockVisuals();
	UE_LOG(LogSovietRefinery, Log,
	       TEXT("SOVREF_CONVERT refinery=%s radixite=%d metal=%d +training=%d +building=%d stock_r=%d stock_m=%d ai=%d"),
	       *GetOwnerNameForLog(), Radixite, Metal, TrainingPoints, BuildingPoints,
	       GetStock(ERTSResourceType::Resource_Radixite), GetStock(ERTSResourceType::Resource_Metal),
	       IsValid(StrategicAI) ? 1 : 0);
}

#if RTS_WITH_MAP_TESTS
void USovietRefineryComponent::MapTest_AddStock(const ERTSResourceType Resource, const int32 Amount)
{
	if (IsValid(M_DropOff))
	{
		M_DropOff->AddResourcesNotHarvested(Resource, Amount);
		UpdateStockVisuals();
	}
}
#endif

// ------------------------------------------------------------------------------------------------------------ harvesters

int32 USovietRefineryComponent::GetMaxHarvesters() const
{
#if RTS_WITH_MAP_TESTS
	if (M_MapTest_MaxHarvestersOverride != INDEX_NONE)
	{
		return M_MapTest_MaxHarvestersOverride;
	}
#endif
	const AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerOfUnit(GetOwner());
	return IsValid(EnemyController) ? EnemyController->GetMaxHarvestersPerRefinery() : FallbackMaxHarvesters;
}

int32 USovietRefineryComponent::GetAliveHarvesterCount() const
{
	int32 Count = 0;
	for (const FSovietRefineryHarvester& Entry : M_Harvesters)
	{
		AActor* Harvester = Entry.Harvester.Get();
		if (IsValid(Harvester) && RTSFunctionLibrary::RTSIsValid(Harvester))
		{
			++Count;
		}
	}
	return Count;
}

TArray<AActor*> USovietRefineryComponent::GetHarvesters(TArray<ERTSResourceType>* OutResources) const
{
	TArray<AActor*> Harvesters;
	for (const FSovietRefineryHarvester& Entry : M_Harvesters)
	{
		AActor* Harvester = Entry.Harvester.Get();
		if (IsValid(Harvester) && RTSFunctionLibrary::RTSIsValid(Harvester))
		{
			Harvesters.Add(Harvester);
			if (OutResources)
			{
				OutResources->Add(Entry.Resource);
			}
		}
	}
	return Harvesters;
}

void USovietRefineryComponent::PruneHarvesters()
{
	M_Harvesters.RemoveAll([](const FSovietRefineryHarvester& Entry)
	{
		AActor* Harvester = Entry.Harvester.Get();
		return not IsValid(Harvester) || not RTSFunctionLibrary::RTSIsValid(Harvester);
	});
}

void USovietRefineryComponent::AdoptNearbyHarvesters()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const int32 OwningPlayer = FRTSPlayers::GetOwningPlayerOfActor(Owner);
	const FVector Origin = Owner->GetActorLocation();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (not IsValid(Candidate) || Candidate->ActorHasTag(SovietRefineryPrivate::AdoptedTag)
			|| FRTSPlayers::GetOwningPlayerOfActor(Candidate) != OwningPlayer
			|| FVector::Dist2D(Candidate->GetActorLocation(), Origin) > AdoptHarvesterRadius
			|| not IsValid(Candidate->FindComponentByClass<UHarvester>()))
		{
			continue;
		}
		const ERTSResourceType Resource = PickResourceForNewHarvester();
		Candidate->Tags.AddUnique(SovietRefineryPrivate::AdoptedTag);
		FSovietRefineryHarvester Entry;
		Entry.Harvester = Candidate;
		Entry.Resource = Resource;
		M_Harvesters.Add(Entry);
		SendHarvesterToWork(Candidate, Resource);
		UE_LOG(LogSovietRefinery, Log, TEXT("SOVREF_HARVESTER adopted=%s refinery=%s resource=%s"),
		       *Candidate->GetName(), *GetOwnerNameForLog(), *Global_GetResourceTypeAsString(Resource));
	}
}

void USovietRefineryComponent::HarvesterThink()
{
	PruneHarvesters();
	if (bM_IsTrainingHarvester || bM_IsSpawningHarvester)
	{
		return;
	}
	if (GetAliveHarvesterCount() >= GetMaxHarvesters())
	{
		return;
	}
	bM_IsTrainingHarvester = true;
	GetWorld()->GetTimerManager().SetTimer(M_HarvesterTrainTimer, this, &USovietRefineryComponent::OnHarvesterTrained,
	                                       HarvesterTrainSeconds, false);
	UE_LOG(LogSovietRefinery, Log, TEXT("SOVREF_HARVESTER training refinery=%s alive=%d max=%d seconds=%.1f"),
	       *GetOwnerNameForLog(), GetAliveHarvesterCount(), GetMaxHarvesters(), HarvesterTrainSeconds);
}

void USovietRefineryComponent::OnHarvesterTrained()
{
	bM_IsTrainingHarvester = false;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	const FTransform Exit = GetHarvesterExitTransform();
	FVector SpawnLocation;
	const FTrainingOption Option(EAllUnitType::UNType_Tank, static_cast<uint8>(HarvesterSubtype));
	if (not IsValid(AsyncSpawner) || not AsyncSpawner->GetHasClassForTrainingOption(Option)
		|| not TryGetNavigableLocation(Exit.GetLocation(), SpawnLocation))
	{
		RTSFunctionLibrary::ReportError("Soviet refinery " + GetOwnerNameForLog() + " cannot spawn its harvester " +
			Option.GetTrainingName() + ": no async spawner, no spawner row or no navigable exit.");
		return;
	}
	bM_IsSpawningHarvester = true;
	const TWeakObjectPtr<USovietRefineryComponent> WeakThis(this);
	// Always the refinery's own player: the Breaker Blueprint's default owner is not guaranteed to be the AI's
	// (GetOwnerOverrideForAIPlayer leaves the first AI player to the Blueprint default).
	const int32 OwnerOverride = FRTSPlayers::GetOwningPlayerOfActor(GetOwner());
	const bool bStarted = AsyncSpawner->AsyncSpawnOptionAtLocation(
		Option, SpawnLocation, TWeakObjectPtr<UObject>(this), ++M_SpawnRequestID,
		[WeakThis](const FTrainingOption& /*SpawnedOption*/, AActor* SpawnedActor, const int32 /*ID*/)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnHarvesterSpawned(SpawnedActor);
			}
		}, FRotator(0.f, Exit.Rotator().Yaw, 0.f), OwnerOverride);
	if (not bStarted)
	{
		bM_IsSpawningHarvester = false;
		RTSFunctionLibrary::ReportError("Soviet refinery " + GetOwnerNameForLog() + " could not start its harvester spawn.");
	}
}

void USovietRefineryComponent::OnHarvesterSpawned(AActor* SpawnedActor)
{
	bM_IsSpawningHarvester = false;
	if (not IsValid(SpawnedActor))
	{
		return;
	}
	if (UFowComp* FowComponent = SpawnedActor->FindComponentByClass<UFowComp>())
	{
		FowComponent->StartFowNowHiddenUntilSeen();
	}
	const ERTSResourceType Resource = PickResourceForNewHarvester();
	SpawnedActor->Tags.AddUnique(SovietRefineryPrivate::AdoptedTag);
	FSovietRefineryHarvester Entry;
	Entry.Harvester = SpawnedActor;
	Entry.Resource = Resource;
	M_Harvesters.Add(Entry);
	SendHarvesterToWork(SpawnedActor, Resource);
	UE_LOG(LogSovietRefinery, Log, TEXT("SOVREF_HARVESTER spawned=%s refinery=%s resource=%s alive=%d"),
	       *SpawnedActor->GetName(), *GetOwnerNameForLog(), *Global_GetResourceTypeAsString(Resource),
	       GetAliveHarvesterCount());
}

ERTSResourceType USovietRefineryComponent::PickResourceForNewHarvester() const
{
	int32 RadixiteCount = 0;
	int32 MetalCount = 0;
	for (const FSovietRefineryHarvester& Entry : M_Harvesters)
	{
		if (not Entry.Harvester.IsValid())
		{
			continue;
		}
		Entry.Resource == ERTSResourceType::Resource_Metal ? ++MetalCount : ++RadixiteCount;
	}
	// Radixite first (army), then alternate.
	return MetalCount < RadixiteCount ? ERTSResourceType::Resource_Metal : ERTSResourceType::Resource_Radixite;
}

void USovietRefineryComponent::SendHarvesterToWork(AActor* Harvester, const ERTSResourceType Resource) const
{
	UHarvester* HarvesterComponent = IsValid(Harvester) ? Harvester->FindComponentByClass<UHarvester>() : nullptr;
	if (not IsValid(HarvesterComponent))
	{
		RTSFunctionLibrary::ReportError("Soviet refinery " + GetOwnerNameForLog() + ": " + GetNameSafe(Harvester) +
			" has no harvester component.");
		return;
	}
	HarvesterComponent->StartAutonomousHarvest(Resource);
}

FTransform USovietRefineryComponent::GetHarvesterExitTransform() const
{
	TArray<USceneComponent*> SceneComponents;
	GetOwner()->GetComponents<USceneComponent>(SceneComponents);
	for (const USceneComponent* Component : SceneComponents)
	{
		if (Component->ComponentHasTag(SovietRefineryPrivate::ExitTag))
		{
			return Component->GetComponentTransform();
		}
	}
	const FTransform Frame = M_BuildingMesh.IsValid() ? M_BuildingMesh->GetComponentTransform()
		                         : GetOwner()->GetActorTransform();
	return FallbackHarvesterExit * FTransform(Frame.GetRotation(), Frame.GetLocation());
}

bool USovietRefineryComponent::TryGetNavigableLocation(const FVector& Desired, FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavSystem))
	{
		return false;
	}
	FNavLocation Projected;
	using namespace SovietRefineryPrivate;
	if (NavSystem->ProjectPointToNavigation(Desired, Projected, FVector(ExitProjectionExtent))
		|| NavSystem->ProjectPointToNavigation(Desired, Projected, FVector(WideExitProjectionExtent)))
	{
		OutLocation = Projected.Location;
		return true;
	}
	return false;
}

FString USovietRefineryComponent::GetOwnerNameForLog() const
{
	return GetNameSafe(GetOwner());
}
