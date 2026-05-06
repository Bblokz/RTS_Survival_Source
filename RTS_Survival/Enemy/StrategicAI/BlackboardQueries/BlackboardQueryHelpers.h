#pragma once

enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
enum class EAircraftSubtype : uint8;
struct FStrategicAIBlackboard;

namespace BlackboardQueries
{

	
	bool HasAtLeastXTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinTanks,
		ETankSubtype TankType);
	
	bool HasAtLeastAnyXTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinTanks);

	bool HasAtLeastXArmoredCars(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinArmoredCars);

	bool HasAtLeastXLightTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinLightTanks);

	bool HasAtLeastXMediumTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinMediumTanks);

	bool HasAtLeastXHeavyTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinHeavyTanks);

	bool HasAtLeastXPlayerTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerTanks);

	bool HasAtLeastXPlayerArmoredCars(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerArmoredCars);

	bool HasAtLeastXPlayerLightTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerLightTanks);

	bool HasAtLeastXPlayerMediumTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerMediumTanks);

	bool HasAtLeastXPlayerHeavyTanks(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerHeavyTanks);

	bool HasAtLeastXPlayerSquads(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerSquads);

	bool HasAtLeastXPlayerNomadicVehicles(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinPlayerNomadicVehicles);

	bool HasAtLeastXAircraft(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinAircraft,
		EAircraftSubtype AircraftType);

	bool HasAtLeastXSquads(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinSquads,
		ESquadSubtype SquadType);

	bool HasValidPlayerHQLocation(
		const FStrategicAIBlackboard& Blackboard);

	bool HasValidPlayerResourceBuildingLocations(
		const FStrategicAIBlackboard& Blackboard);

	bool HasValidPlayerHeavyTankFLankLocations(
		const FStrategicAIBlackboard& Blackboard);

	TArray<FVector> GetRandomLocationsOfIdleUnits(
		const FStrategicAIBlackboard& Blackboard,
		const int32 PreferredAmount = 3);
}
