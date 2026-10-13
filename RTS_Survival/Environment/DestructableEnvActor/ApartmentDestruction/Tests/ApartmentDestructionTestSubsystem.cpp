// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentDestructionTestSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Physics/Experimental/ChaosEventType.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "RTS_Survival/Environment/DestructableEnvActor/ApartmentDestruction/ApartmentDestructibleActor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/ApartmentDestruction/ApartmentFootprintNavModifier.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/GeometryDamageComponent/GeometryDamageComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RHIStats.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSApartmentGcTest, Log, All);

namespace ApartmentDestructionTest
{
	const TCHAR* const CommandLineSwitch = TEXT("ApartmentGcPrototypeTest");
	const TCHAR* const DirectSwitch = TEXT("ApartmentGcDirect");
	const TCHAR* const GameplayActorSwitch = TEXT("ApartmentGcGameplayActor");
	const TCHAR* const CrushSwitch = TEXT("ApartmentGcCrush");
	const TCHAR* const ActorClassSwitch = TEXT("ApartmentGcActorClass=");
	const TCHAR* const CoverReachSwitch = TEXT("ApartmentCoverReach");
	const TCHAR* const GarrisonExitSwitch = TEXT("ApartmentGarrisonExit");
	constexpr float GarrisonEnterTimeoutSeconds = 120.f;
	// Card slot the garrison actor puts Exit Cargo in.
	constexpr int32 GarrisonExitCargoButtonIndex = 0;
	constexpr float GarrisonLeaveTimeoutSeconds = 30.f;
	constexpr float CoverReachWaitSeconds = 25.f;
	// Where the infantry path to a socket starts: out on the street in front of it.
	constexpr double CoverReachStartDistances[] = {1500.0, 2500.0, 900.0};
	const FVector CoverReachStartExtent(400.0, 400.0, 400.0);
	const FVector CoverApproachProjectExtent(120.0, 120.0, 250.0);
	const TCHAR* const InfantryNavFilterPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Nav/Filters/RTS_NavFilter_Characters.RTS_NavFilter_Characters_C");
	const FName InfantryNavAgentName(TEXT("Character"));
	// Where a soldier's muzzle is when he fires from a window, and a height that must stay behind the sill.
	constexpr double CoverFiringHeight = 155.0;
	constexpr double CoverSillProbeHeight = 60.0;
	constexpr double CoverLaneLength = 260.0;
	constexpr float CoverCountRadius = 3000.f;
	// Roughly a heavy tank.
	const FVector CrushBoxHalfExtent(350.0, 180.0, 150.0);
	constexpr double CrushStartOffsetEast = 1500.0;
	constexpr double CrushStepLength = 150.0;
	constexpr int32 CrushStepCount = 7;
	constexpr double CrushLaneOffsetNorth = 150.0;
	constexpr double NavProfileStep = 100.0;
	constexpr int32 NavMapHalfSampleCount = 20;
	const FVector NavProjectExtent(20.0, 20.0, 400.0);
	const TCHAR* const PanelHealthSwitch = TEXT("ApartmentGcPanelHealth=");
	// In direct mode the damage threshold is the health of one panel, in the same units as weapon damage.
	constexpr float DefaultPanelHealth = 250.f;
	constexpr float DirectStrainRadius = 230.f;
	constexpr int32 DirectStrainPropagationDepth = 1;
	constexpr float DirectStrainPropagationFactor = 0.35f;
	constexpr float BreakingPieceSpeed = 450.f;
	constexpr float TraceOvershoot = 400.f;
	const TCHAR* const CollectionSwitch = TEXT("ApartmentGcCollection=");
	const TCHAR* const NameSwitch = TEXT("ApartmentGcRunName=");
	const TCHAR* const PivotAtSiteSwitch = TEXT("ApartmentGcPivotAtSite");
	const TCHAR* const KeepPropagationSwitch = TEXT("ApartmentGcKeepDamagePropagation");
	const TCHAR* const RuntimeConnectionsSwitch = TEXT("ApartmentGcRuntimeConnections");
	const TCHAR* const ThresholdSwitch = TEXT("ApartmentGcThreshold=");
	constexpr float DefaultPanelDamageThreshold = 60000.f;
	// A panel is 300 cm wide, so its centre is at most ~212 cm from a hit on it.
	constexpr float PanelMinForceRadius = 180.f;
	constexpr float PanelMaxForceRadius = 520.f;
	constexpr float PanelMaxStrainRadius = 320.f;
	const TCHAR* const CollectionPath = TEXT("/Game/DBK_Apartments/Destructible/GC_Apt_C2.GC_Apt_C2");

	// The collection's pivot is the last piece that was appended; this places the building upright on the site.
	const FVector ActorLocation(-14238.0, 4230.9, 722.0);
	const FRotator ActorRotation(0.0, 329.04, 0.0);
	const FVector SiteCentre(-15000.0, 3600.0, 100.0);
	const FVector CameraOffsetFromSite(2700.0, -2100.0, 2300.0);
	const FVector CameraLookAtOffset(0.0, 0.0, 450.0);
	const FVector PlayerPawnOffsetFromSite(0.0, -2500.0, 1500.0);
	// The ground storey is pinned so an anchored dynamic collection cannot sink or topple as a whole.
	const FVector AnchorBoxHalfExtent(1100.0, 1250.0, 160.0);
	const FVector AnchorBoxCentreOffset(0.0, 0.0, 150.0);

	constexpr float IntactObserveSeconds = 4.f;
	constexpr float SecondsBetweenImpacts = 3.f;
	constexpr float SecondsAfterLastImpact = 6.f;
	constexpr double SecondsToMilliseconds = 1000.0;
}

UApartmentTestGeometryDamageComponent::UApartmentTestGeometryDamageComponent()
{
	using namespace ApartmentDestructionTest;
	DefaultForceProfile.MinRadius = PanelMinForceRadius;
	DefaultForceProfile.MaxRadius = PanelMaxForceRadius;
	DefaultForceProfile.MaxStrainRadius = PanelMaxStrainRadius;
	DefaultForceProfile.FalloffType = EFieldFalloffType::Field_Falloff_Linear;
}

bool UApartmentDestructionTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (not FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::CommandLineSwitch))
	{
		return false;
	}
	return Super::ShouldCreateSubsystem(Outer);
}

bool UApartmentDestructionTestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UApartmentDestructionTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UApartmentDestructionTestSubsystem, STATGROUP_Tickables);
}

void UApartmentDestructionTestSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (not IsValid(World) || bM_IsFinished)
	{
		return;
	}
	if (not bM_HasLoggedFirstTick)
	{
		bM_HasLoggedFirstTick = true;
		UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC FIRST_TICK world=%s"), *World->GetName());
	}
	if (FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::CoverReachSwitch))
	{
		Tick_CoverReach(*World);
		return;
	}
	if (FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::GarrisonExitSwitch))
	{
		Tick_GarrisonExit(*World);
		return;
	}
	if (not Tick_UnpauseAndSpawn(*World))
	{
		return;
	}
	UpdateCamera(*World);
	AccumulateFrame();
	if (World->GetTimeSeconds() >= M_NextStepSeconds)
	{
		AdvanceScript(*World);
	}
}

bool UApartmentDestructionTestSubsystem::Tick_UnpauseAndSpawn(UWorld& World)
{
	// The map boots paused behind the start-game widget; physics does not step until it is unpaused.
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(&World);
	if (not IsValid(PlayerController))
	{
		return false;
	}
	if (PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return false;
	}
	if (bM_HasSpawned)
	{
		return true;
	}
	bM_HasSpawned = true;
	bM_UseDirectStrain = FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::DirectSwitch);
	bM_UseDynamicAnchored = bM_UseDirectStrain;
	M_RunName = bM_UseDirectStrain ? TEXT("direct") : TEXT("kinematic");
	FParse::Value(FCommandLine::Get(), ApartmentDestructionTest::NameSwitch, M_RunName);
	bM_UseGameplayActor = FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::GameplayActorSwitch);
	if (bM_UseGameplayActor)
	{
		LogNavAreaProfile(World, TEXT("baseline"));
		SpawnGameplayActor(World);
	}
	else
	{
		SpawnPrototype(World);
	}
	bM_UseCrushScript = bM_UseGameplayActor && FParse::Param(FCommandLine::Get(), ApartmentDestructionTest::CrushSwitch);
	if (bM_UseCrushScript)
	{
		BuildCrushScript();
	}
	else
	{
		BuildImpactScript();
	}
	M_NextStepSeconds = World.GetTimeSeconds() + ApartmentDestructionTest::IntactObserveSeconds;
	return bM_UseGameplayActor ? IsValid(M_GameplayActor) : GetIsValidPrototypeActor();
}

void UApartmentDestructionTestSubsystem::SpawnPrototype(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	FString CollectionToLoad = CollectionPath;
	FParse::Value(FCommandLine::Get(), CollectionSwitch, CollectionToLoad);
	UGeometryCollection* Collection = LoadObject<UGeometryCollection>(nullptr, *CollectionToLoad);
	if (not IsValid(Collection))
	{
		UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_GC could not load %s"), *CollectionToLoad);
		return;
	}
	// The fog of war is a post process material that blacks out unexplored ground; the pictures need the building.
	if (GEngine != nullptr)
	{
		GEngine->Exec(&World, TEXT("ShowFlag.PostProcessMaterial 0"));
		// The connection graph baked into the asset does not link separate kit pieces; with this the solver builds
		// it at spawn from the asset's connection type instead, so undamaged neighbours keep holding together.
		if (FParse::Param(FCommandLine::Get(), RuntimeConnectionsSwitch))
		{
			GEngine->Exec(&World, TEXT("p.GeometryCollection.AlwaysGenerateConnectionGraph 1"));
		}
	}
	// Collections built by RTS.Apartment.BuildGeometryCollection have their pivot at the footprint centre on the ground.
	const bool bPivotAtSite = FParse::Param(FCommandLine::Get(), PivotAtSiteSwitch);
	M_PrototypeActor = bPivotAtSite
		                   ? World.SpawnActor<AGeometryCollectionActor>(SiteCentre, FRotator::ZeroRotator)
		                   : World.SpawnActor<AGeometryCollectionActor>(ActorLocation, ActorRotation);
	if (not GetIsValidPrototypeActor())
	{
		return;
	}
	UGeometryCollectionComponent* GeometryComponent = M_PrototypeActor->GetGeometryCollectionComponent();
	GeometryComponent->ObjectType = bM_UseDynamicAnchored
		                                ? EObjectStateTypeEnum::Chaos_Object_Dynamic
		                                : EObjectStateTypeEnum::Chaos_Object_Kinematic;
	GeometryComponent->SetRestCollection(Collection);
	// Panels sit one level below the root; this is the strain a hit must exceed to free one.
	float PanelDamageThreshold = DefaultPanelDamageThreshold;
	FParse::Value(FCommandLine::Get(), ThresholdSwitch, PanelDamageThreshold);
	if (bM_UseDirectStrain)
	{
		PanelDamageThreshold = DefaultPanelHealth;
		FParse::Value(FCommandLine::Get(), PanelHealthSwitch, PanelDamageThreshold);
		// Only weapons may break panels; otherwise falling debris chain-breaks everything it lands on.
		GeometryComponent->bEnableDamageFromCollision = false;
		// By default a breaking piece passes its full damage on to every connected neighbour; with one shared panel
		// health that chain-reacts through the whole building, so only the pieces a weapon damaged may break.
		FGeometryCollectionDamagePropagationData DamagePropagation = GeometryComponent->DamagePropagationData;
		DamagePropagation.bEnabled = FParse::Param(FCommandLine::Get(), KeepPropagationSwitch);
		GeometryComponent->SetDamagePropagationData(DamagePropagation);
	}
	GeometryComponent->DamageThreshold = {PanelDamageThreshold};
	GeometryComponent->SetNotifyBreaks(true);
	GeometryComponent->RecreatePhysicsState();
	GeometryComponent->OnChaosBreakEvent.AddDynamic(this, &UApartmentDestructionTestSubsystem::OnChaosBreak);
	if (bM_UseDynamicAnchored)
	{
		const FVector AnchorCentre = SiteCentre + AnchorBoxCentreOffset;
		GeometryComponent->SetAnchoredByBox(FBox(AnchorCentre - AnchorBoxHalfExtent, AnchorCentre + AnchorBoxHalfExtent), true);
		GeometryComponent->SetSimulatePhysics(true);
	}

	bool bInitialised = false;
	if (not bM_UseDirectStrain)
	{
		M_GeometryDamage = NewObject<UApartmentTestGeometryDamageComponent>(M_PrototypeActor, TEXT("GeometryDamage"));
		M_GeometryDamage->RegisterComponent();
		bInitialised = M_GeometryDamage->InitGeometryDamage(GeometryComponent, nullptr);
	}
	M_Camera = World.SpawnActor<ACameraActor>();

	const FBoxSphereBounds Bounds = GeometryComponent->Bounds;
	UE_LOG(LogRTSApartmentGcTest, Display,
	       TEXT("RTS_APARTMENT_GC SPAWNED mode=%s damage_component_init=%s transforms=%d bounds_origin=%s bounds_extent=%s"),
	       bM_UseDirectStrain ? TEXT("dynamic_anchored_direct_strain") : TEXT("kinematic_damage_component"),
	       bInitialised ? TEXT("true") : TEXT("false"),
	       Collection->NumElements(FGeometryCollection::TransformGroup),
	       *Bounds.Origin.ToCompactString(), *Bounds.BoxExtent.ToCompactString());
}

void UApartmentDestructionTestSubsystem::SpawnGameplayActor(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	FString CollectionToLoad = CollectionPath;
	FParse::Value(FCommandLine::Get(), CollectionSwitch, CollectionToLoad);
	UGeometryCollection* Collection = LoadObject<UGeometryCollection>(nullptr, *CollectionToLoad);
	if (not IsValid(Collection))
	{
		UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_GC could not load %s"), *CollectionToLoad);
		return;
	}
	if (GEngine != nullptr)
	{
		GEngine->Exec(&World, TEXT("ShowFlag.PostProcessMaterial 0"));
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator, SiteCentre);
	// A Blueprint of one building brings its own collection, cover sockets and part health.
	FString ActorClassPath;
	UClass* ActorClass = AApartmentDestructibleActor::StaticClass();
	if (FParse::Value(FCommandLine::Get(), ActorClassSwitch, ActorClassPath))
	{
		ActorClass = LoadClass<AApartmentDestructibleActor>(nullptr, *ActorClassPath);
	}
	if (ActorClass == nullptr)
	{
		UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_GC could not load actor class %s"), *ActorClassPath);
		return;
	}
	M_GameplayActor = World.SpawnActorDeferred<AApartmentDestructibleActor>(ActorClass, SpawnTransform);
	if (not IsValid(M_GameplayActor))
	{
		return;
	}
	// A placed actor has its collection assigned in the editor; here it has to be in before BeginPlay runs.
	UGeometryCollectionComponent* GeometryComponent = GetTestedGeometryComponent();
	if (ActorClassPath.IsEmpty())
	{
		GeometryComponent->SetRestCollection(Collection);
	}
	GeometryComponent->SetNotifyBreaks(true);
	GeometryComponent->OnChaosBreakEvent.AddDynamic(this, &UApartmentDestructionTestSubsystem::OnChaosBreak);
	M_GameplayActor->FinishSpawning(SpawnTransform);
	M_Camera = World.SpawnActor<ACameraActor>();
	UE_LOG(LogRTSApartmentGcTest, Display,
	       TEXT("RTS_APARTMENT_GC SPAWNED mode=gameplay_actor transforms=%d asset_memory_bytes=%llu custom_renderer=%s"),
	       Collection->NumElements(FGeometryCollection::TransformGroup),
	       static_cast<uint64>(Collection->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal)),
	       *GetNameSafe(Collection->CustomRendererType));
}

UGeometryCollectionComponent* UApartmentDestructionTestSubsystem::GetTestedGeometryComponent() const
{
	if (IsValid(M_GameplayActor))
	{
		return M_GameplayActor->FindComponentByClass<UGeometryCollectionComponent>();
	}
	return IsValid(M_PrototypeActor) ? M_PrototypeActor->GetGeometryCollectionComponent() : nullptr;
}

void UApartmentDestructionTestSubsystem::FireImpactAsProjectile(const FApartmentDestructionTestImpact& Impact,
                                                                const FVector& WorldLocation) const
{
	using namespace ApartmentDestructionTest;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const FVector TraceStart = WorldLocation + Impact.TraceStartOffset;
	const FVector ShotDirection = (WorldLocation - TraceStart).GetSafeNormal();
	FHitResult Hit;
	// The channel a player-owned projectile traces on.
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, TraceStart, WorldLocation + ShotDirection * TraceOvershoot, COLLISION_TRACE_ENEMY);
	AActor* HitActor = Hit.GetActor();
	if (not bHit || not IsValid(HitActor))
	{
		UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC PROJECTILE_TRACE_MISS label=%s"), *Impact.Label);
		return;
	}
	// Like the projectile: only the location is filled in, and there is no moving damage causer.
	FPointDamageEvent DamageEvent;
	DamageEvent.HitInfo.Location = Hit.Location;
	for (int32 RepeatIndex = 0; RepeatIndex < Impact.RepeatCount; ++RepeatIndex)
	{
		HitActor->TakeDamage(Impact.DamageAmount, DamageEvent, nullptr, nullptr);
	}
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC PROJECTILE_TRACE_HIT label=%s actor=%s at=%s"),
	       *Impact.Label, *HitActor->GetName(), *Hit.Location.ToCompactString());
}

void UApartmentDestructionTestSubsystem::BuildCrushScript()
{
	using namespace ApartmentDestructionTest;
	for (int32 StepIndex = 0; StepIndex < CrushStepCount; ++StepIndex)
	{
		FApartmentDestructionTestImpact Step;
		Step.Label = FString::Printf(TEXT("crush_step_%d"), StepIndex);
		Step.OffsetFromSite = FVector(CrushStartOffsetEast - StepIndex * CrushStepLength, CrushLaneOffsetNorth,
		                              CrushBoxHalfExtent.Z);
		M_Impacts.Add(Step);
	}
}

void UApartmentDestructionTestSubsystem::FireCrushStep(const FApartmentDestructionTestImpact& Impact,
                                                       const FVector& WorldLocation) const
{
	using namespace ApartmentDestructionTest;
	if (not IsValid(M_GameplayActor))
	{
		return;
	}
	const int32 PiecesInside = M_GameplayActor->CrushInsideBox(
		FBox::BuildAABB(WorldLocation, CrushBoxHalfExtent), FVector::BackwardVector);
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC CRUSH label=%s pieces_inside=%d at=%s"),
	       *Impact.Label, PiecesInside, *WorldLocation.ToCompactString());
}

void UApartmentDestructionTestSubsystem::LogNavAreaProfile(UWorld& World, const FString& MapLabel) const
{
	using namespace ApartmentDestructionTest;
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	if (not IsValid(NavigationSystem))
	{
		return;
	}
	TInlineComponentArray<UApartmentFootprintNavModifier*> FootprintModifiers;
	if (IsValid(M_GameplayActor))
	{
		M_GameplayActor->GetComponents(FootprintModifiers);
	}
	for (const UApartmentFootprintNavModifier* FootprintModifier : FootprintModifiers)
	{
		const FBox ModifierBounds = FootprintModifier->GetNavigationBounds();
		UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC NAV_MODIFIER name=%s area=%s min=%s max=%s"),
		       *FootprintModifier->GetName(), *GetNameSafe(FootprintModifier->AreaClass),
		       *ModifierBounds.Min.ToCompactString(), *ModifierBounds.Max.ToCompactString());
	}
	for (const ANavigationData* NavData : NavigationSystem->NavDataSet)
	{
		const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavData);
		if (not IsValid(NavMesh))
		{
			continue;
		}
		// One character per sample, north at the top: '#' no navigation mesh, 'r' the heavy-tank-only ring, '.' open
		// ground, '?' any other area.
		for (int32 RowIndex = NavMapHalfSampleCount; RowIndex >= -NavMapHalfSampleCount; --RowIndex)
		{
			FString Row;
			for (int32 ColumnIndex = -NavMapHalfSampleCount; ColumnIndex <= NavMapHalfSampleCount; ++ColumnIndex)
			{
				const FVector SamplePoint = SiteCentre + FVector(ColumnIndex * NavProfileStep, RowIndex * NavProfileStep, 0.0);
				FNavLocation Projected;
				const bool bOnNavMesh = NavigationSystem->ProjectPointToNavigation(
					SamplePoint, Projected, NavProjectExtent, NavMesh);
				const UClass* AreaClass = bOnNavMesh ? NavMesh->GetAreaClass(NavMesh->GetPolyAreaID(Projected.NodeRef)) : nullptr;
				const FString AreaName = GetNameSafe(AreaClass);
				Row += AreaClass == nullptr ? TEXT("#") : AreaName.Contains(TEXT("BlockCharLightMedium")) ? TEXT("r") : AreaName.Contains(TEXT("Default")) ? TEXT(".") : TEXT("?");
			}
			UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC NAVMAP %s %s %s"), *MapLabel,
			       *NavMesh->GetConfig().Name.ToString(), *Row);
		}
	}
}

void UApartmentDestructionTestSubsystem::BuildImpactScript()
{
	// Damage is normalised against the component's 1000 debug health: 20 is inside the deadzone, 250+ is a full hit.
	const auto AddImpact = [this](const TCHAR* Label, const FVector& Offset, const float Damage, const int32 Repeat,
	                              const FVector& TraceStartOffset)
	{
		FApartmentDestructionTestImpact Impact;
		Impact.Label = Label;
		Impact.OffsetFromSite = Offset;
		Impact.DamageAmount = Damage;
		Impact.RepeatCount = Repeat;
		Impact.TraceStartOffset = TraceStartOffset;
		M_Impacts.Add(Impact);
	};
	const FVector FromEast(700.0, 0.0, 0.0);
	const FVector FromAbove(0.0, 0.0, 700.0);
	AddImpact(TEXT("smallarms_east_wall_s2"), FVector(900.0, 150.0, 750.0), 15.f, 1, FromEast);
	AddImpact(TEXT("autocannon_east_wall_s2"), FVector(900.0, 150.0, 750.0), 60.f, 1, FromEast);
	AddImpact(TEXT("autocannon_burst_east_wall_s2"), FVector(900.0, 150.0, 750.0), 60.f, 4, FromEast);
	AddImpact(TEXT("shell_east_wall_s2"), FVector(900.0, 500.0, 750.0), 150.f, 1, FromEast);
	AddImpact(TEXT("heavy_shell_roof_corner"), FVector(800.0, 950.0, 950.0), 300.f, 1, FromAbove);
	AddImpact(TEXT("heavy_shell_roof_centre"), FVector(-100.0, 0.0, 1040.0), 300.f, 1, FromAbove);
	AddImpact(TEXT("heavy_shell_east_wall_s1"), FVector(900.0, -450.0, 450.0), 300.f, 1, FromEast);
	AddImpact(TEXT("volley_east_wall_s1"), FVector(900.0, -450.0, 450.0), 300.f, 4, FromEast);
	AddImpact(TEXT("heavy_shells_east_wall_s1_row"), FVector(900.0, 150.0, 450.0), 300.f, 3, FromEast);
	AddImpact(TEXT("heavy_shell_ground_storey"), FVector(900.0, 300.0, 150.0), 300.f, 2, FromEast);
}

void UApartmentDestructionTestSubsystem::UpdateCamera(UWorld& World) const
{
	using namespace ApartmentDestructionTest;
	APlayerController* PlayerController = World.GetFirstPlayerController();
	if (not IsValid(PlayerController) || not IsValid(M_Camera))
	{
		return;
	}
	const FVector CameraLocation = SiteCentre + CameraOffsetFromSite;
	M_Camera->SetActorLocationAndRotation(
		CameraLocation, UKismetMathLibrary::FindLookAtRotation(CameraLocation, SiteCentre + CameraLookAtOffset));
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	PlayerController->SetViewTarget(M_Camera);
	// The damage component skips physics for hits far from the player's camera pawn, so the pawn is parked here too.
	if (APawn* PlayerPawn = PlayerController->GetPawn())
	{
		PlayerPawn->SetActorLocation(SiteCentre + PlayerPawnOffsetFromSite);
	}
}

void UApartmentDestructionTestSubsystem::AccumulateFrame()
{
	const double FrameSeconds = FApp::GetDeltaTime();
	++M_Window.FrameCount;
	M_Window.TotalFrameSeconds += FrameSeconds;
	M_Window.WorstFrameSeconds = FMath::Max(M_Window.WorstFrameSeconds, FrameSeconds);
	// Counted by the render thread for the frame it last finished; close enough for an average.
	M_Window.TotalDrawCalls += GNumDrawCallsRHI[0];
}

void UApartmentDestructionTestSubsystem::AdvanceScript(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	const FString FinishedLabel = M_Impacts.IsValidIndex(M_NextImpactIndex) ? M_Impacts[M_NextImpactIndex].Label : TEXT("intact");
	ReportWindow(World, FinishedLabel);
	RequestScreenshot(FinishedLabel);
	if (bM_UseGameplayActor)
	{
		ReportCover(World, FinishedLabel);
		LogNavAreaProfile(World, FinishedLabel);
	}

	++M_NextImpactIndex;
	if (not M_Impacts.IsValidIndex(M_NextImpactIndex))
	{
		Finish(World);
		return;
	}
	FireImpact(M_Impacts[M_NextImpactIndex]);
	const bool bIsLastImpact = M_NextImpactIndex == M_Impacts.Num() - 1;
	M_NextStepSeconds = World.GetTimeSeconds() + (bIsLastImpact ? SecondsAfterLastImpact : SecondsBetweenImpacts);
}

void UApartmentDestructionTestSubsystem::FireImpact(const FApartmentDestructionTestImpact& Impact) const
{
	const FVector WorldLocation = ApartmentDestructionTest::SiteCentre + Impact.OffsetFromSite;
	if (bM_UseCrushScript)
	{
		FireCrushStep(Impact, WorldLocation);
	}
	else if (bM_UseGameplayActor)
	{
		FireImpactAsProjectile(Impact, WorldLocation);
	}
	else if (bM_UseDirectStrain)
	{
		FireImpactDirect(Impact, WorldLocation);
	}
	else if (GetIsValidGeometryDamage())
	{
		for (int32 RepeatIndex = 0; RepeatIndex < Impact.RepeatCount; ++RepeatIndex)
		{
			M_GeometryDamage->Debug_FireTestImpact(WorldLocation, Impact.DamageAmount);
		}
	}
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC FIRE label=%s damage=%.0f repeat=%d at=%s"),
	       *Impact.Label, Impact.DamageAmount, Impact.RepeatCount, *WorldLocation.ToCompactString());
}

void UApartmentDestructionTestSubsystem::FireImpactDirect(const FApartmentDestructionTestImpact& Impact,
                                                         const FVector& WorldLocation) const
{
	using namespace ApartmentDestructionTest;
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not GetIsValidPrototypeActor())
	{
		return;
	}
	UGeometryCollectionComponent* GeometryComponent = M_PrototypeActor->GetGeometryCollectionComponent();
	const FVector TraceStart = WorldLocation + Impact.TraceStartOffset;
	const FVector ShotDirection = (WorldLocation - TraceStart).GetSafeNormal();
	FHitResult Hit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ApartmentGcTest), true);
	const bool bHit = GeometryComponent->LineTraceComponent(
		Hit, TraceStart, WorldLocation + ShotDirection * TraceOvershoot, QueryParams);
	if (not bHit)
	{
		UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC TRACE_MISS label=%s"), *Impact.Label);
		return;
	}
	for (int32 RepeatIndex = 0; RepeatIndex < Impact.RepeatCount; ++RepeatIndex)
	{
		GeometryComponent->ApplyInternalStrain(Hit.Item, Hit.ImpactPoint, DirectStrainRadius,
		                                       DirectStrainPropagationDepth, DirectStrainPropagationFactor,
		                                       Impact.DamageAmount);
	}
	GeometryComponent->ApplyBreakingLinearVelocity(Hit.Item, ShotDirection * BreakingPieceSpeed);
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC TRACE_HIT label=%s item=%d at=%s"), *Impact.Label,
	       Hit.Item, *Hit.ImpactPoint.ToCompactString());
}

void UApartmentDestructionTestSubsystem::FinishGarrisonExit(UWorld& World, const bool bPassed, const FString& Details)
{
	bM_IsFinished = true;
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GARRISON RESULT %s %s"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), *Details);
	GEngine->Exec(&World, TEXT("quit"));
}

void UApartmentDestructionTestSubsystem::Tick_GarrisonExit(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	const float Now = World.GetTimeSeconds();
	if (M_GarrisonDeadlineSeconds <= 0.f)
	{
		M_GarrisonDeadlineSeconds = Now + GarrisonEnterTimeoutSeconds;
	}
	if (M_GarrisonApartment.IsValid())
	{
		// Ordered out: everyone has to leave and the building is the player's no longer.
		const int32 StillInside = M_GarrisonApartment->GetGarrisonedSoldierCount();
		const bool bSelectableGone = not IsValid(M_GarrisonApartment->GetSelectionProxyActor());
		if (StillInside == 0 && bSelectableGone)
		{
			FinishGarrisonExit(World, true, FString::Printf(TEXT("soldiers_out=%d"), M_GarrisonSoldiersAtOrder));
			return;
		}
		if (Now > M_GarrisonDeadlineSeconds)
		{
			FinishGarrisonExit(World, false, FString::Printf(
				TEXT("still_inside=%d selectable_gone=%d"), StillInside, bSelectableGone ? 1 : 0));
		}
		return;
	}
	for (TActorIterator<AApartmentDestructibleActor> ApartmentIterator(&World); ApartmentIterator; ++ApartmentIterator)
	{
		AApartmentDestructibleActor* Apartment = *ApartmentIterator;
		ASelectableActorObjectsMaster* Selectable = Cast<ASelectableActorObjectsMaster>(
			Apartment->GetSelectionProxyActor());
		if (Apartment->GetGarrisonedSoldierCount() == 0 || not IsValid(Selectable))
		{
			continue;
		}
		const URTSComponent* SelectableRTSComponent = Selectable->GetRTSComponent();
		const USelectionComponent* SelectionComponent = Selectable->GetSelectionComponent();
		const bool bHasExitCargo = Selectable->HasAbility(EAbilityID::IdExitCargo);
		UE_LOG(LogRTSApartmentGcTest, Display,
		       TEXT("RTS_APARTMENT_GARRISON ENTERED apartment=%s soldiers=%d owner=%d selectable=%d exit_cargo_on_card=%d"),
		       *Apartment->GetActorNameOrLabel(), Apartment->GetGarrisonedSoldierCount(),
		       IsValid(SelectableRTSComponent) ? SelectableRTSComponent->GetOwningPlayer() : -1,
		       IsValid(SelectionComponent) && SelectionComponent->GetCanBeSelected() ? 1 : 0, bHasExitCargo ? 1 : 0);
		M_GarrisonSoldiersAtOrder = Apartment->GetGarrisonedSoldierCount();
		// The same path as a click on the building followed by the first button of the command card.
		ACPPController* PlayerController = Cast<ACPPController>(World.GetFirstPlayerController());
		if (not IsValid(PlayerController) || not bHasExitCargo)
		{
			FinishGarrisonExit(World, false, TEXT("no player controller or no exit cargo on the card"));
			return;
		}
		AActor* ClickedActor = Apartment;
		PlayerController->ChangeClickedActorIfHasSelectionProxy(ClickedActor);
		const bool bClickSelectsGarrison = ClickedActor == Selectable && PlayerController->GetIsClickedUnitSelectable(ClickedActor);
		PlayerController->UpdateUIForSelectionAction(PlayerController->ClickUnit_GetSelectionAction(ClickedActor), ClickedActor);
		const bool bIsSelected = PlayerController->TSelectedActorsMasters.Contains(Selectable) && SelectionComponent->GetIsSelected();
		PlayerController->ActivateActionButton(GarrisonExitCargoButtonIndex);
		UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GARRISON CLICK redirects=%d selected=%d"),
		       bClickSelectsGarrison ? 1 : 0, bIsSelected ? 1 : 0);
		if (not bClickSelectsGarrison || not bIsSelected)
		{
			FinishGarrisonExit(World, false, TEXT("a click on the apartment did not select its garrison"));
			return;
		}
		M_GarrisonApartment = Apartment;
		M_GarrisonDeadlineSeconds = Now + GarrisonLeaveTimeoutSeconds;
		return;
	}
	if (Now > M_GarrisonDeadlineSeconds)
	{
		FinishGarrisonExit(World, false, TEXT("no player soldier took an apartment socket"));
	}
}

void UApartmentDestructionTestSubsystem::Tick_CoverReach(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	M_CoverReachWaitSeconds += FApp::GetDeltaTime();
	if (M_CoverReachWaitSeconds < CoverReachWaitSeconds)
	{
		return;
	}
	bM_IsFinished = true;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	const ANavigationData* InfantryNavData = nullptr;
	if (IsValid(NavigationSystem))
	{
		for (const ANavigationData* NavData : NavigationSystem->NavDataSet)
		{
			if (IsValid(NavData) && NavData->GetConfig().Name == InfantryNavAgentName)
			{
				InfantryNavData = NavData;
			}
		}
	}
	const TSubclassOf<UNavigationQueryFilter> FilterClass = LoadClass<UNavigationQueryFilter>(nullptr, InfantryNavFilterPath);
	if (not IsValid(InfantryNavData) || FilterClass == nullptr)
	{
		UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_REACH no infantry navigation data or filter"));
		GEngine->Exec(&World, TEXT("quit"));
		return;
	}
	const FSharedConstNavQueryFilter QueryFilter = UNavigationQueryFilter::GetQueryFilter(*InfantryNavData, &World, FilterClass);
	int32 TotalSockets = 0;
	int32 TotalOnNavigation = 0;
	int32 TotalReachable = 0;
	const URTSCoverFinderWorldSubsystem* CoverSubsystem = World.GetSubsystem<URTSCoverFinderWorldSubsystem>();
	for (TActorIterator<AApartmentDestructibleActor> ApartmentIterator(&World); ApartmentIterator; ++ApartmentIterator)
	{
		const AApartmentDestructibleActor* Apartment = *ApartmentIterator;
		// What the cover finder offers for this building, checked with a path query of this test's own.
		int32 Published = 0;
		int32 Reachable = 0;
		for (const FRTSCoverPoint& CoverPoint : CoverSubsystem->FindCoverPointsInRadius(Apartment->GetActorLocation(), CoverCountRadius))
		{
			if (not CoverPoint.bSnapSoldierToSocket || CoverSubsystem->ResolveBlockingProvider(CoverPoint) != Apartment)
			{
				continue;
			}
			++Published;
			bool bReached = false;
			for (const double StartDistance : CoverReachStartDistances)
			{
				FNavLocation Start;
				// The normal points from the cover to the soldier; the street is the other way.
				if (not NavigationSystem->ProjectPointToNavigation(CoverPoint.Location - CoverPoint.CoverNormal * StartDistance, Start, CoverReachStartExtent, InfantryNavData, QueryFilter))
				{
					continue;
				}
				const FPathFindingQuery Query(nullptr, *InfantryNavData, Start.Location, CoverPoint.Location, QueryFilter);
				const FPathFindingResult Result = NavigationSystem->FindPathSync(Query);
				if (Result.IsSuccessful() && Result.Path.IsValid() && not Result.Path->IsPartial())
				{
					bReached = true;
					break;
				}
			}
			Reachable += bReached ? 1 : 0;
		}
		const int32 SocketCount = Apartment->GetUsableCoverSocketCount();
		TotalSockets += SocketCount;
		TotalOnNavigation += Published;
		TotalReachable += Reachable;
		if (Published != SocketCount || Reachable != Published)
		{
			UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_REACH actor=%s sockets=%d published=%d reachable=%d"),
			       *Apartment->GetActorNameOrLabel(), SocketCount, Published, Reachable);
		}
	}
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_REACH TOTAL sockets=%d published=%d reachable=%d"),
	       TotalSockets, TotalOnNavigation, TotalReachable);
	GEngine->Exec(&World, TEXT("quit"));
}

void UApartmentDestructionTestSubsystem::ReportCover(UWorld& World, const FString& Label) const
{
	using namespace ApartmentDestructionTest;
	if (not IsValid(M_GameplayActor))
	{
		return;
	}
	TArray<FRTSActorCoverSocket> UsableSockets;
	M_GameplayActor->GetUsableCoverSockets(UsableSockets);
	int32 OpenFiringLaneCount = 0;
	int32 SolidSillCount = 0;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ApartmentGcCoverTest), true);
	for (const FRTSActorCoverSocket& Socket : UsableSockets)
	{
		const FVector Forward = Socket.WorldTransform.GetRotation().GetForwardVector();
		const FVector Feet = Socket.WorldTransform.GetLocation();
		FHitResult FiringHit;
		const FVector Muzzle = Feet + FVector::UpVector * CoverFiringHeight;
		World.LineTraceSingleByChannel(FiringHit, Muzzle, Muzzle + Forward * CoverLaneLength, COLLISION_TRACE_ENEMY, QueryParams);
		OpenFiringLaneCount += FiringHit.GetActor() == M_GameplayActor ? 0 : 1;
		FHitResult SillHit;
		const FVector Knee = Feet + FVector::UpVector * CoverSillProbeHeight;
		World.LineTraceSingleByChannel(SillHit, Knee, Knee + Forward * CoverLaneLength, COLLISION_TRACE_ENEMY, QueryParams);
		SolidSillCount += SillHit.GetActor() == M_GameplayActor ? 1 : 0;
	}
	int32 PublishedSnapPointCount = 0;
	int32 PublishedTrenchCount = 0;
	int32 PublishedCrouchCount = 0;
	if (const URTSCoverFinderWorldSubsystem* CoverSubsystem = World.GetSubsystem<URTSCoverFinderWorldSubsystem>())
	{
		for (const FRTSCoverPoint& CoverPoint : CoverSubsystem->FindCoverPointsInRadius(SiteCentre, CoverCountRadius))
		{
			if (not CoverPoint.bSnapSoldierToSocket || CoverSubsystem->ResolveBlockingProvider(CoverPoint) != M_GameplayActor)
			{
				continue;
			}
			++PublishedSnapPointCount;
			PublishedTrenchCount += CoverPoint.CoverType == ERTSCoverType::TrenchStandUp ? 1 : 0;
			PublishedCrouchCount += CoverPoint.CoverType == ERTSCoverType::Crouch ? 1 : 0;
		}
	}
	UE_LOG(LogRTSApartmentGcTest, Display,
	       TEXT("RTS_APARTMENT_GC COVER after=%s standing_parts=%d usable_sockets=%d published_points=%d trench=%d crouch=%d open_firing_lanes=%d solid_sills=%d"),
	       *Label, M_GameplayActor->GetStandingPartCount(), UsableSockets.Num(), PublishedSnapPointCount,
	       PublishedTrenchCount, PublishedCrouchCount, OpenFiringLaneCount, SolidSillCount);
}

void UApartmentDestructionTestSubsystem::ReportWindow(const UWorld& World, const FString& Label)
{
	using namespace ApartmentDestructionTest;
	const double AverageMilliseconds = M_Window.FrameCount > 0
		                                   ? M_Window.TotalFrameSeconds / M_Window.FrameCount * SecondsToMilliseconds
		                                   : 0.0;
	FVector BoundsExtent = FVector::ZeroVector;
	FVector BoundsOrigin = FVector::ZeroVector;
	if (const UGeometryCollectionComponent* TestedGeometryComponent = GetTestedGeometryComponent())
	{
		const FBoxSphereBounds Bounds = TestedGeometryComponent->Bounds;
		BoundsExtent = Bounds.BoxExtent;
		BoundsOrigin = Bounds.Origin;
	}
	UE_LOG(LogRTSApartmentGcTest, Display,
	       TEXT("RTS_APARTMENT_GC WINDOW after=%s time=%.1f frames=%d avg_ms=%.2f worst_ms=%.2f avg_draw_calls=%lld breaks=%d total_breaks=%d bounds_origin=%s bounds_extent=%s"),
	       *Label, World.GetTimeSeconds(), M_Window.FrameCount, AverageMilliseconds,
	       M_Window.WorstFrameSeconds * SecondsToMilliseconds,
	       M_Window.FrameCount > 0 ? M_Window.TotalDrawCalls / M_Window.FrameCount : 0, M_Window.BreakEventCount, M_TotalBreakEvents,
	       *BoundsOrigin.ToCompactString(), *BoundsExtent.ToCompactString());
	M_Window = FApartmentDestructionTestWindow();
}

void UApartmentDestructionTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("AptBuilder") / TEXT("gc_test") / FString::Printf(
		TEXT("%s_%02d_%s.png"), *M_RunName, M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

void UApartmentDestructionTestSubsystem::Finish(UWorld& World)
{
	bM_IsFinished = true;
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC RESULT DONE total_breaks=%d"), M_TotalBreakEvents);
	if (GEngine != nullptr)
	{
		// One more second so the last screenshot is written before the process goes away.
		GEngine->Exec(&World, TEXT("quit"));
	}
}

void UApartmentDestructionTestSubsystem::OnChaosBreak(const FChaosBreakEvent& BreakEvent)
{
	++M_Window.BreakEventCount;
	++M_TotalBreakEvents;
}

bool UApartmentDestructionTestSubsystem::GetIsValidPrototypeActor() const
{
	if (IsValid(M_PrototypeActor))
	{
		return true;
	}
	UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_GC prototype actor is not valid."));
	return false;
}

bool UApartmentDestructionTestSubsystem::GetIsValidGeometryDamage() const
{
	if (IsValid(M_GeometryDamage))
	{
		return true;
	}
	UE_LOG(LogRTSApartmentGcTest, Error, TEXT("RTS_APARTMENT_GC geometry damage component is not valid."));
	return false;
}
