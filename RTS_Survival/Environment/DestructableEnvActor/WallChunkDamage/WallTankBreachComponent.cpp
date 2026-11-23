// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallTankBreachComponent.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavAreas/NavArea_Null.h"
#include "NavModifierComponent.h"
#include "AI/NavigationSystemBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "WallDamageShared.h"
#include "WallHoleDamageComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WallGate/WallGateNavigation.h"
#include "RTS_Survival/Environment/WallActor/WallActor.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionTypeSystems.h/ExplosionTypeSystems.h"
#include "RTS_Survival/Navigation/NavQueryFilterSettings/RTSNavQueryFilterSettings.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/NavCollision/NavCollisionEnvObstacle/RTSNavCollisionEnvObstacle.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallTankBreach, Log, All);

namespace WallTankBreachConstants
{
	constexpr float UpdateIntervalSeconds = 0.05f;
	// Rubble decal under a fallen top (min/max size, random offset), like the walls' own death decal.
	const FVector2D RubbleDecalSize(220.f, 320.f);
	const FVector2D RubbleDecalOffset(0.f, 30.f);
	// A crossing that never broke anything is dropped this long after its first contact once the tank has left.
	constexpr double AbandonedCrossingSeconds = 2.0;
	// The first contact dust is smaller than the dust of the falling top.
	constexpr float ContactDustShare = 0.6f;
	// Dust of the falling top rises from halfway up the hull.
	constexpr float FallDustHeightShare = 0.5f;
	// Pushing through a sandbag wall starts when the hull's nose is this share of the wall's thickness from the centre.
	constexpr float ContactThicknessShare = 0.5f;
	// Turning and acceleration while pushing through, as a share of normal.
	constexpr float PushTurnRateShare = 0.5f;
	constexpr float PushAccelerationShare = 0.5f;
	// Enough damage to kill a tall piece outright, whatever its reduction.
	constexpr float KillDamageShareOfMaxHealth = 10.f;
	// The first crumbling step of the top already drops the slab over the hull.
	constexpr float MinTopCollapse = 0.05f;
	// A step that moves no breach edge further than this (cm) cuts nothing new.
	constexpr float BreachChangeTolerance = 1.f;
	// Rubble: share on the tank's exit side, spread off the wall (cm), size of a piece (cm) and how deep it sits.
	constexpr float RubbleTravelSideShare = 0.7f;
	constexpr float RubbleMinDistance = 20.f;
	constexpr float RubbleMaxDistance = 220.f;
	constexpr float RubbleMinSize = 70.f;
	constexpr float RubbleMaxSize = 170.f;
	constexpr float RubbleSinkShare = 0.3f;
	// Rubble spills a little past the broken stretch.
	constexpr float RubbleSpillShare = 0.25f;
	// Rubble lies at a random heading, tipped by up to this much (degrees).
	constexpr float RubbleMaxTilt = 35.f;
	constexpr float FullTurnDegrees = 360.f;
	constexpr float SquareCentimetresPerSquareMetre = 10000.f;
	// Shorter nav boxes than this (cm) are left out.
	constexpr float MinNavBlockLength = 5.f;
	// Rubble lands in the outer quarter of the gap on each side and beyond, not in the tank's track.
	constexpr float RubbleGapEdgeShare = 0.25f;
	// Attachments further than this from the wall's face (cm) stand on their own and stay.
	constexpr float AttachmentWallReach = 150.f;
	// A falling attachment tips this far (degrees) over this many seconds, accelerating.
	// The top's fall takes TopCollapseDuration for every this much wall (cm) over the hull, at least once.
	constexpr float TopCollapseReferenceHeight = 200.f;
	// Probe for a touching wall piece just beyond a path end.
	constexpr float PathEndSupportProbeRadius = 40.f;
	constexpr float PathEndSupportProbeOutset = 30.f;
	// Nav blocks reach this far past the wall's face (the engine grows them by each agent's radius anyway): infantry
	// can stand at a wall, a 1.8 m strip kept them 2 m off it.
	constexpr float NavBlockMarginOverWall = 40.f;
	// Weapon blocks follow the fallen V in columns this wide; neighbours whose tops differ less than the step merge.
	constexpr float WeaponColumnWidth = 50.f;
	constexpr float WeaponTopStep = 20.f;
	// Rubble left on the ground grows with the fallen area: RubblePieces per lane's worth, at most this many times.
	constexpr float MaxRubbleScale = 3.f;
	// Footprint radius of the kit's collapse heaps at scale 1 (SM_BerlinFoot_CollapseHeap_S / _L).
	constexpr float SmallDeathHeapRadius = 300.f;
	constexpr float LargeDeathHeapRadius = 500.f;
	// Removed share of the wall face at which a fallen breach counts as the whole wall gone.
	constexpr float WholeWallFallenShare = 0.95f;
	constexpr float AttachmentFallDegrees = 80.f;
	constexpr float AttachmentFallSeconds = 0.7f;
}

UWallTankBreachComponent::UWallTankBreachComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWallTankBreachComponent::BeginPlay()
{
	Super::BeginPlay();
	M_HoleDamage = GetOwner()->FindComponentByClass<UWallHoleDamageComponent>();
	if (ADestructableEnvActor* const Wall = Cast<ADestructableEnvActor>(GetOwner()))
	{
		M_WallDiedHandle = Wall->OnDestructableEnvActorDied.AddUObject(this, &UWallTankBreachComponent::OnWallDied);
	}
	CollectKitObstacleBoxes();
	BeginPlay_SetUpOwnBreachSoon();
	if (GetIsValidHoleDamage() && not M_HoleDamage->GetIsFieldMode())
	{
		RTSFunctionLibrary::ReportError(TEXT("UWallTankBreachComponent on ") + GetNameSafe(GetOwner())
			+ TEXT(": the wall hole component must use the FieldTexture mask storage for breaches."));
	}
}

void UWallTankBreachComponent::BeginPlay_SetUpOwnBreachSoon()
{
	// AWallActor calls OnWallCollisionSetUp from its collision setup; every other owner gets it here.
	if (GetOwner()->IsA<AWallActor>())
	{
		return;
	}
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	// Next tick: the hole component has found its mesh and laid out its field by then.
	TWeakObjectPtr<UWallTankBreachComponent> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid() && WeakThis->GetIsValidHoleDamage())
		{
			WeakThis->OnWallCollisionSetUp(WeakThis->M_HoleDamage->GetWallMeshComponent());
		}
	}));
}

void UWallTankBreachComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimer);
	}
	if (IsValid(M_WallMesh))
	{
		M_WallMesh->OnComponentBeginOverlap.RemoveDynamic(this, &UWallTankBreachComponent::OnWallBeginOverlap);
		M_WallMesh->OnComponentEndOverlap.RemoveDynamic(this, &UWallTankBreachComponent::OnWallEndOverlap);
	}
	if (ADestructableEnvActor* const Wall = Cast<ADestructableEnvActor>(GetOwner()))
	{
		Wall->OnDestructableEnvActorDied.Remove(M_WallDiedHandle);
	}
	ReleaseAllTanks();
	Super::EndPlay(EndPlayReason);
}

void UWallTankBreachComponent::OnWallCollisionSetUp(UPrimitiveComponent* const WallMesh)
{
	if (not IsValid(WallMesh))
	{
		return;
	}
	M_WallMesh = WallMesh;
	// Tanks drive through: they overlap the wall (the overlap cuts the breach) and the mesh carves no nav; the wall's
	// nav boxes decide who may plan through.
	// Only the human player's tanks breach: AI-side units never raise overlap events on the wall (their response is
	// left as the wall had it, unless that was an overlap).
	WallMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Overlap);
	if (Settings.bAnyVehicleBreaks || Settings.bAnySideBreaks)
	{
		// Fences: the AI's vehicles break them too.
		WallMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Overlap);
	}
	else if (WallMesh->GetCollisionResponseToChannel(COLLISION_OBJ_ENEMY) == ECR_Overlap)
	{
		WallMesh->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Ignore);
	}
	WallMesh->SetGenerateOverlapEvents(true);
	WallMesh->SetCanEverAffectNavigation(false);
	WallMesh->OnComponentBeginOverlap.RemoveDynamic(this, &UWallTankBreachComponent::OnWallBeginOverlap);
	WallMesh->OnComponentEndOverlap.RemoveDynamic(this, &UWallTankBreachComponent::OnWallEndOverlap);
	WallMesh->OnComponentBeginOverlap.AddDynamic(this, &UWallTankBreachComponent::OnWallBeginOverlap);
	WallMesh->OnComponentEndOverlap.AddDynamic(this, &UWallTankBreachComponent::OnWallEndOverlap);
	// A wall that died before its set-up (killed in its first frames) keeps the kit's own nav: its remnant blocks.
	ADestructableEnvActor* const Wall = Cast<ADestructableEnvActor>(GetOwner());
	const bool bIsDead = IsValid(Wall) && not Wall->IsUnitAlive();
	if (Settings.bManageWallNav && not bIsDead)
	{
		BuildWallNav();
	}
}

void UWallTankBreachComponent::Debug_StartCrossing(AActor* const Tank)
{
	if (not GetIsBreachingTank(Tank))
	{
		UE_LOG(LogWallTankBreach, Warning,
		       TEXT("RTS_WALL_BREACH debug crossing ignored: %s may not breach %s"), *GetNameSafe(Tank),
		       *GetNameSafe(GetOwner()));
		return;
	}
	StartCrossing(Tank);
}

void UWallTankBreachComponent::OnWallBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* const OtherActor,
                                                  UPrimitiveComponent* /*OtherComp*/, const int32 /*OtherBodyIndex*/,
                                                  const bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// A dead wall (a ruin remnant) is not breached again: its gaps stay as they are.
	ADestructableEnvActor* const Wall = Cast<ADestructableEnvActor>(GetOwner());
	if (IsValid(Wall) && not Wall->IsUnitAlive())
	{
		return;
	}
	if (not GetIsBreachingTank(OtherActor))
	{
		return;
	}
	StartCrossing(OtherActor);
}

void UWallTankBreachComponent::OnWallEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* const OtherActor,
                                                UPrimitiveComponent* /*OtherComp*/, const int32 /*OtherBodyIndex*/)
{
	FWallTankCrossing* const Crossing = FindCrossing(OtherActor);
	if (Crossing == nullptr || not GetIsValidWallMesh())
	{
		return;
	}
	// A tank can touch the wall with several components; it has left once none of them overlaps.
	TArray<UPrimitiveComponent*> OverlappingComponents;
	M_WallMesh->GetOverlappingComponents(OverlappingComponents);
	for (const UPrimitiveComponent* const Overlapping : OverlappingComponents)
	{
		if (IsValid(Overlapping) && Overlapping->GetOwner() == OtherActor)
		{
			return;
		}
	}
	Crossing->bIsOverlapping = false;
}

bool UWallTankBreachComponent::GetIsBreachingTank(const AActor* const Actor) const
{
	const IRTSNavAgentInterface* const NavAgent = Cast<IRTSNavAgentInterface>(Actor);
	if (NavAgent == nullptr)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	if (Settings.bAnyVehicleBreaks)
	{
		// Wire fences: any vehicle of any side, whoever owns the fence.
		return AgentType == ERTSNavAgents::LightTank || AgentType == ERTSNavAgents::MediumTank
			|| AgentType == ERTSNavAgents::HeavyTank;
	}
	if (Settings.bAnySideBreaks)
	{
		// Concrete fences: medium and heavy vehicles of any side, whoever owns the fence.
		return AgentType == ERTSNavAgents::MediumTank || AgentType == ERTSNavAgents::HeavyTank;
	}
	const bool bIsAllowedClass = AgentType == ERTSNavAgents::HeavyTank
		|| (AgentType == ERTSNavAgents::MediumTank && not Settings.bHeavyTanksOnly);
	if (not bIsAllowedClass)
	{
		return false;
	}
	// Only the human player's tanks break walls, and only walls not of his side (the Soviet walls; the unowned ruin
	// walls when allowed); a wall whose owner is unknown otherwise never.
	const URTSComponent* const TankRTS = Actor->FindComponentByClass<URTSComponent>();
	if (not IsValid(TankRTS) || TankRTS->GetOwningPlayer() != RTSWallSides::HumanPlayerIndex)
	{
		return false;
	}
	uint8 WallOwner = 0;
	if (not TryGetWallOwningPlayer(WallOwner))
	{
		return Settings.bBreachUnownedWall;
	}
	return WallOwner != RTSWallSides::HumanPlayerIndex;
}

bool UWallTankBreachComponent::TryGetWallOwningPlayer(uint8& OutOwningPlayer) const
{
	const URTSComponent* const WallRTS = GetOwner()->FindComponentByClass<URTSComponent>();
	if (not IsValid(WallRTS))
	{
		return false;
	}
	OutOwningPlayer = WallRTS->GetOwningPlayer();
	return true;
}

FWallTankCrossing* UWallTankBreachComponent::FindCrossing(const AActor* const Tank)
{
	return M_Crossings.FindByPredicate([Tank](const FWallTankCrossing& Crossing)
	{
		return Crossing.Tank.Get() == Tank;
	});
}

void UWallTankBreachComponent::StartCrossing(AActor* const Tank)
{
	if (not GetIsValidHoleDamage() || not M_HoleDamage->GetIsFieldMode() || M_HoleDamage->GetHasCollapsed())
	{
		return;
	}
	// Debug crossings can start before the wall's collision setup bound the overlap.
	if (not IsValid(M_WallMesh))
	{
		M_WallMesh = M_HoleDamage->GetWallMeshComponent();
	}
	if (FWallTankCrossing* const Existing = FindCrossing(Tank))
	{
		Existing->bIsOverlapping = true;
		return;
	}
	if (bM_IsUpdatingCrossings)
	{
		M_DeferredCrossingTanks.AddUnique(Tank);
		return;
	}
	FWallTankCrossing& Crossing = M_Crossings.AddDefaulted_GetRef();
	Crossing.Tank = Tank;
	// Colliding parts only: the hull and turret, not effects or selection decals.
	constexpr bool bIncludeNonColliding = false;
	constexpr bool bIncludeChildActors = false;
	Crossing.LocalHullBox = Tank->CalculateComponentsBoundingBoxInLocalSpace(bIncludeNonColliding, bIncludeChildActors);
	// Height from the tank's nav agent: the colliding bounds include the gun, antennas and hatches (Pz IV 375 cm).
	const APawn* const TankPawn = Cast<APawn>(Tank);
	const float AgentHeight = IsValid(TankPawn) ? TankPawn->GetNavAgentPropertiesRef().AgentHeight : 0.f;
	if (AgentHeight > 0.f && Crossing.LocalHullBox.IsValid && Crossing.LocalHullBox.GetSize().Z > AgentHeight)
	{
		Crossing.LocalHullBox.Max.Z = Crossing.LocalHullBox.Min.Z + AgentHeight;
	}
	Crossing.FirstContactTime = GetWorld()->GetTimeSeconds();
	Crossing.Breach.VSlope = Settings.VSlope;
	Crossing.Breach.EdgeBand = Settings.EdgeBand;
	Crossing.Breach.StubHeight = Settings.StubHeight;
	UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH contact %s by %s hull=%s"), *GetNameSafe(GetOwner()),
	       *GetNameSafe(Tank), *Crossing.LocalHullBox.GetSize().ToCompactString());
	// The first step runs on the timer, never inside the overlap callback that started the crossing.
	EnsureUpdateTimer();
}

void UWallTankBreachComponent::EnsureUpdateTimer()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_UpdateTimer))
	{
		return;
	}
	TWeakObjectPtr<UWallTankBreachComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_UpdateTimer, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateCrossings();
		}
	}), WallTankBreachConstants::UpdateIntervalSeconds, true);
}

void UWallTankBreachComponent::UpdateCrossings()
{
	if (not GetIsValidHoleDamage() || M_HoleDamage->GetHasCollapsed())
	{
		ReleaseAllTanks();
	}
	// Index loop, backwards: a step can kill the wall (collapse); new crossings wait in M_DeferredCrossingTanks.
	bM_IsUpdatingCrossings = true;
	for (int32 CrossingIndex = M_Crossings.Num() - 1; CrossingIndex >= 0; --CrossingIndex)
	{
		if (not M_Crossings.IsValidIndex(CrossingIndex))
		{
			continue;
		}
		if (not UpdateCrossing(M_Crossings[CrossingIndex]) && M_Crossings.IsValidIndex(CrossingIndex))
		{
			ReleaseTank(M_Crossings[CrossingIndex]);
			if (M_Crossings[CrossingIndex].bHasWidenedAfterFall)
			{
				RefreshAfterLateWidening(M_Crossings[CrossingIndex]);
			}
			if (M_Crossings.IsValidIndex(CrossingIndex))
			{
				M_Crossings.RemoveAt(CrossingIndex);
			}
		}
	}
	bM_IsUpdatingCrossings = false;
	const TArray<TWeakObjectPtr<AActor>> DeferredTanks = MoveTemp(M_DeferredCrossingTanks);
	M_DeferredCrossingTanks.Reset();
	for (const TWeakObjectPtr<AActor>& DeferredTank : DeferredTanks)
	{
		if (DeferredTank.IsValid())
		{
			StartCrossing(DeferredTank.Get());
		}
	}
	UpdateAttachmentFalls();
	if (M_Crossings.IsEmpty() && M_AttachmentFalls.IsEmpty())
	{
		if (UWorld* const World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_UpdateTimer);
		}
	}
	// Last: the damage can kill and destroy the wall (and this component with it).
	ApplyPendingBreachDamage();
}

void UWallTankBreachComponent::ApplyPendingBreachDamage()
{
	const float Damage = M_PendingBreachDamage;
	M_PendingBreachDamage = 0.f;
	if (Damage <= 0.f)
	{
		return;
	}
	if (ADestructableEnvActor* const Wall = Cast<ADestructableEnvActor>(GetOwner()))
	{
		Wall->ApplyCrushBreachDamage(Damage);
	}
}

bool UWallTankBreachComponent::UpdateCrossing(FWallTankCrossing& Crossing)
{
	if (M_HoleDamage->GetHasCollapsed())
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bIsStillPushing = Crossing.Tank.IsValid() && Crossing.bIsOverlapping;
	if (bIsStillPushing)
	{
		UpdateCrossing_Push(Crossing);
	}
	// The wall above the hull comes down a moment after the first break, whether or not the tank is still in it.
	UpdateCrossing_TopFall(Crossing, Now);
	if (bIsStillPushing)
	{
		return true;
	}
	// Clear of the wall: full speed again, even while the top is still falling.
	ReleaseTank(Crossing);
	if (Crossing.BreachIndex != INDEX_NONE)
	{
		return not Crossing.bHasTopFallen;
	}
	return Now - Crossing.FirstContactTime < WallTankBreachConstants::AbandonedCrossingSeconds;
}

void UWallTankBreachComponent::UpdateCrossing_Push(FWallTankCrossing& Crossing)
{
	using namespace WallTankBreachConstants;
	Crossing.LastTravel = GetTravelDirection(Crossing);
	float UMin = 0.f;
	float UMax = 0.f;
	float GroundZ = 0.f;
	float HullHeight = 0.f;
	if (not GetHullInterval(Crossing, UMin, UMax, GroundZ, HullHeight))
	{
		return;
	}
	// The breach is the union of everything the hull has covered.
	const bool bIsFirstReach = not Crossing.HullZone.bIsValid;
	FWallHoleFieldBreach& Breach = Crossing.Breach;
	Breach.UMin = bIsFirstReach ? UMin - Settings.SideMargin : FMath::Min(Breach.UMin, UMin - Settings.SideMargin);
	Breach.UMax = bIsFirstReach ? UMax + Settings.SideMargin : FMath::Max(Breach.UMax, UMax + Settings.SideMargin);
	Breach.GroundZ = bIsFirstReach ? GroundZ : FMath::Min(Breach.GroundZ, GroundZ);
	Breach.HullHeight = bIsFirstReach ? HullHeight : FMath::Max(Breach.HullHeight, HullHeight);
	Crossing.HullZone = FBox2D(FVector2D(UMin, GroundZ), FVector2D(UMax, GroundZ + HullHeight));
	Crossing.CutUMin = bIsFirstReach ? UMin - Settings.SideMargin : FMath::Min(Crossing.CutUMin, UMin - Settings.SideMargin);
	Crossing.CutUMax = bIsFirstReach ? UMax + Settings.SideMargin : FMath::Max(Crossing.CutUMax, UMax + Settings.SideMargin);
	constexpr bool bKeepDebrisOutOfHull = true;
	if (ApplyBreachStep(Crossing, Settings.PushDebrisSpeedScale, bKeepDebrisOutOfHull) <= 0.f)
	{
		// Through an existing gap: nothing breaks, nothing slows the tank, no dust.
		return;
	}
	if (Crossing.bHasTopFallen)
	{
		Crossing.bHasWidenedAfterFall = true;
	}
	SlowTank(Crossing);
	if (Crossing.FirstBreakTime >= 0.0)
	{
		return;
	}
	Crossing.FirstBreakTime = GetWorld()->GetTimeSeconds();
	PlayBreachEffects(GetBreachWorldPoint(Breach, 0.f), Settings.DustScale * ContactDustShare);
}

void UWallTankBreachComponent::UpdateCrossing_TopFall(FWallTankCrossing& Crossing, const double Now)
{
	using namespace WallTankBreachConstants;
	if (Crossing.BreachIndex == INDEX_NONE || Crossing.bHasTopFallen || Crossing.FirstBreakTime < 0.0)
	{
		return;
	}
	const double FallingFor = Now - Crossing.FirstBreakTime - Settings.TopCollapseDelay;
	if (FallingFor < 0.0)
	{
		return;
	}
	if (Settings.bKillWhenTopFalls)
	{
		// A tall piece cannot stand over a tank-sized gap: all of it comes down (nav and weapon collision open over the
		// whole run), then its own death plays (dust, debris, heap); the remnant it leaves is the removed field.
		Crossing.bHasTopStartedFalling = true;
		Crossing.bHasTopFallen = true;
		Crossing.LaneIndex = M_TankLanes.Add(FVector2f(Crossing.CutUMin, Crossing.CutUMax));
		Crossing.Breach.UMin = -Crossing.Breach.EdgeBand;
		Crossing.Breach.UMax = M_HoleDamage->GetField().GetPathLength() + Crossing.Breach.EdgeBand;
		Crossing.Breach.TopCollapse = 1.f;
		OnTopStartsFalling(Crossing);
		constexpr bool bKeepDebrisOutOfHullOnKill = false;
		ApplyBreachStep(Crossing, Settings.FallDebrisSpeedScale, bKeepDebrisOutOfHullOnKill);
		OnTopFallen(Crossing);
		M_PendingBreachDamage += FMath::Max(M_HoleDamage->GetKnownMaxOwnerHealth(), 1.f) * KillDamageShareOfMaxHealth;
		return;
	}
	if (not Crossing.bHasTopStartedFalling)
	{
		Crossing.bHasTopStartedFalling = true;
		Crossing.LaneIndex = M_TankLanes.Add(FVector2f(Crossing.CutUMin, Crossing.CutUMax));
		ExtendBreachOverFreeEnds(Crossing.Breach, Crossing.BreachIndex);
		OnTopStartsFalling(Crossing);
	}
	// The slab over the hull drops first, then the edges crumble out to the full V.
	const float CollapseDuration = GetTopCollapseDuration(Crossing.Breach);
	const float Progress = CollapseDuration > 0.f
		                       ? FMath::Clamp(static_cast<float>(FallingFor) / CollapseDuration, 0.f, 1.f)
		                       : 1.f;
	Crossing.Breach.TopCollapse = FMath::Max(Progress, MinTopCollapse);
	constexpr bool bKeepDebrisOutOfHull = false;
	ApplyBreachStep(Crossing, Settings.FallDebrisSpeedScale, bKeepDebrisOutOfHull);
	if (Progress < 1.f)
	{
		return;
	}
	// Gaps that finished falling meanwhile (two tanks side by side): a pier left between them falls now.
	const float UMinBeforeRecheck = Crossing.Breach.UMin;
	const float UMaxBeforeRecheck = Crossing.Breach.UMax;
	ExtendBreachOverFreeEnds(Crossing.Breach, Crossing.BreachIndex);
	if (Crossing.Breach.UMin != UMinBeforeRecheck || Crossing.Breach.UMax != UMaxBeforeRecheck)
	{
		ApplyBreachStep(Crossing, Settings.FallDebrisSpeedScale, bKeepDebrisOutOfHull);
	}
	Crossing.bHasTopFallen = true;
	OnTopFallen(Crossing);
	// Nothing left standing (the free ends came down too): the wall's own death plays instead of an invisible wall.
	if (M_HoleDamage->GetRemovedFaceShare() >= WholeWallFallenShare)
	{
		M_PendingBreachDamage += FMath::Max(M_HoleDamage->GetKnownMaxOwnerHealth(), 1.f) * KillDamageShareOfMaxHealth;
	}
}

bool UWallTankBreachComponent::GetHullInterval(const FWallTankCrossing& Crossing, float& OutUMin, float& OutUMax,
                                               float& OutGroundZ, float& OutHullHeight) const
{
	if (not GetIsValidWallMesh() || not Crossing.LocalHullBox.IsValid)
	{
		return false;
	}
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const FTransform TankTransform = Crossing.Tank->GetActorTransform();
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector Extent = Crossing.LocalHullBox.GetExtent();
	// Half axes of the hull box in the wall mesh's space: correct for scaled tanks and scaled or stretched walls.
	const FVector MeshHalfForward = MeshTransform.InverseTransformVector(TankTransform.TransformVector(
		FVector(Extent.X, 0.0, 0.0)));
	// Width from the tank's nav radius (its hull), not its bounds: a turret turned sideways must not widen the gap.
	const APawn* const TankPawn = Cast<APawn>(Crossing.Tank.Get());
	const float NavRadius = IsValid(TankPawn) ? TankPawn->GetNavAgentPropertiesRef().AgentRadius : 0.f;
	const FVector MeshHalfRight = NavRadius > 0.f
		                              ? MeshTransform.InverseTransformVector(TankTransform.GetUnitAxis(EAxis::Y) * NavRadius)
		                              : MeshTransform.InverseTransformVector(TankTransform.TransformVector(
			                              FVector(0.0, Extent.Y, 0.0)));
	const FVector MeshHalfUp = MeshTransform.InverseTransformVector(TankTransform.TransformVector(
		FVector(0.0, 0.0, Extent.Z)));
	const FVector MeshCentre = MeshTransform.InverseTransformPosition(
		TankTransform.TransformPosition(Crossing.LocalHullBox.GetCenter()));
	const FVector2D Forward2D(MeshHalfForward.X, MeshHalfForward.Y);
	const FVector2D Right2D(MeshHalfRight.X, MeshHalfRight.Y);
	// The nose touches a thick wall's face before it reaches the centre line.
	const float NoseReach = Field.GetThickness() * WallTankBreachConstants::ContactThicknessShare;
	const FVector2D HalfExtent(Forward2D.Size() + NoseReach, Right2D.Size());
	if (not Field.GetFootprintInterval(FVector2D(MeshCentre.X, MeshCentre.Y), Forward2D.GetSafeNormal(),
	                                   Right2D.GetSafeNormal(), HalfExtent, OutUMin, OutUMax))
	{
		return false;
	}
	const float HalfHeight = static_cast<float>(FMath::Abs(MeshHalfUp.Z));
	OutGroundZ = FMath::Max(Field.GetZMin(), static_cast<float>(MeshCentre.Z) - HalfHeight);
	OutHullHeight = FMath::Min(HalfHeight * 2.f, Field.GetZMax() - OutGroundZ);
	return true;
}

FVector UWallTankBreachComponent::GetTravelDirection(const FWallTankCrossing& Crossing) const
{
	const AActor* const Tank = Crossing.Tank.Get();
	const FVector Velocity = Tank->GetVelocity();
	const FVector Direction = Velocity.SizeSquared2D() > 1.0 ? Velocity : Tank->GetActorForwardVector();
	return FVector(Direction.X, Direction.Y, 0.0).GetSafeNormal();
}

void UWallTankBreachComponent::SlowTank(FWallTankCrossing& Crossing)
{
	ATankMaster* const Tank = Cast<ATankMaster>(Crossing.Tank.Get());
	if (Crossing.bIsSlowed || not IsValid(Tank))
	{
		return;
	}
	// Breaking through costs momentum; heavier hulls keep more of it.
	const ERTSNavAgents AgentType = Tank->GetRTSNavAgentType();
	float SpeedShare = Settings.MediumTankPushSpeedShare;
	if (AgentType == ERTSNavAgents::HeavyTank)
	{
		SpeedShare = Settings.HeavyTankPushSpeedShare;
	}
	else if (AgentType == ERTSNavAgents::LightTank)
	{
		SpeedShare = Settings.LightTankPushSpeedShare;
	}
	Tank->SetMobilityRestriction(this, SpeedShare, WallTankBreachConstants::PushTurnRateShare,
	                             WallTankBreachConstants::PushAccelerationShare);
	Crossing.bIsSlowed = true;
}

void UWallTankBreachComponent::ReleaseTank(FWallTankCrossing& Crossing) const
{
	if (not Crossing.bIsSlowed)
	{
		return;
	}
	Crossing.bIsSlowed = false;
	if (ATankMaster* const Tank = Cast<ATankMaster>(Crossing.Tank.Get()))
	{
		Tank->ClearMobilityRestriction(this);
	}
}

void UWallTankBreachComponent::ReleaseAllTanks()
{
	for (FWallTankCrossing& Crossing : M_Crossings)
	{
		ReleaseTank(Crossing);
	}
	M_Crossings.Reset();
}

float UWallTankBreachComponent::ApplyBreachStep(FWallTankCrossing& Crossing, const float DebrisSpeedScale,
                                                const bool bKeepDebrisOutOfHull)
{
	using namespace WallTankBreachConstants;
	if (not GetIsValidHoleDamage())
	{
		return 0.f;
	}
	const FWallHoleFieldBreach& Breach = Crossing.Breach;
	const FWallHoleFieldBreach& Applied = Crossing.AppliedBreach;
	// Also a tank parked in an existing gap (no breach of its own) is not cut again every update.
	const bool bIsUnchanged = Crossing.bHasAppliedStep
		&& Breach.UMin >= Applied.UMin - BreachChangeTolerance && Breach.UMax <= Applied.UMax + BreachChangeTolerance
		&& Breach.HullHeight <= Applied.HullHeight + BreachChangeTolerance
		&& Breach.TopCollapse <= Applied.TopCollapse;
	if (bIsUnchanged)
	{
		return 0.f;
	}
	const float FaceArea = M_HoleDamage->GetField().GetFaceArea();
	const FBox2D NoDebrisZone = bKeepDebrisOutOfHull ? Crossing.HullZone : FBox2D(ForceInit);
	const float Removed = M_HoleDamage->ApplyBreach(Crossing.BreachIndex, Breach, Crossing.LastTravel,
	                                                DebrisSpeedScale, NoDebrisZone);
	Crossing.AppliedBreach = Breach;
	Crossing.bHasAppliedStep = true;
	if (Removed <= 0.f || FaceArea <= 0.f)
	{
		return 0.f;
	}
	const float Damage = Removed / FaceArea * M_HoleDamage->GetKnownMaxOwnerHealth() * Settings.HealthDamageScale;
	UE_LOG(LogWallTankBreach, Verbose, TEXT("RTS_WALL_BREACH step %s removed=%.2f m2 damage=%.0f share=%.2f"),
	       *GetNameSafe(GetOwner()), Removed / SquareCentimetresPerSquareMetre, Damage,
	       M_HoleDamage->GetRemovedFaceShare());
	M_PendingBreachDamage += Damage;
	return Removed;
}

void UWallTankBreachComponent::ExtendBreachOverFreeEnds(FWallHoleFieldBreach& Breach, const int32 OwnBreachIndex) const
{
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const float WallHeight = Field.GetZMax() - Breach.GroundZ;
	const float MinStandingWidth = FMath::Min(WallHeight * Settings.MinFreeStandingWidthShare,
	                                          Settings.MaxFreeStandingWidth);
	FWallHoleFieldBreach FullyFallen = Breach;
	FullyFallen.TopCollapse = 1.f;
	const float TopReach = Field.GetBreachTopHalfWidth(FullyFallen) - (Breach.UMax - Breach.UMin) * 0.5f;
	// The stretch between the top of the V and a wall end is all that holds that end up.
	// A pier between this gap and one that fell earlier holds up nothing but itself: too narrow, it falls too.
	for (int32 BreachIndex = 0; BreachIndex < M_HoleDamage->GetBreachCount(); ++BreachIndex)
	{
		const FWallHoleFieldBreach* const Other = M_HoleDamage->GetBreach(BreachIndex);
		if (BreachIndex == OwnBreachIndex || Other == nullptr || Other->TopCollapse < 1.f)
		{
			continue;
		}
		const float OtherTopReach = Field.GetBreachTopHalfWidth(*Other) - (Other->UMax - Other->UMin) * 0.5f;
		if (Other->UMax <= Breach.UMin && Breach.UMin - TopReach - (Other->UMax + OtherTopReach) < MinStandingWidth)
		{
			Breach.UMin = FMath::Min(Breach.UMin, Other->UMax);
		}
		if (Other->UMin >= Breach.UMax && Other->UMin - OtherTopReach - (Breach.UMax + TopReach) < MinStandingWidth)
		{
			Breach.UMax = FMath::Max(Breach.UMax, Other->UMin);
		}
	}
	if (Breach.UMin - TopReach < MinStandingWidth && not GetIsPathEndSupported(0.f))
	{
		Breach.UMin = -Breach.EdgeBand;
	}
	if (Field.GetPathLength() - (Breach.UMax + TopReach) < MinStandingWidth
		&& not GetIsPathEndSupported(Field.GetPathLength()))
	{
		Breach.UMax = Field.GetPathLength() + Breach.EdgeBand;
	}
}

void UWallTankBreachComponent::RefreshAfterLateWidening(FWallTankCrossing& Crossing)
{
	Crossing.bHasWidenedAfterFall = false;
	if (not GetIsValidHoleDamage() || M_HoleDamage->GetHasCollapsed())
	{
		return;
	}
	if (M_TankLanes.IsValidIndex(Crossing.LaneIndex))
	{
		M_TankLanes[Crossing.LaneIndex] = FVector2f(Crossing.CutUMin, Crossing.CutUMax);
	}
	const float UMinBefore = Crossing.Breach.UMin;
	const float UMaxBefore = Crossing.Breach.UMax;
	ExtendBreachOverFreeEnds(Crossing.Breach, Crossing.BreachIndex);
	if (Crossing.Breach.UMin != UMinBefore || Crossing.Breach.UMax != UMaxBefore)
	{
		constexpr bool bKeepDebrisOutOfHull = false;
		ApplyBreachStep(Crossing, Settings.FallDebrisSpeedScale, bKeepDebrisOutOfHull);
	}
	if (Settings.bHideAttachmentsOverBreach)
	{
		DropAttachmentsOverBreach(Crossing);
	}
	if (Settings.bManageWallNav)
	{
		BuildWallNav();
	}
	RebuildWeaponCollision();
	UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH late widening %s u=[%.0f, %.0f]"), *GetNameSafe(GetOwner()),
	       Crossing.Breach.UMin, Crossing.Breach.UMax);
}

float UWallTankBreachComponent::GetNavBlockHalfDepth() const
{
	using namespace WallTankBreachConstants;
	const float WallHalfThickness = GetIsValidHoleDamage() ? M_HoleDamage->GetField().GetThickness() * 0.5f : 0.f;
	return FMath::Min(Settings.NavBlockHalfDepth, WallHalfThickness + NavBlockMarginOverWall);
}

bool UWallTankBreachComponent::GetIsPathEndSupported(const float U) const
{
	using namespace WallTankBreachConstants;
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not GetIsValidWallMesh() || not GetIsValidHoleDamage())
	{
		return false;
	}
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const bool bIsStart = U <= 0.f;
	const float ProbeU = bIsStart ? U - PathEndSupportProbeOutset : U + PathEndSupportProbeOutset;
	FVector FaceNormal = FVector::ZeroVector;
	const float MidZ = (Field.GetZMin() + Field.GetZMax()) * 0.5f;
	const FVector Probe = M_WallMesh->GetComponentTransform().TransformPosition(
		Field.GetMeshPoint(ProbeU, MidZ, 0.f, FaceNormal));
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(WallBreachEndSupport), false, GetOwner());
	World->OverlapMultiByObjectType(Overlaps, Probe, FQuat::Identity,
	                                FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
	                                FCollisionShape::MakeSphere(PathEndSupportProbeRadius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		// Only a standing wall piece holds the end up: not a dead one, a crate or a fence.
		AActor* const Other = Overlap.GetActor();
		if (not IsValid(Other) || Other == GetOwner() || not Other->IsA<ADestructableEnvActor>() || not Other->IsUnitAlive())
		{
			continue;
		}
		const UWallHoleDamageComponent* const OtherHoles = Other->FindComponentByClass<UWallHoleDamageComponent>();
		// Its collision has no gaps: a neighbour whose own breach reaches the joint holds nothing up there.
		if (IsValid(OtherHoles) && not OtherHoles->GetIsWorldPointInBreach(Probe))
		{
			return true;
		}
	}
	return false;
}

float UWallTankBreachComponent::GetTopCollapseDuration(const FWallHoleFieldBreach& Breach) const
{
	using namespace WallTankBreachConstants;
	const float HeightOverHull = M_HoleDamage->GetField().GetZMax() - Breach.GroundZ - Breach.HullHeight;
	return Settings.TopCollapseDuration * FMath::Max(1.f, HeightOverHull / TopCollapseReferenceHeight);
}

void UWallTankBreachComponent::OnTopStartsFalling(const FWallTankCrossing& Crossing)
{
	using namespace WallTankBreachConstants;
	const FWallHoleFieldBreach& Breach = Crossing.Breach;
	PlayBreachEffects(GetBreachWorldPoint(Breach, Breach.HullHeight * FallDustHeightShare), Settings.DustScale);
}

void UWallTankBreachComponent::OnTopFallen(const FWallTankCrossing& Crossing)
{
	using namespace WallTankBreachConstants;
	const FWallHoleFieldBreach& Breach = Crossing.Breach;
	UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH top fell %s u=[%.0f, %.0f] removed share=%.2f"),
	       *GetNameSafe(GetOwner()), Breach.UMin, Breach.UMax, M_HoleDamage->GetRemovedFaceShare());
	if (M_HoleDamage->GetHasCollapsed())
	{
		return;
	}
	// Under fog of war only the sound and dust are left out (PlayBreachEffects); the wall itself still changes.
	if (Settings.bSpawnRubbleDecal)
	{
		URTSBlueprintFunctionLibrary::RTSSpawnDecal(this, ERTS_DecalType::StoneDestroyed,
		                                            GetBreachWorldPoint(Breach, 0.f), RubbleDecalSize, 0.f,
		                                            RubbleDecalOffset);
	}
	SpawnRubble(Crossing);
	if (Settings.bHideAttachmentsOverBreach)
	{
		DropAttachmentsOverBreach(Crossing);
	}
	if (Settings.bManageWallNav)
	{
		BuildWallNav();
	}
	RebuildWeaponCollision();
}

void UWallTankBreachComponent::RebuildWeaponCollision()
{
	using namespace WallTankBreachConstants;
	if (not GetIsValidWallMesh() || not GetIsValidHoleDamage())
	{
		return;
	}
	if (GetOpenNavIntervals().IsEmpty())
	{
		return;
	}
	if (not bM_HasTakenOverWeaponCollision)
	{
		bM_HasTakenOverWeaponCollision = true;
		M_WeaponResponses = M_WallMesh->GetCollisionResponseToChannels();
		// The mesh keeps the tank overlap (breaching) and building placement; weapons and sight go to the blocks.
		M_WallMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		M_WallMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Ignore);
		M_WallMesh->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Ignore);
		M_WallMesh->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Ignore);
	}
	for (UBoxComponent* const Block : M_WeaponBlocks)
	{
		if (IsValid(Block))
		{
			Block->DestroyComponent();
		}
	}
	M_WeaponBlocks.Reset();
	const FWallHoleField& Field = M_HoleDamage->GetField();
	for (int32 SegmentIndex = 0; SegmentIndex < Field.GetSegmentCount(); ++SegmentIndex)
	{
		FVector2D Start;
		FVector2D End;
		float UStart = 0.f;
		float UEnd = 0.f;
		Field.GetSegment(SegmentIndex, Start, End, UStart, UEnd);
		// Columns along the run: each block is as high as the wall still standing there, so shells and sight lines
		// pass over the shoulders of the fallen V; columns with the same top merge into one block.
		float BlockFrom = UStart;
		float BlockTop = GetStandingTopZ(FMath::Min(UStart + WeaponColumnWidth * 0.5f, UEnd));
		for (float ColumnFrom = UStart + WeaponColumnWidth; ColumnFrom < UEnd; ColumnFrom += WeaponColumnWidth)
		{
			const float ColumnTop = GetStandingTopZ(FMath::Min(ColumnFrom + WeaponColumnWidth * 0.5f, UEnd));
			if (FMath::Abs(ColumnTop - BlockTop) < WeaponTopStep)
			{
				continue;
			}
			AddWeaponBlock(SegmentIndex, BlockFrom, ColumnFrom, BlockTop);
			BlockFrom = ColumnFrom;
			BlockTop = ColumnTop;
		}
		AddWeaponBlock(SegmentIndex, BlockFrom, UEnd, BlockTop);
	}
}

float UWallTankBreachComponent::GetStandingTopZ(const float U) const
{
	using namespace WallTankBreachConstants;
	const FWallHoleField& Field = M_HoleDamage->GetField();
	for (float Z = Field.GetZMax() - WeaponTopStep * 0.5f; Z > Field.GetZMin(); Z -= WeaponTopStep)
	{
		if (not Field.GetIsInBreach(U, Z))
		{
			return Z + WeaponTopStep * 0.5f;
		}
	}
	return Field.GetZMin();
}

void UWallTankBreachComponent::AddWeaponBlock(const int32 SegmentIndex, const float UFrom, const float UTo,
                                              const float TopZ)
{
	const float BottomZ = M_HoleDamage->GetField().GetZMin();
	if (UTo - UFrom < WallTankBreachConstants::MinNavBlockLength || TopZ - BottomZ < WallTankBreachConstants::MinNavBlockLength)
	{
		return;
	}
	const FWallHoleField& Field = M_HoleDamage->GetField();
	FVector2D Start;
	FVector2D End;
	float UStart = 0.f;
	float UEnd = 0.f;
	Field.GetSegment(SegmentIndex, Start, End, UStart, UEnd);
	const FVector2D Direction = (End - Start).GetSafeNormal();
	const FVector2D MeshCentre2D = Start + Direction * ((UFrom + UTo) * 0.5f - UStart);
	const float MidZ = (BottomZ + TopZ) * 0.5f;
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector WorldAlong = MeshTransform.TransformVector(FVector(Direction.X, Direction.Y, 0.0));
	const FVector WorldAcross = MeshTransform.TransformVector(FVector(-Direction.Y, Direction.X, 0.0));
	const FVector WorldUp = MeshTransform.TransformVector(FVector::UpVector);
	UBoxComponent* const Block = NewObject<UBoxComponent>(GetOwner());
	Block->SetMobility(EComponentMobility::Movable);
	Block->SetCanEverAffectNavigation(false);
	Block->SetGenerateOverlapEvents(false);
	// Not the mesh's object type (destructible): area damage queries that type and hits every component it finds, so a
	// breached wall would take splash once per block. Shells and sight find the blocks by trace channel (responses).
	Block->SetCollisionObjectType(ECC_WorldDynamic);
	Block->SetCollisionResponseToChannels(M_WeaponResponses);
	Block->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Ignore);
	Block->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Ignore);
	Block->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Block->SetBoxExtent(FVector((UTo - UFrom) * 0.5f * WorldAlong.Size(), Field.GetThickness() * 0.5f * WorldAcross.Size(),
	                            (TopZ - BottomZ) * 0.5f * WorldUp.Size()), false);
	Block->SetWorldLocationAndRotation(MeshTransform.TransformPosition(FVector(MeshCentre2D.X, MeshCentre2D.Y, MidZ)),
	                                   FRotationMatrix::MakeFromXZ(WorldAlong, WorldUp).ToQuat());
	Block->RegisterComponent();
	// On the mesh: a ruin remnant sinks and turns after its death, and the blocks of its standing stretches with it.
	Block->AttachToComponent(M_WallMesh, FAttachmentTransformRules::KeepWorldTransform);
	M_WeaponBlocks.Add(Block);
}

void UWallTankBreachComponent::OnWallDied()
{
	ReleaseAllTanks();
	if (not M_KitObstacleBoxes.IsEmpty() && not GetOpenNavIntervals().IsEmpty())
	{
		OnWallDied_KeepBreachInRemnant();
		return;
	}
	// The owner's death takes over: the mesh blocks weapons and sight again (a ruin remnant stays and is still shot at
	// and clicked), the weapon blocks go and the kit's own nav boxes come back for its death graph.
	for (UBoxComponent* const Block : M_WeaponBlocks)
	{
		if (IsValid(Block))
		{
			Block->DestroyComponent();
		}
	}
	M_WeaponBlocks.Reset();
	if (bM_HasTakenOverWeaponCollision && IsValid(M_WallMesh))
	{
		M_WallMesh->SetCollisionResponseToChannels(M_WeaponResponses);
		bM_HasTakenOverWeaponCollision = false;
	}
	if (not M_KitObstacleBoxes.IsEmpty())
	{
		HandNavBackToKit();
		return;
	}
	// A Rus wall collapses and is hidden at once (the actor lives on a few seconds): nothing blocks there any more.
	for (URTSNavCollisionEnvObstacle* const NavBlock : M_NavBlocks)
	{
		if (IsValid(NavBlock))
		{
			NavBlock->DestroyComponent();
		}
	}
	M_NavBlocks.Reset();
}

void UWallTankBreachComponent::OnWallDied_KeepBreachInRemnant()
{
	// A breached ruin dies into a remnant that stays (sunk and turned by the kit's death graph): its gap stays open.
	// The kit's boxes stay off (they span the gap); the weapon blocks ride on the mesh; the centre-line nav blocks keep
	// their world box at ground level and now block every unit: a remnant is not breached again.
	for (URTSNavCollisionEnvObstacle* const NavBlock : M_NavBlocks)
	{
		if (IsValid(NavBlock))
		{
			NavBlock->SetKeepExactBoxWhenOwnerMoves(true);
			NavBlock->SetAreaClass(UNavArea_Null::StaticClass());
		}
	}
	RestoreKitAreasOfRelevantBoxes();
	RemoveDeathHeapsOverOpenBreaches();
	UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH remnant %s keeps its gap: nav blocks=%d weapon blocks=%d"),
	       *GetNameSafe(GetOwner()), M_NavBlocks.Num(), M_WeaponBlocks.Num());
}

void UWallTankBreachComponent::CollectKitObstacleBoxes()
{
	M_KitObstacleBoxes.Reset();
	M_KitObstacleAreas.Reset();
	TInlineComponentArray<UShapeComponent*> Shapes(GetOwner());
	for (UShapeComponent* const Shape : Shapes)
	{
		if (IsValid(Shape) && Shape->bDynamicObstacle && Shape->CanEverAffectNavigation())
		{
			M_KitObstacleBoxes.Add(Shape);
			M_KitObstacleAreas.Add(Shape->GetDesiredAreaClass());
		}
	}
}

void UWallTankBreachComponent::SetKitObstacleArea(const TSubclassOf<UNavArea> AreaClass, const bool bIsRelevant)
{
	for (UShapeComponent* const Shape : M_KitObstacleBoxes)
	{
		if (not IsValid(Shape))
		{
			continue;
		}
		if (bIsRelevant && Shape->GetDesiredAreaClass() != AreaClass)
		{
			// SetAreaClassOverride only stores the area: the nav octree has to be told, or the tiles keep the old one.
			Shape->SetAreaClassOverride(AreaClass);
			FNavigationSystem::UpdateComponentData(*Shape);
		}
		if (Shape->CanEverAffectNavigation() != bIsRelevant)
		{
			Shape->SetCanEverAffectNavigation(bIsRelevant);
		}
	}
}

void UWallTankBreachComponent::HandNavBackToKit()
{
	if (M_KitObstacleBoxes.IsEmpty())
	{
		return;
	}
	for (URTSNavCollisionEnvObstacle* const NavBlock : M_NavBlocks)
	{
		if (IsValid(NavBlock))
		{
			NavBlock->DestroyComponent();
		}
	}
	M_NavBlocks.Reset();
	for (int32 BoxIndex = 0; BoxIndex < M_KitObstacleBoxes.Num(); ++BoxIndex)
	{
		UShapeComponent* const Shape = M_KitObstacleBoxes[BoxIndex];
		if (not IsValid(Shape))
		{
			continue;
		}
		Shape->SetAreaClassOverride(M_KitObstacleAreas.IsValidIndex(BoxIndex) ? M_KitObstacleAreas[BoxIndex] : nullptr);
		if (Shape->CanEverAffectNavigation())
		{
			FNavigationSystem::UpdateComponentData(*Shape);
		}
		else
		{
			Shape->SetCanEverAffectNavigation(true);
		}
	}
}

void UWallTankBreachComponent::SetKitObstacleAreaOutsideTankLanes(const TSubclassOf<UNavArea> AreaClass)
{
	for (UShapeComponent* const Shape : M_KitObstacleBoxes)
	{
		if (not IsValid(Shape))
		{
			continue;
		}
		const bool bIsRelevant = not GetIsKitBoxInTankLane(*Shape);
		if (bIsRelevant && Shape->GetDesiredAreaClass() != AreaClass)
		{
			Shape->SetAreaClassOverride(AreaClass);
			FNavigationSystem::UpdateComponentData(*Shape);
		}
		if (Shape->CanEverAffectNavigation() != bIsRelevant)
		{
			Shape->SetCanEverAffectNavigation(bIsRelevant);
		}
	}
}

bool UWallTankBreachComponent::GetIsKitBoxInTankLane(const UShapeComponent& Shape) const
{
	if (M_TankLanes.IsEmpty() || not GetIsValidWallMesh() || not GetIsValidHoleDamage())
	{
		return false;
	}
	// The box's run along the wall: its footprint corners projected onto the wall path.
	const FBoxSphereBounds LocalBounds = Shape.CalcBounds(FTransform::Identity);
	const FTransform& BoxTransform = Shape.GetComponentTransform();
	const FTransform& MeshTransform = M_WallMesh->GetComponentTransform();
	float BoxUMin = TNumericLimits<float>::Max();
	float BoxUMax = TNumericLimits<float>::Lowest();
	for (const float SignX : {-1.f, 1.f})
	{
		for (const float SignY : {-1.f, 1.f})
		{
			const FVector Corner = LocalBounds.Origin + FVector(SignX * LocalBounds.BoxExtent.X,
			                                                    SignY * LocalBounds.BoxExtent.Y, 0.0);
			float U = 0.f;
			float Offset = 0.f;
			M_HoleDamage->GetField().ProjectToPath(
				MeshTransform.InverseTransformPosition(BoxTransform.TransformPosition(Corner)), U, Offset);
			BoxUMin = FMath::Min(BoxUMin, U);
			BoxUMax = FMath::Max(BoxUMax, U);
		}
	}
	for (const FVector2f& Lane : M_TankLanes)
	{
		if (BoxUMax > Lane.X && BoxUMin < Lane.Y)
		{
			return true;
		}
	}
	return false;
}

void UWallTankBreachComponent::RestoreKitAreasOfRelevantBoxes()
{
	for (int32 BoxIndex = 0; BoxIndex < M_KitObstacleBoxes.Num(); ++BoxIndex)
	{
		UShapeComponent* const Shape = M_KitObstacleBoxes[BoxIndex];
		if (not IsValid(Shape) || not Shape->CanEverAffectNavigation() || not M_KitObstacleAreas.IsValidIndex(BoxIndex))
		{
			continue;
		}
		Shape->SetAreaClassOverride(M_KitObstacleAreas[BoxIndex]);
		FNavigationSystem::UpdateComponentData(*Shape);
	}
}

void UWallTankBreachComponent::RemoveDeathHeapsOverOpenBreaches() const
{
	using namespace WallTankBreachConstants;
	if (not GetIsValidWallMesh() || not GetIsValidHoleDamage())
	{
		return;
	}
	// The tank lanes, not the fallen run: heaps beside the lane still come down where the kit put them.
	const TArray<FVector2f>& OpenIntervals = M_TankLanes;
	const TPair<FName, float> HeapArrays[] = {
		{FName(TEXT("DeathHeapsSmall")), SmallDeathHeapRadius}, {FName(TEXT("DeathHeapsLarge")), LargeDeathHeapRadius}
	};
	for (const TPair<FName, float>& HeapArray : HeapArrays)
	{
		const FArrayProperty* const ArrayProperty = FindFProperty<FArrayProperty>(GetOwner()->GetClass(), HeapArray.Key);
		const FStructProperty* const InnerProperty = ArrayProperty != nullptr
			                                             ? CastField<FStructProperty>(ArrayProperty->Inner)
			                                             : nullptr;
		if (InnerProperty == nullptr || InnerProperty->Struct != TBaseStructure<FTransform>::Get())
		{
			RTSFunctionLibrary::ReportError(TEXT("UWallTankBreachComponent on ") + GetNameSafe(GetOwner())
				+ TEXT(": the ruin kit death heap array ") + HeapArray.Key.ToString()
				+ TEXT(" (TArray<FTransform>) is missing; a death heap may land in the breach."));
			continue;
		}
		TArray<FTransform>* const Heaps = ArrayProperty->ContainerPtrToValuePtr<TArray<FTransform>>(GetOwner());
		const int32 Removed = Heaps->RemoveAll([this, &HeapArray, &OpenIntervals](const FTransform& Heap)
		{
			return GetIsHeapOverOpenBreach(Heap, HeapArray.Value, OpenIntervals);
		});
		UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH remnant %s: %d of %s dropped over the breach"),
		       *GetNameSafe(GetOwner()), Removed, *HeapArray.Key.ToString());
	}
}

bool UWallTankBreachComponent::GetIsHeapOverOpenBreach(const FTransform& ActorRelativeHeap, const float HeapRadius,
                                                       const TArray<FVector2f>& OpenIntervals) const
{
	const FVector WorldHeap = GetOwner()->GetActorTransform().TransformPosition(
		ActorRelativeHeap.GetLocation());
	const FVector MeshHeap = M_WallMesh->GetComponentTransform().InverseTransformPosition(WorldHeap);
	float U = 0.f;
	float Offset = 0.f;
	M_HoleDamage->GetField().ProjectToPath(MeshHeap, U, Offset);
	const float Radius = HeapRadius * ActorRelativeHeap.GetScale3D().GetMax();
	// The tank's lane runs across the wall: anything within the nav depth plus the heap's size on either side.
	if (FMath::Abs(Offset) > Radius + Settings.NavBlockHalfDepth)
	{
		return false;
	}
	for (const FVector2f& Open : OpenIntervals)
	{
		if (U + Radius > Open.X && U - Radius < Open.Y)
		{
			return true;
		}
	}
	return false;
}

void UWallTankBreachComponent::PlayBreachEffects(const FVector& WorldLocation, const float DustScale) const
{
	// Fog of war hides enemy walls; their dust and noise would give them away.
	if (GetOwner()->IsHidden())
	{
		return;
	}
	if (IsValid(Settings.DustSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Settings.DustSystem, WorldLocation,
		                                               FRotator::ZeroRotator, FVector(DustScale), true, true,
		                                               ENCPoolMethod::AutoRelease);
	}
	if (IsValid(Settings.BreachSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Settings.BreachSound, WorldLocation, FRotator::ZeroRotator, 1.f, 1.f,
		                                      0.f, Settings.BreachAttenuation, Settings.BreachConcurrency);
	}
}

void UWallTankBreachComponent::SpawnRubble(const FWallTankCrossing& Crossing)
{
	using namespace WallTankBreachConstants;
	const TArray<TObjectPtr<UStaticMesh>>& Meshes = M_HoleDamage->GetDebrisMeshes();
	if (Meshes.IsEmpty() || Settings.RubblePieces <= 0 || not GetIsValidWallMesh())
	{
		return;
	}
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const FWallHoleFieldBreach& Breach = Crossing.Breach;
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const float Spill = (Breach.UMax - Breach.UMin) * RubbleSpillShare;
	// More fell than the lane (free ends, the V of a tall wall): more rubble, up to MaxRubbleScale times.
	// This crossing's own lane (two tanks can cross the same wall at once).
	const FVector2f Lane = M_TankLanes.IsValidIndex(Crossing.LaneIndex)
		                       ? M_TankLanes[Crossing.LaneIndex]
		                       : FVector2f(Breach.UMin, Breach.UMax);
	const float RubbleScale = FMath::Clamp((Breach.UMax - Breach.UMin) / FMath::Max(Lane.Y - Lane.X, 1.f), 1.f,
	                                       MaxRubbleScale);
	const int32 RubbleCount = FMath::RoundToInt32(Settings.RubblePieces * RubbleScale);
	for (int32 PieceIndex = 0; PieceIndex < RubbleCount; ++PieceIndex)
	{
		UStaticMesh* const Mesh = Meshes[FMath::RandRange(0, Meshes.Num() - 1)];
		if (not IsValid(Mesh))
		{
			continue;
		}
		// Outer quarter of the gap and beyond on either side: the tank's track stays clear.
		const float EdgeReach = (Breach.UMax - Breach.UMin) * RubbleGapEdgeShare;
		const float AlongEdge = FMath::FRandRange(-Spill, EdgeReach);
		const float U = FMath::Clamp(FMath::RandBool() ? Breach.UMin + AlongEdge : Breach.UMax - AlongEdge, 0.f,
		                             Field.GetPathLength());
		FVector FrontNormal;
		const FVector OnWall = MeshTransform.TransformPosition(Field.GetMeshPoint(U, Breach.GroundZ, 0.f, FrontNormal));
		const FVector WorldNormal = MeshTransform.TransformVectorNoScale(FrontNormal).GetSafeNormal();
		// Most of the wall falls the way the tank pushed it.
		const float TravelSide = FVector::DotProduct(WorldNormal, Crossing.LastTravel) >= 0.0 ? 1.f : -1.f;
		const float Side = FMath::FRand() < RubbleTravelSideShare ? TravelSide : -TravelSide;
		FVector Location = OnWall + WorldNormal * Side * FMath::FRandRange(RubbleMinDistance, RubbleMaxDistance);
		const FVector MeshSize = Mesh->GetBoundingBox().GetSize();
		const float Scale = FMath::FRandRange(RubbleMinSize, RubbleMaxSize) / FMath::Max(MeshSize.GetMax(), 1.0);
		Location.Z = WallDamageShared::GetGroundHeightBelow(*GetOwner(), Location + FVector::UpVector * RubbleMaxSize)
			- MeshSize.Z * Scale * RubbleSinkShare;
		UInstancedStaticMeshComponent* const Instances = GetOrCreateRubbleInstances(Mesh);
		if (not IsValid(Instances))
		{
			continue;
		}
		const FRotator Rotation(FMath::FRandRange(-RubbleMaxTilt, RubbleMaxTilt), FMath::FRandRange(0.f, FullTurnDegrees),
		                        FMath::FRandRange(-RubbleMaxTilt, RubbleMaxTilt));
		constexpr bool bIsWorldSpace = true;
		Instances->AddInstance(FTransform(Rotation, Location, FVector(Scale)), bIsWorldSpace);
	}
}

UInstancedStaticMeshComponent* UWallTankBreachComponent::GetOrCreateRubbleInstances(UStaticMesh* const Mesh)
{
	for (UInstancedStaticMeshComponent* const Instances : M_RubbleInstances)
	{
		if (IsValid(Instances) && Instances->GetStaticMesh() == Mesh)
		{
			return Instances;
		}
	}
	UInstancedStaticMeshComponent* const Instances = NewObject<UInstancedStaticMeshComponent>(GetOwner());
	Instances->SetStaticMesh(Mesh);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(true);
	Instances->RegisterComponent();
	M_RubbleInstances.Add(Instances);
	return Instances;
}

void UWallTankBreachComponent::DropAttachmentsOverBreach(const FWallTankCrossing& Crossing)
{
	if (not GetIsValidWallMesh())
	{
		return;
	}
	const FWallHoleFieldBreach& Breach = Crossing.Breach;
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const float Centre = (Breach.UMin + Breach.UMax) * 0.5f;
	const float TopHalfWidth = Field.GetBreachTopHalfWidth(Breach);
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(GetOwner());
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		const bool bIsWallOrRubble = MeshComponent == M_WallMesh.Get()
			|| M_RubbleInstances.Contains(Cast<UInstancedStaticMeshComponent>(MeshComponent));
		if (not IsValid(MeshComponent) || bIsWallOrRubble || not MeshComponent->IsVisible())
		{
			continue;
		}
		// Anything standing on the fallen stretch of the wall (flag poles, banners) comes down with it.
		float U = 0.f;
		float Offset = 0.f;
		Field.ProjectToPath(MeshTransform.InverseTransformPosition(MeshComponent->Bounds.Origin), U, Offset);
		const bool bIsOnWall = FMath::Abs(Offset) <= Field.GetThickness() * 0.5f + WallTankBreachConstants::AttachmentWallReach;
		if (not bIsOnWall || FMath::Abs(U - Centre) > TopHalfWidth)
		{
			continue;
		}
		const bool bIsFallingAlready = M_AttachmentFalls.ContainsByPredicate(
			[MeshComponent](const FWallAttachmentFall& Fall) { return Fall.Component.Get() == MeshComponent; });
		if (not bIsFallingAlready)
		{
			StartAttachmentFall(MeshComponent, Crossing.LastTravel);
		}
	}
}

void UWallTankBreachComponent::StartAttachmentFall(UStaticMeshComponent* const Attachment, const FVector& TravelDirection)
{
	const bool bIsFalling = M_AttachmentFalls.ContainsByPredicate([Attachment](const FWallAttachmentFall& Fall)
	{
		return Fall.Component.Get() == Attachment;
	});
	if (bIsFalling)
	{
		return;
	}
	Attachment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Attachment->SetMobility(EComponentMobility::Movable);
	// Detached so it can lie on the ground on its own while the wall stands on.
	Attachment->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	const FBox Bounds = Attachment->Bounds.GetBox();
	const FVector Travel = TravelDirection.IsNearlyZero() ? GetOwner()->GetActorForwardVector() : TravelDirection;
	FWallAttachmentFall& Fall = M_AttachmentFalls.AddDefaulted_GetRef();
	Fall.Component = Attachment;
	Fall.StartTransform = Attachment->GetComponentTransform();
	Fall.Pivot = Bounds.GetCenter();
	// Tipping over along the travel: the rotation axis lies flat, across the travel.
	Fall.Axis = FVector::CrossProduct(FVector::UpVector, Travel.GetSafeNormal2D()).GetSafeNormal();
	const float GroundZ = WallDamageShared::GetGroundHeightBelow(*GetOwner(), Fall.Pivot);
	Fall.DropHeight = FMath::Max(0.f, static_cast<float>(Fall.Pivot.Z) - GroundZ - static_cast<float>(Bounds.GetExtent().GetMin()));
	EnsureUpdateTimer();
}

void UWallTankBreachComponent::UpdateAttachmentFalls()
{
	using namespace WallTankBreachConstants;
	for (int32 FallIndex = M_AttachmentFalls.Num() - 1; FallIndex >= 0; --FallIndex)
	{
		FWallAttachmentFall& Fall = M_AttachmentFalls[FallIndex];
		USceneComponent* const Attachment = Fall.Component.Get();
		if (not IsValid(Attachment))
		{
			M_AttachmentFalls.RemoveAt(FallIndex);
			continue;
		}
		Fall.Elapsed += UpdateIntervalSeconds;
		// Accelerating, as a toppling mast does.
		const float Progress = FMath::Square(FMath::Clamp(Fall.Elapsed / AttachmentFallSeconds, 0.f, 1.f));
		const FQuat Tip(Fall.Axis, FMath::DegreesToRadians(AttachmentFallDegrees * Progress));
		const FVector Location = Fall.Pivot + Tip.RotateVector(Fall.StartTransform.GetLocation() - Fall.Pivot)
			- FVector::UpVector * Fall.DropHeight * Progress;
		Attachment->SetWorldLocationAndRotation(Location, Tip * Fall.StartTransform.GetRotation());
		if (Progress >= 1.f)
		{
			M_AttachmentFalls.RemoveAt(FallIndex);
		}
	}
}

FVector UWallTankBreachComponent::GetBreachWorldPoint(const FWallHoleFieldBreach& Breach,
                                                      const float HeightAboveGround) const
{
	if (not GetIsValidWallMesh())
	{
		return GetOwner()->GetActorLocation();
	}
	FVector FaceNormal;
	const float CentreU = FMath::Clamp((Breach.UMin + Breach.UMax) * 0.5f, 0.f, M_HoleDamage->GetField().GetPathLength());
	const FVector MeshPoint = M_HoleDamage->GetField().GetMeshPoint(CentreU, Breach.GroundZ + HeightAboveGround, 0.f,
	                                                                FaceNormal);
	return M_WallMesh->GetComponentTransform().TransformPosition(MeshPoint);
}

// ---------------------------------------------------------------------------------------------------------------------
// Navigation: boxes along the wall's centre line in the owner's breachable area, open where a breach has fallen.
// ---------------------------------------------------------------------------------------------------------------------

void UWallTankBreachComponent::BuildWallNav()
{
	if (not GetIsValidWallMesh() || not GetIsValidHoleDamage() || not M_HoleDamage->GetField().GetIsInitialised())
	{
		return;
	}
	const TSubclassOf<UNavArea> WallArea = GetWallNavArea();
	if (not IsValid(WallArea))
	{
		return;
	}
	const bool bHasOpenBreach = not GetOpenNavIntervals().IsEmpty();
	if (not M_KitObstacleBoxes.IsEmpty() && not bHasOpenBreach)
	{
		// The kit's boxes follow the wall runs and heaps better than a centre line: they block in the wall's area.
		SetKitObstacleArea(WallArea, true);
		UE_LOG(LogWallTankBreach, Verbose, TEXT("RTS_WALL_BREACH nav %s kit boxes=%d area=%s"), *GetNameSafe(GetOwner()),
		       M_KitObstacleBoxes.Num(), *GetNameSafe(WallArea.Get()));
		return;
	}
	SetKitObstacleAreaOutsideTankLanes(WallArea);
	TakeOverWallNav();
	for (URTSNavCollisionEnvObstacle* const NavBlock : M_NavBlocks)
	{
		if (IsValid(NavBlock))
		{
			NavBlock->DestroyComponent();
		}
	}
	M_NavBlocks.Reset();
	const FWallHoleField& Field = M_HoleDamage->GetField();
	const TArray<FVector2f> OpenIntervals = GetOpenNavIntervals();
	for (int32 SegmentIndex = 0; SegmentIndex < Field.GetSegmentCount(); ++SegmentIndex)
	{
		FVector2D Start;
		FVector2D End;
		float UStart = 0.f;
		float UEnd = 0.f;
		Field.GetSegment(SegmentIndex, Start, End, UStart, UEnd);
		// The blocked runs of this segment: its length minus the open stretches (sorted, merged).
		float BlockFrom = UStart;
		for (const FVector2f& Open : OpenIntervals)
		{
			if (Open.Y <= BlockFrom || Open.X >= UEnd)
			{
				continue;
			}
			AddNavBlock(SegmentIndex, BlockFrom, FMath::Max(BlockFrom, Open.X), WallArea);
			BlockFrom = FMath::Max(BlockFrom, Open.Y);
		}
		AddNavBlock(SegmentIndex, BlockFrom, UEnd, WallArea);
	}
	UE_LOG(LogWallTankBreach, Display, TEXT("RTS_WALL_BREACH nav %s area=%s open stretches=%d blocks=%d"),
	       *GetNameSafe(GetOwner()), *GetNameSafe(WallArea.Get()), OpenIntervals.Num(), M_NavBlocks.Num());
}

void UWallTankBreachComponent::TakeOverWallNav()
{
	if (bM_HasTakenOverNav)
	{
		return;
	}
	bM_HasTakenOverNav = true;
	TInlineComponentArray<UNavModifierComponent*> OwnModifiers(GetOwner());
	for (UNavModifierComponent* const Modifier : OwnModifiers)
	{
		Modifier->SetNavigationRelevancy(false);
		Modifier->RefreshNavigationModifiers();
	}
	TInlineComponentArray<UChildActorComponent*> ChildActors(GetOwner());
	for (const UChildActorComponent* const ChildActor : ChildActors)
	{
		const AActor* const Child = IsValid(ChildActor) ? ChildActor->GetChildActor() : nullptr;
		if (not IsValid(Child))
		{
			continue;
		}
		TInlineComponentArray<UNavModifierComponent*> ChildModifiers(Child);
		for (UNavModifierComponent* const Modifier : ChildModifiers)
		{
			Modifier->SetNavigationRelevancy(false);
			Modifier->RefreshNavigationModifiers();
		}
	}
}

bool UWallTankBreachComponent::GetDoNavFiltersKnowWallSides()
{
	const URTSNavQueryFilterSettings* const FilterSettings = URTSNavQueryFilterSettings::Get();
	if (FilterSettings == nullptr)
	{
		return false;
	}
	for (const ERTSNavAgents Agent : {ERTSNavAgents::Character, ERTSNavAgents::LightTank, ERTSNavAgents::MediumTank,
	                                  ERTSNavAgents::HeavyTank})
	{
		const TSubclassOf<UNavigationQueryFilter> FilterClass = FilterSettings->ResolveFilterClassForAgent(Agent);
		if (not FilterClass || not FilterClass->IsChildOf(URTSWallGateNavQueryFilter::StaticClass()))
		{
			return false;
		}
	}
	return true;
}

TSubclassOf<UNavArea> UWallTankBreachComponent::GetWallNavArea()
{
	if (not GetDoNavFiltersKnowWallSides())
	{
		// No filter excludes the breachable areas: the intact wall blocks everyone (no tank plans through it).
		if (not bM_HasWarnedAboutNavFilters)
		{
			bM_HasWarnedAboutNavFilters = true;
			RTSFunctionLibrary::ReportError(TEXT("UWallTankBreachComponent: the RTS_NavFilter_* Blueprints do not derive "
				"from URTSWallGateNavQueryFilter; Rus walls block every unit (no tank plans a breach)."));
		}
		return UNavArea_Null::StaticClass();
	}
	if (Settings.bAnyVehicleBreaks)
	{
		return UNavArea_BreachableAnyVehicle::StaticClass();
	}
	if (Settings.bAnySideBreaks)
	{
		return UNavArea_BreachableAnySideMediumHeavy::StaticClass();
	}
	if (Settings.bHeavyTanksOnly)
	{
		return UNavArea_BreachableWallHeavyOnly::StaticClass();
	}
	uint8 WallOwner = 0;
	const bool bIsPlayerWall = TryGetWallOwningPlayer(WallOwner) && WallOwner == RTSWallSides::HumanPlayerIndex;
	return bIsPlayerWall
		       ? UNavArea_BreachableWallPlayerOwned::StaticClass()
		       : UNavArea_BreachableWallEnemyOwned::StaticClass();
}

TArray<FVector2f> UWallTankBreachComponent::GetOpenNavIntervals() const
{
	return GetOpenIntervals(Settings.NavGapInset);
}

TArray<FVector2f> UWallTankBreachComponent::GetOpenIntervals(const float Inset) const
{
	TArray<FVector2f> Intervals;
	if (not GetIsValidHoleDamage())
	{
		return Intervals;
	}
	for (int32 BreachIndex = 0; BreachIndex < M_HoleDamage->GetBreachCount(); ++BreachIndex)
	{
		const FWallHoleFieldBreach* const Breach = M_HoleDamage->GetBreach(BreachIndex);
		// Open once the top is fully down (a crossing marks that with TopCollapse 1).
		if (Breach == nullptr || Breach->TopCollapse < 1.f)
		{
			continue;
		}
		const float From = Breach->UMin + Inset;
		const float To = Breach->UMax - Inset;
		if (To > From)
		{
			Intervals.Add(FVector2f(From, To));
		}
	}
	Intervals.Sort([](const FVector2f& A, const FVector2f& B) { return A.X < B.X; });
	// Overlapping breaches (two tanks side by side) make one gap.
	TArray<FVector2f> Merged;
	for (const FVector2f& Interval : Intervals)
	{
		if (not Merged.IsEmpty() && Interval.X <= Merged.Last().Y)
		{
			Merged.Last().Y = FMath::Max(Merged.Last().Y, Interval.Y);
			continue;
		}
		Merged.Add(Interval);
	}
	return Merged;
}

void UWallTankBreachComponent::AddNavBlock(const int32 SegmentIndex, const float UFrom, const float UTo,
                                           const TSubclassOf<UNavArea> BlockedArea)
{
	if (UTo - UFrom < WallTankBreachConstants::MinNavBlockLength)
	{
		return;
	}
	const FWallHoleField& Field = M_HoleDamage->GetField();
	FVector2D Start;
	FVector2D End;
	float UStart = 0.f;
	float UEnd = 0.f;
	Field.GetSegment(SegmentIndex, Start, End, UStart, UEnd);
	const FVector2D Direction = (End - Start).GetSafeNormal();
	const FVector2D MeshCentre2D = Start + Direction * ((UFrom + UTo) * 0.5f - UStart);
	const FTransform MeshTransform = M_WallMesh->GetComponentTransform();
	const FVector WorldCentre = MeshTransform.TransformPosition(FVector(MeshCentre2D.X, MeshCentre2D.Y, Field.GetZMin()));
	// Scaled along the run: one mesh-space cm along the segment is this long in the world.
	const FVector WorldAlong = MeshTransform.TransformVector(FVector(Direction.X, Direction.Y, 0.0));
	const FVector LocalExtent((UTo - UFrom) * 0.5f * WorldAlong.Size(), GetNavBlockHalfDepth(),
	                          Settings.NavBlockHalfHeight);
	URTSNavCollisionEnvObstacle* const NavBlock = NewObject<URTSNavCollisionEnvObstacle>(GetOwner());
	NavBlock->SetAreaClass(BlockedArea);
	NavBlock->SetKeepExactBoxWhenOwnerMoves(true);
	NavBlock->RegisterComponent();
	NavBlock->SetUpNavModifierVolume(WorldCentre, LocalExtent,
	                                 FRotationMatrix::MakeFromX(WorldAlong.GetSafeNormal()).ToQuat());
	M_NavBlocks.Add(NavBlock);
}

bool UWallTankBreachComponent::GetIsValidHoleDamage() const
{
	if (IsValid(M_HoleDamage))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_HoleDamage", "GetIsValidHoleDamage",
	                                                             GetOwner());
	return false;
}

bool UWallTankBreachComponent::GetIsValidWallMesh() const
{
	if (IsValid(M_WallMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_WallMesh", "GetIsValidWallMesh", GetOwner());
	return false;
}
