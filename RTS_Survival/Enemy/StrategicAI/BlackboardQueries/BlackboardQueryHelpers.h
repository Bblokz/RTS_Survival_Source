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

	bool HasAtLeastXAircraft(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinAircraft,
		EAircraftSubtype AircraftType);

	bool HasAtLeastXSquads(
		const FStrategicAIBlackboard& Blackboard,
		const int32 MinSquads,
		ESquadSubtype SquadType);
}
