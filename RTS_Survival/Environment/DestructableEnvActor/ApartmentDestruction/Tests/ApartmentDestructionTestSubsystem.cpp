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
#include "RTS_Survival/Environment/DestructableEnvActor/ApartmentDestruction/ApartmentDestructibleActor.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/GeometryDamageComponent/GeometryDamageComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSApartmentGcTest, Log, All);

namespace ApartmentDestructionTest
{
	const TCHAR* const CommandLineSwitch = TEXT("ApartmentGcPrototypeTest");
	const TCHAR* const DirectSwitch = TEXT("ApartmentGcDirect");
	const TCHAR* const GameplayActorSwitch = TEXT("ApartmentGcGameplayActor");
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
		SpawnGameplayActor(World);
	}
	else
	{
		SpawnPrototype(World);
	}
	BuildImpactScript();
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
	M_GameplayActor = World.SpawnActorDeferred<AApartmentDestructibleActor>(
		AApartmentDestructibleActor::StaticClass(), SpawnTransform);
	if (not IsValid(M_GameplayActor))
	{
		return;
	}
	// A placed actor has its collection assigned in the editor; here it has to be in before BeginPlay runs.
	UGeometryCollectionComponent* GeometryComponent = GetTestedGeometryComponent();
	GeometryComponent->SetRestCollection(Collection);
	GeometryComponent->SetNotifyBreaks(true);
	GeometryComponent->OnChaosBreakEvent.AddDynamic(this, &UApartmentDestructionTestSubsystem::OnChaosBreak);
	M_GameplayActor->FinishSpawning(SpawnTransform);
	M_Camera = World.SpawnActor<ACameraActor>();
	UE_LOG(LogRTSApartmentGcTest, Display, TEXT("RTS_APARTMENT_GC SPAWNED mode=gameplay_actor transforms=%d"),
	       Collection->NumElements(FGeometryCollection::TransformGroup));
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
}

void UApartmentDestructionTestSubsystem::AdvanceScript(UWorld& World)
{
	using namespace ApartmentDestructionTest;
	const FString FinishedLabel = M_Impacts.IsValidIndex(M_NextImpactIndex) ? M_Impacts[M_NextImpactIndex].Label : TEXT("intact");
	ReportWindow(World, FinishedLabel);
	RequestScreenshot(FinishedLabel);

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
	if (bM_UseGameplayActor)
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
	       TEXT("RTS_APARTMENT_GC WINDOW after=%s time=%.1f frames=%d avg_ms=%.2f worst_ms=%.2f breaks=%d total_breaks=%d bounds_origin=%s bounds_extent=%s"),
	       *Label, World.GetTimeSeconds(), M_Window.FrameCount, AverageMilliseconds,
	       M_Window.WorstFrameSeconds * SecondsToMilliseconds, M_Window.BreakEventCount, M_TotalBreakEvents,
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
