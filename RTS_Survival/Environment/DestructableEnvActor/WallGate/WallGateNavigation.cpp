// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallGateNavigation.h"

#include "NavigationData.h"
#include "NavAreas/NavArea_Null.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "WallGate.h"

namespace WallGateNavigationConstants
{
	constexpr uint8 HumanPlayerIndex = RTSWallSides::HumanPlayerIndex;
	const FColor PlayerSideAreaColour = FColor(40, 160, 255);
	const FColor EnemySideAreaColour = FColor(255, 120, 40);
	const FColor Player3AreaColour = FColor(200, 60, 200);
	const FColor Player4AreaColour = FColor(60, 200, 90);
	const FColor BreachableWallColour = FColor(150, 90, 60);
	// A tank breaks a wall only when the way round is more than about this far (cm of path) longer: a gap or gate right
	// beside it wins, a long way round does not.
	constexpr float BreachableWallEnteringCost = 300.f;
	// Berlin ruin walls (heavy tanks only): a heavy tank drives through brick and rubble as through open ground. A ruin is
	// a wall box plus heap boxes, each entered separately, so any entering cost would add up into a detour.
	constexpr float HeavyOnlyRuinEnteringCost = 0.f;
	// ... but each cm through a ruin costs a little more than open ground (counted over the area grown by the 2 m heavy
	// agent radius on both sides): a way round a metre or so longer still wins; at 1.15 a corner ruin (B5) was already
	// avoided, which the owner asked against.
	constexpr float HeavyOnlyRuinTankCost = 1.05f;
	// Cost per cm through a breachable wall for queries without the RTS filter: an obstacle's (NavArea_Obstacle).
	constexpr float BreachableWallUnfilteredCost = 1000000.f;
	constexpr float BreachableWallTankCost = 1.f;
	// Wire fences (any vehicle): a vehicle flattens one as if it were open ground; a gate or gap within a metre or so
	// still wins. Fences stand in chained runs, so the entering cost stays small (an oblique path enters several).
	constexpr float AnyVehicleFenceEnteringCost = 100.f;
}

namespace
{
	const AActor* GetQuerierActor(const UObject* Querier)
	{
		if (const AController* const QuerierController = Cast<AController>(Querier))
		{
			return QuerierController->GetPawn();
		}
		return Cast<AActor>(Querier);
	}

}

UNavArea_BreachableWallPlayerOwned::UNavArea_BreachableWallPlayerOwned(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::BreachableWallColour;
	DefaultCost = WallGateNavigationConstants::BreachableWallUnfilteredCost;
	FixedAreaEnteringCost = WallGateNavigationConstants::BreachableWallEnteringCost;
}

UNavArea_BreachableWallHeavyOnly::UNavArea_BreachableWallHeavyOnly(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::BreachableWallColour;
	DefaultCost = WallGateNavigationConstants::BreachableWallUnfilteredCost;
	FixedAreaEnteringCost = WallGateNavigationConstants::BreachableWallEnteringCost;
}

UNavArea_BreachableWallEnemyOwned::UNavArea_BreachableWallEnemyOwned(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::BreachableWallColour;
	DefaultCost = WallGateNavigationConstants::BreachableWallUnfilteredCost;
	FixedAreaEnteringCost = WallGateNavigationConstants::BreachableWallEnteringCost;
}

UNavArea_BreachableAnyVehicle::UNavArea_BreachableAnyVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::BreachableWallColour;
	// Plain ground for queries without the RTS filter (convoy routes, planners), as the fence's old BlockCharLight area
	// was: a fence never seals off a route; only infantry with the RTS filter go round.
	DefaultCost = 1.f;
	FixedAreaEnteringCost = WallGateNavigationConstants::AnyVehicleFenceEnteringCost;
}

UNavArea_BreachableAnySideMediumHeavy::UNavArea_BreachableAnySideMediumHeavy(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::BreachableWallColour;
	DefaultCost = 1.f;
	FixedAreaEnteringCost = WallGateNavigationConstants::BreachableWallEnteringCost;
}

UNavArea_WallGatePlayerSide::UNavArea_WallGatePlayerSide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::PlayerSideAreaColour;
}

UNavArea_WallGateEnemySide::UNavArea_WallGateEnemySide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::EnemySideAreaColour;
}

UNavArea_WallGatePlayer3::UNavArea_WallGatePlayer3(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::Player3AreaColour;
}

UNavArea_WallGatePlayer4::UNavArea_WallGatePlayer4(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::Player4AreaColour;
}

TSubclassOf<UNavArea> RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(const int32 Player)
{
	constexpr int32 ThirdPlayer = 3;
	constexpr int32 FourthPlayer = 4;
	static_assert(RTSPlayers::MaxPlayers == FourthPlayer, "One closed gate area class per player.");
	switch (FRTSPlayers::NormalizePlayer(Player))
	{
	case RTSPlayers::HumanPlayer:
		return UNavArea_WallGatePlayerSide::StaticClass();
	case RTSPlayers::FirstAIPlayer:
		return UNavArea_WallGateEnemySide::StaticClass();
	case ThirdPlayer:
		return UNavArea_WallGatePlayer3::StaticClass();
	case FourthPlayer:
		return UNavArea_WallGatePlayer4::StaticClass();
	default:
		// The neutral owner: allied to nobody, so nobody plans through.
		return nullptr;
	}
}

URTSWallGateNavQueryFilter::URTSWallGateNavQueryFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Units of both sides share these filters, so the gate exclusion is decided per querier, not cached per class.
	bInstantiateForQuerier = true;
}

void URTSWallGateNavQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
                                                  FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);
	uint8 OwningPlayer = 0;
	const bool bHasKnownSide = TryGetQuerierOwningPlayer(Querier, OwningPlayer);
	const bool bIsHumanPlayer = OwningPlayer == WallGateNavigationConstants::HumanPlayerIndex;
	ExcludeBreachableWalls(NavData, Querier, bHasKnownSide, bIsHumanPlayer, Filter);
	ExcludeClosedGatesOfNonAllies(NavData, bHasKnownSide, OwningPlayer, Filter);
}

void URTSWallGateNavQueryFilter::ExcludeClosedGatesOfNonAllies(const ANavigationData& NavData,
                                                               const bool bHasKnownSide, const uint8 QuerierPlayer,
                                                               FNavigationQueryFilter& Filter)
{
	for (int32 GatePlayer = RTSPlayers::HumanPlayer; GatePlayer <= RTSPlayers::MaxPlayers; ++GatePlayer)
	{
		// Unknown side: no closed gate lets it through. Neutral units are allied to nobody, so neither does any.
		if (bHasKnownSide && FRTSPlayers::AreAllied(GatePlayer, QuerierPlayer))
		{
			continue;
		}
		ExcludeArea(NavData, RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(GatePlayer), Filter);
	}
}

void URTSWallGateNavQueryFilter::ExcludeBreachableWalls(const ANavigationData& NavData, const UObject* Querier,
                                                        const bool bHasKnownSide, const bool bIsHumanPlayer,
                                                        FNavigationQueryFilter& Filter)
{
	// Only the human player's tanks break walls: mediums and heavies the AI side's walls, heavies alone the ruin walls.
	ExcludeArea(NavData, UNavArea_BreachableWallPlayerOwned::StaticClass(), Filter);
	const bool bIsPlayerUnit = bHasKnownSide && bIsHumanPlayer;
	if (bIsPlayerUnit && GetIsQuerierMediumOrHeavyTank(Querier))
	{
		OpenBreachableArea(NavData, UNavArea_BreachableWallEnemyOwned::StaticClass(), Filter,
		                   WallGateNavigationConstants::BreachableWallTankCost,
		                   WallGateNavigationConstants::BreachableWallEnteringCost);
	}
	else
	{
		ExcludeArea(NavData, UNavArea_BreachableWallEnemyOwned::StaticClass(), Filter);
	}
	if (bIsPlayerUnit && GetIsQuerierHeavyTank(Querier))
	{
		OpenBreachableArea(NavData, UNavArea_BreachableWallHeavyOnly::StaticClass(), Filter,
		                   WallGateNavigationConstants::HeavyOnlyRuinTankCost,
		                   WallGateNavigationConstants::HeavyOnlyRuinEnteringCost);
	}
	else
	{
		ExcludeArea(NavData, UNavArea_BreachableWallHeavyOnly::StaticClass(), Filter);
	}
	// Wire fences: every vehicle of every side drives through; infantry and unknown queriers go round.
	if (bHasKnownSide && GetIsQuerierVehicle(Querier))
	{
		OpenBreachableArea(NavData, UNavArea_BreachableAnyVehicle::StaticClass(), Filter,
		                   WallGateNavigationConstants::BreachableWallTankCost,
		                   WallGateNavigationConstants::AnyVehicleFenceEnteringCost);
	}
	else
	{
		ExcludeArea(NavData, UNavArea_BreachableAnyVehicle::StaticClass(), Filter);
	}
	// Concrete fences: medium and heavy vehicles of every side drive through; light vehicles and infantry go round.
	if (bHasKnownSide && GetIsQuerierMediumOrHeavyTank(Querier))
	{
		OpenBreachableArea(NavData, UNavArea_BreachableAnySideMediumHeavy::StaticClass(), Filter,
		                   WallGateNavigationConstants::BreachableWallTankCost,
		                   WallGateNavigationConstants::BreachableWallEnteringCost);
	}
	else
	{
		ExcludeArea(NavData, UNavArea_BreachableAnySideMediumHeavy::StaticClass(), Filter);
	}
}

void URTSWallGateNavQueryFilter::OpenBreachableArea(const ANavigationData& NavData, const UClass* AreaClass,
                                                    FNavigationQueryFilter& Filter, const float TankCost,
                                                    const float EnteringCost)
{
	// The area's own cost is an obstacle's (for unfiltered queries); for the tanks that may break it, the wall is plain
	// ground plus the entering cost.
	const int32 AreaId = NavData.GetAreaID(AreaClass);
	if (AreaId < 0 || AreaId > MAX_uint8)
	{
		return;
	}
	Filter.SetAreaCost(static_cast<uint8>(AreaId), TankCost);
	Filter.SetFixedAreaEnteringCost(static_cast<uint8>(AreaId), EnteringCost);
}

bool URTSWallGateNavQueryFilter::GetIsQuerierHeavyTank(const UObject* Querier)
{
	const IRTSNavAgentInterface* const NavAgent = Cast<IRTSNavAgentInterface>(GetQuerierActor(Querier));
	return NavAgent != nullptr && NavAgent->GetRTSNavAgentType() == ERTSNavAgents::HeavyTank;
}

bool URTSWallGateNavQueryFilter::GetIsQuerierVehicle(const UObject* Querier)
{
	const IRTSNavAgentInterface* const NavAgent = Cast<IRTSNavAgentInterface>(GetQuerierActor(Querier));
	if (NavAgent == nullptr)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	return AgentType == ERTSNavAgents::LightTank || AgentType == ERTSNavAgents::MediumTank
		|| AgentType == ERTSNavAgents::HeavyTank;
}

bool URTSWallGateNavQueryFilter::GetIsQuerierMediumOrHeavyTank(const UObject* Querier)
{
	const IRTSNavAgentInterface* const NavAgent = Cast<IRTSNavAgentInterface>(GetQuerierActor(Querier));
	if (NavAgent == nullptr)
	{
		return false;
	}
	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	return AgentType == ERTSNavAgents::MediumTank || AgentType == ERTSNavAgents::HeavyTank;
}

bool URTSWallGateNavQueryFilter::TryGetQuerierOwningPlayer(const UObject* Querier, uint8& OutOwningPlayer)
{
	const AActor* const QuerierActor = GetQuerierActor(Querier);
	if (not IsValid(QuerierActor))
	{
		return false;
	}
	const URTSComponent* const RTSComponent = QuerierActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return false;
	}
	OutOwningPlayer = RTSComponent->GetOwningPlayer();
	return true;
}

void URTSWallGateNavQueryFilter::ExcludeArea(const ANavigationData& NavData, const UClass* AreaClass,
                                             FNavigationQueryFilter& Filter)
{
	const int32 AreaId = NavData.GetAreaID(AreaClass);
	// Not registered with this navigation data: no gate uses the area, nothing to exclude.
	if (AreaId < 0 || AreaId > MAX_uint8)
	{
		return;
	}
	Filter.SetExcludedArea(static_cast<uint8>(AreaId));
}

void URTSClosedBreachableWallsQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
                                                            FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);
	for (const UClass* const AreaClass : {UNavArea_BreachableWallEnemyOwned::StaticClass(),
	                                      UNavArea_BreachableWallPlayerOwned::StaticClass(),
	                                      UNavArea_BreachableWallHeavyOnly::StaticClass()})
	{
		const int32 AreaId = NavData.GetAreaID(AreaClass);
		if (AreaId >= 0 && AreaId <= MAX_uint8)
		{
			Filter.SetExcludedArea(static_cast<uint8>(AreaId));
		}
	}
}

UWallGateNavModifierBase::UWallGateNavModifierBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWallGateNavModifierBase::AddOrientedBox(const FBox& LocalBox, const FTransform& LocalToWorld,
                                              const float WorldPadding) const
{
	const FQuat Rotation = LocalToWorld.GetRotation();
	const FVector WorldCentre = LocalToWorld.TransformPosition(LocalBox.GetCenter());
	const FVector WorldExtent = LocalBox.GetExtent() * LocalToWorld.GetScale3D().GetAbs() + FVector(WorldPadding);
	// The engine expects each box in a frame that is only rotated, with the rotation handed over separately.
	ComponentBounds.Add(FRotatedBox(FBox::BuildAABB(Rotation.UnrotateVector(WorldCentre), WorldExtent), Rotation));
	Bounds += FBox::BuildAABB(FVector::ZeroVector, WorldExtent).TransformBy(FTransform(Rotation, WorldCentre));
}

void UWallGateNavModifierBase::SetEmptyBounds() const
{
	const AActor* const OwnerActor = GetOwner();
	const FVector Location = IsValid(OwnerActor) ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	Bounds = FBox::BuildAABB(Location, FVector::OneVector);
}

UWallGateNavModifier::UWallGateNavModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWallGateNavModifier::CalculateBounds() const
{
	ComponentBounds.Reset();
	Bounds = FBox(ForceInit);
	const AWallGate* const Gate = Cast<AWallGate>(GetOwner());
	FBox LeafSpaceBox;
	FTransform LeafTransform;
	if (not IsValid(Gate) || not Gate->GetDoorwayNavBox(LeafSpaceBox, LeafTransform))
	{
		SetEmptyBounds();
		return;
	}
	// The gate already padded the box in leaf space.
	AddOrientedBox(LeafSpaceBox, LeafTransform, 0.f);
}

UWallGateFrameNavModifier::UWallGateFrameNavModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AreaClass = UNavArea_Null::StaticClass();
}

void UWallGateFrameNavModifier::CalculateBounds() const
{
	ComponentBounds.Reset();
	Bounds = FBox(ForceInit);
	const AWallGate* const Gate = Cast<AWallGate>(GetOwner());
	TArray<FBox> FrameSpaceBoxes;
	FTransform FrameTransform;
	if (not IsValid(Gate) || not Gate->GetFrameNavBoxes(FrameSpaceBoxes, FrameTransform))
	{
		SetEmptyBounds();
		return;
	}
	for (const FBox& FrameSpaceBox : FrameSpaceBoxes)
	{
		AddOrientedBox(FrameSpaceBox, FrameTransform, Gate->GetFrameNavBoxPadding());
	}
}
