// Copyright (C) Bas Blokzijl - All rights reserved.

#include "LookoutTowerCaptureActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRoute.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace LookoutTowerConstants
{
	// More than any squad has: a taken tower refuses capture orders (the order checks the squad size first).
	constexpr int32 LockedTowerCaptureUnitsNeeded = 999;
	// Steel headframe and stone house: shells do not bring it down, so the holder keeps it.
	constexpr float TowerHealth = 1.0e7f;
	constexpr int32 DefaultCaptureUnitsNeeded = 2;
	constexpr float DefaultCaptureDurationSeconds = 20.f;
	// Vision bubble lifespan: 0 keeps it for good (AActor::SetLifeSpan).
	constexpr float PermanentRevealSeconds = 0.f;
}

namespace LookoutTowerHelpers
{
	bool GetHasSocketContaining(const UStaticMeshComponent& MeshComponent, const FString& SocketNameFragment)
	{
		const TArray<FName> SocketNames = MeshComponent.GetAllSocketNames();
		for (const FName& SocketName : SocketNames)
		{
			if (SocketName.ToString().Contains(SocketNameFragment))
			{
				return true;
			}
		}
		return false;
	}
}

ALookoutTowerCaptureActor::ALookoutTowerCaptureActor(const FObjectInitializer& ObjectInitializer)
	: ACaptureActor(ObjectInitializer)
{
	// Ticks only while held, to turn the sheaves.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);
	Health = LookoutTowerConstants::TowerHealth;
	CaptureSettings.AmountCaptureUnitsNeeded = LookoutTowerConstants::DefaultCaptureUnitsNeeded;
	CaptureSettings.CaptureDuration = LookoutTowerConstants::DefaultCaptureDurationSeconds;
	CaptureProgressText = NSLOCTEXT("LookoutTower", "CaptureProgress", "Manning the lookout...");
}

void ALookoutTowerCaptureActor::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CacheTaggedComponents();
	BeginPlay_SetupTowerMesh();
	BeginPlay_BindWatchedConvoyRoutes();
	SetHeldPanelsVisible(false);
}

void ALookoutTowerCaptureActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LookoutCaptureTimerHandle);
	}
	UnbindConvoyRoutes();
	if (GetIsValidRevealBubble())
	{
		M_RevealBubble->Destroy();
	}
	M_RevealBubble = nullptr;

	Super::EndPlay(EndPlayReason);
}

void ALookoutTowerCaptureActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FRotator SpinStep(0.f, 0.f, LookoutSettings.SpinDegreesPerSecond * DeltaSeconds);
	for (USceneComponent* SpinComponent : M_SpinComponents)
	{
		if (not IsValid(SpinComponent))
		{
			continue;
		}
		SpinComponent->AddLocalRotation(SpinStep);
	}
}

int32 ALookoutTowerCaptureActor::GetCaptureUnitAmountNeeded() const
{
	if (bM_IsCaptureLocked || bM_IsHeld)
	{
		return LookoutTowerConstants::LockedTowerCaptureUnitsNeeded;
	}
	return Super::GetCaptureUnitAmountNeeded();
}

void ALookoutTowerCaptureActor::OnCaptureByPlayer(const int32 Player)
{
	if (bM_IsCaptureLocked || bM_IsHeld)
	{
		// A second squad that slipped through must not restart the progress bar.
		return;
	}
	bM_IsCaptureLocked = true;
	Super::OnCaptureByPlayer(Player);

	const float CaptureDurationSeconds = CaptureSettings.CaptureDuration;
	UWorld* World = GetWorld();
	if (CaptureDurationSeconds <= 0.f || not IsValid(World))
	{
		OnLookoutCaptured(Player);
		return;
	}

	TWeakObjectPtr<ALookoutTowerCaptureActor> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_LookoutCaptureTimerHandle,
		[WeakThis, Player]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnLookoutCaptured(Player);
		},
		CaptureDurationSeconds,
		false);
}

void ALookoutTowerCaptureActor::OnLookoutCaptured(const int32 Player)
{
	if (bM_IsHeld)
	{
		return;
	}

	bM_IsHeld = true;
	bM_IsCaptureLocked = true;
	M_HoldingPlayer = Player;
	const UWorld* World = GetWorld();
	M_HeldSinceSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.0;

	SetHeldPanelsVisible(true);
	StartSpin();
	if (FRTSPlayers::IsHumanPlayer(Player))
	{
		StartFogReveal(Player);
	}
	OnLookoutTowerCaptured.Broadcast(this, Player);
}

TArray<ALookoutTowerCaptureActor*> ALookoutTowerCaptureActor::GetAllLookoutTowers(const UObject* WorldContextObject)
{
	TArray<ALookoutTowerCaptureActor*> Towers;
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return Towers;
	}

	for (TActorIterator<ALookoutTowerCaptureActor> It(World); It; ++It)
	{
		Towers.Add(*It);
	}
	return Towers;
}

bool ALookoutTowerCaptureActor::GetIsAnyLookoutTowerHeld(const UObject* WorldContextObject, const int32 Player)
{
	for (const ALookoutTowerCaptureActor* Tower : GetAllLookoutTowers(WorldContextObject))
	{
		if (IsValid(Tower) && Tower->GetIsHeld() && Tower->GetHoldingPlayer() == Player)
		{
			return true;
		}
	}
	return false;
}

void ALookoutTowerCaptureActor::BeginPlay_CacheTaggedComponents()
{
	M_HeldPanelComponents.Reset();
	M_SpinComponents.Reset();

	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent->ComponentHasTag(LookoutSettings.HeldPanelTag))
		{
			M_HeldPanelComponents.Add(SceneComponent);
		}
		if (SceneComponent->ComponentHasTag(LookoutSettings.SpinTag))
		{
			M_SpinComponents.Add(SceneComponent);
		}
	}
}

void ALookoutTowerCaptureActor::BeginPlay_SetupTowerMesh()
{
	const FString SocketFilter = LookoutSettings.CaptureLocationSocketFilter.ToString();
	UStaticMeshComponent* TowerMesh = FindMeshWithSocketContaining(SocketFilter);
	if (not IsValid(TowerMesh))
	{
		RTSFunctionLibrary::ReportError(
			"Lookout tower " + GetName() + " has no mesh with a socket containing '" + SocketFilter +
			"'; capturing units walk to the tower's pivot instead.");
		return;
	}
	SetupCaptureLocations(TowerMesh, LookoutSettings.CaptureLocationSocketFilter);

	// The Blueprint keeps BlockAll (units and the navmesh); stone and steel also stop shells and every player's shots.
	TowerMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(TowerMesh, ECR_Block);
}

void ALookoutTowerCaptureActor::BeginPlay_BindWatchedConvoyRoutes()
{
	for (AEnemyConvoyRoute* Route : FindWatchedConvoyRoutes())
	{
		Route->OnConvoyStarted.AddUniqueDynamic(this, &ALookoutTowerCaptureActor::HandleConvoyStarted);
		M_BoundRoutes.Add(Route);
	}
}

UStaticMeshComponent* ALookoutTowerCaptureActor::FindMeshWithSocketContaining(const FString& SocketNameFragment) const
{
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (LookoutTowerHelpers::GetHasSocketContaining(*MeshComponent, SocketNameFragment))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

void ALookoutTowerCaptureActor::SetHeldPanelsVisible(const bool bVisible) const
{
	constexpr bool bPropagateToChildren = true;
	for (USceneComponent* PanelComponent : M_HeldPanelComponents)
	{
		if (not IsValid(PanelComponent))
		{
			continue;
		}
		PanelComponent->SetVisibility(bVisible, bPropagateToChildren);
	}
}

void ALookoutTowerCaptureActor::StartSpin()
{
	if (M_SpinComponents.IsEmpty() || LookoutSettings.SpinDegreesPerSecond <= 0.f)
	{
		return;
	}
	SetActorTickEnabled(true);
}

void ALookoutTowerCaptureActor::StartFogReveal(const int32 Player)
{
	UWorld* World = GetWorld();
	if (LookoutSettings.RevealRadius <= 0.f || not IsValid(World) || GetIsValidRevealBubble())
	{
		return;
	}

	const UStaticMeshComponent* RevealMesh = FindMeshWithSocketContaining(LookoutSettings.RevealSocketName.ToString());
	const FVector RevealLocation = IsValid(RevealMesh)
		                               ? RevealMesh->GetSocketLocation(LookoutSettings.RevealSocketName)
		                               : GetActorLocation();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	M_RevealBubble = World->SpawnActor<AGlobalAbilityVisionBubble>(
		AGlobalAbilityVisionBubble::StaticClass(), RevealLocation, FRotator::ZeroRotator, SpawnParameters);
	if (not GetIsValidRevealBubble())
	{
		RTSFunctionLibrary::ReportError("Lookout tower " + GetName() + " could not spawn its fog reveal.");
		return;
	}
	M_RevealBubble->StartVision(LookoutSettings.RevealRadius, LookoutTowerConstants::PermanentRevealSeconds,
	                            static_cast<uint8>(Player));
}

bool ALookoutTowerCaptureActor::GetIsValidRevealBubble() const
{
	// No error report: the bubble only exists once a human player holds the tower.
	return IsValid(M_RevealBubble);
}

TArray<AEnemyConvoyRoute*> ALookoutTowerCaptureActor::FindWatchedConvoyRoutes() const
{
	TArray<AEnemyConvoyRoute*> Routes;
	if (LookoutSettings.WatchedRouteIds.IsEmpty())
	{
		for (TActorIterator<AEnemyConvoyRoute> It(GetWorld()); It; ++It)
		{
			Routes.Add(*It);
		}
		return Routes;
	}

	for (const FName& RouteId : LookoutSettings.WatchedRouteIds)
	{
		// Reports an error itself when no route has this id.
		AEnemyConvoyRoute* Route = AEnemyConvoyRoute::FindEnemyConvoyRoute(this, RouteId);
		if (IsValid(Route))
		{
			Routes.AddUnique(Route);
		}
	}
	return Routes;
}

void ALookoutTowerCaptureActor::UnbindConvoyRoutes()
{
	for (const TWeakObjectPtr<AEnemyConvoyRoute>& Route : M_BoundRoutes)
	{
		if (not Route.IsValid())
		{
			continue;
		}
		Route->OnConvoyStarted.RemoveDynamic(this, &ALookoutTowerCaptureActor::HandleConvoyStarted);
	}
	M_BoundRoutes.Reset();
}

void ALookoutTowerCaptureActor::HandleConvoyStarted(AEnemyConvoyRoute* Route, AConvoy* Convoy)
{
	M_WatchedConvoyStartCount++;
	if (not bM_IsHeld || not FRTSPlayers::IsHumanPlayer(M_HoldingPlayer))
	{
		return;
	}
	if (not IsValid(Route) || not IsValid(Convoy) || not GetIsWarningTower(Route))
	{
		return;
	}

	const UConvoyDefinition* Definition = Convoy->GetDefinition();
	if (not IsValid(Definition))
	{
		RTSFunctionLibrary::ReportError(
			"Lookout tower " + GetName() + ": convoy of route " + Route->GetRouteId().ToString() +
			" has no definition, so no cargo to warn about.");
		return;
	}

	OnConvoyWarning.Broadcast(this, M_WatchedConvoyStartCount, Definition->Cargo.CargoType, Route, Convoy);
}

bool ALookoutTowerCaptureActor::GetIsWarningTower(const AEnemyConvoyRoute* Route) const
{
	if (not LookoutSettings.bOnlyFirstHeldTowerWarns)
	{
		return true;
	}

	for (const ALookoutTowerCaptureActor* OtherTower : GetAllLookoutTowers(this))
	{
		if (not IsValid(OtherTower) || OtherTower == this || not OtherTower->GetIsHeld()
			|| OtherTower->GetHoldingPlayer() != M_HoldingPlayer || not OtherTower->GetIsWatchingRoute(Route))
		{
			continue;
		}
		const bool bTakenEarlier = OtherTower->M_HeldSinceSeconds < M_HeldSinceSeconds;
		const bool bTakenSameTimeFirstByName = OtherTower->M_HeldSinceSeconds == M_HeldSinceSeconds
			&& OtherTower->GetName() < GetName();
		if (bTakenEarlier || bTakenSameTimeFirstByName)
		{
			return false;
		}
	}
	return true;
}

bool ALookoutTowerCaptureActor::GetIsWatchingRoute(const AEnemyConvoyRoute* Route) const
{
	for (const TWeakObjectPtr<AEnemyConvoyRoute>& BoundRoute : M_BoundRoutes)
	{
		if (BoundRoute.Get() == Route)
		{
			return true;
		}
	}
	return false;
}
