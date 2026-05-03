#pragma once

#include "CoreMinimal.h"

namespace EnemyDirectControlConstants
{
	inline constexpr float DirectControlTickRateSeconds = 3.f;

	inline constexpr float DebugDrawDurationSeconds = 3.25f;
	inline constexpr float DebugTextOffsetZ = 260.f;
	inline constexpr float DebugSphereRadius = 80.f;
	inline constexpr int32 DebugSphereSegments = 12;
	inline constexpr float DebugSphereThickness = 2.f;
	inline constexpr float DebugSphereOffsetZ = 30.f;

	namespace Debugging
	{
		inline constexpr bool DrawRegisteredUnitLabel = true;
		inline constexpr bool DrawRegisteredUnitSphere = true;
		inline constexpr bool ReportRegisterDeregister = true;

		inline constexpr bool DebugRemoveDirectControlUnitFromFormations = true;

		const FColor RemoveFromFormationColor = FColor::Red;
		const float RemoveFromFormationZOffset = 500.f;
		const float RemoveFromLocationDebugTime = 10.f;
		const FColor RetreatGroupColor = FColor::Purple;
	}
}
