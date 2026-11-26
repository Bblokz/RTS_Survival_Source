// Copyright (C) Bas Blokzijl - All rights reserved.

// Orders for units the player already has (e.g. the start force driving up to the base) and small rewards, given by
// mission scripts.

#include "MissionBase.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace MissionUnitOrdersPrivate
{
	constexpr uint8 HumanPlayer = 1;
	// Units fan out across the road in rows of this many at the end of the route.
	constexpr int32 UnitsPerEndRow = 3;

	/** @return Every alive player vehicle and squad controller within Radius (2D) of Centre, each once. */
	TArray<AActor*> GatherPlayerUnits(const UGameUnitManager& UnitManager, const FVector& Centre, const float Radius)
	{
		const float RadiusSquared = FMath::Square(Radius);
		TArray<AActor*> Gathered;
		for (ATankMaster* const Tank : UnitManager.GetPlayerTanks(HumanPlayer))
		{
			if (IsValid(Tank) && FVector::DistSquared2D(Tank->GetActorLocation(), Centre) <= RadiusSquared)
			{
				Gathered.AddUnique(Tank);
			}
		}
		for (const ASquadUnit* const SquadUnit : UnitManager.GetSquadUnitsOfPlayerView(HumanPlayer))
		{
			if (not IsValid(SquadUnit) || FVector::DistSquared2D(SquadUnit->GetActorLocation(), Centre) > RadiusSquared)
			{
				continue;
			}
			ASquadController* const Squad = SquadUnit->GetSquadControllerChecked();
			if (IsValid(Squad))
			{
				Gathered.AddUnique(Squad);
			}
		}
		return Gathered;
	}

	/** @return Where unit number UnitIndex stops: rows of UnitsPerEndRow across the road, later rows further back. */
	FVector GetEndSlot(const FVector& EndPoint, const FVector& RoadDirection, const int32 UnitIndex,
	                   const float Spacing)
	{
		const FVector Across(-RoadDirection.Y, RoadDirection.X, 0.f);
		const int32 Row = UnitIndex / UnitsPerEndRow;
		const int32 Column = UnitIndex % UnitsPerEndRow;
		const float AcrossOffset = (Column - (UnitsPerEndRow - 1) * 0.5f) * Spacing;
		return EndPoint + Across * AcrossOffset - RoadDirection * (Row * Spacing);
	}
}

int32 UMissionBase::MovePlayerUnitsAlongRoute(const FVector GatherCentre, const float GatherRadius,
                                              const TArray<FVector>& Route, const float EndSpacing)
{
	using namespace MissionUnitOrdersPrivate;
	const UGameUnitManager* const UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (Route.IsEmpty() || not IsValid(UnitManager))
	{
		RTSFunctionLibrary::ReportError("MovePlayerUnitsAlongRoute: empty route or no game unit manager. Mission: "
			+ GetName());
		return 0;
	}
	TArray<AActor*> Units = GatherPlayerUnits(*UnitManager, GatherCentre, GatherRadius);
	// The unit nearest the road's start leads, so the column keeps its order and nobody overtakes on the road.
	const FVector RouteStart = Route[0];
	Units.Sort([RouteStart](const AActor& A, const AActor& B)
	{
		return FVector::DistSquared2D(A.GetActorLocation(), RouteStart)
			< FVector::DistSquared2D(B.GetActorLocation(), RouteStart);
	});

	const FVector EndPoint = Route.Last();
	const FVector BeforeEnd = Route.Num() > 1 ? Route[Route.Num() - 2] : GatherCentre;
	const FVector RoadDirection = (EndPoint - BeforeEnd).GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FRotator EndRotation = RoadDirection.Rotation();
	int32 OrderedUnits = 0;
	for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
	{
		ICommands* const Commands = Cast<ICommands>(Units[UnitIndex]);
		if (Commands == nullptr)
		{
			continue;
		}
		for (int32 PointIndex = 0; PointIndex < Route.Num() - 1; ++PointIndex)
		{
			Commands->MoveToLocation(Route[PointIndex], PointIndex == 0, EndRotation, false);
		}
		const FVector EndSlot = GetEndSlot(EndPoint, RoadDirection, UnitIndex, EndSpacing);
		Commands->MoveToLocation(EndSlot, Route.Num() == 1, EndRotation, false);
		++OrderedUnits;
	}
	return OrderedUnits;
}

bool UMissionBase::GetHasPlayerProfileLoaded() const
{
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	return IsValid(PlayerController) && PlayerController->GetHasPlayerProfileLoaded();
}

bool UMissionBase::GivePlayerResources(const ERTSResourceType Resource, const int32 Amount)
{
	UPlayerResourceManager* const Resources = FRTS_Statics::GetPlayerResourceManager(this);
	if (not IsValid(Resources) || Amount <= 0)
	{
		RTSFunctionLibrary::ReportError("GivePlayerResources: no resource manager or no amount. Mission: " + GetName());
		return false;
	}
	const bool bAllAdded = Resources->AddResource(Resource, Amount);
	const FText ResourceName = StaticEnum<ERTSResourceType>()->GetDisplayNameTextByValue(static_cast<int64>(Resource));
	RTSFunctionLibrary::DisplayNotification(FText::Format(
		NSLOCTEXT("MissionBase", "ResourcesArrived", "Supplies arrived: {0} {1}"), FText::AsNumber(Amount),
		ResourceName));
	return bAllAdded;
}
