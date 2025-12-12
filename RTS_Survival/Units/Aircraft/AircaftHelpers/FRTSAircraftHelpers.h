#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

enum class EPostLiftOffAction : uint8;

struct FRTSAircraftHelpers
{
	static void AircraftDebug(const FString& Message, const FColor Color = FColor::White)	;
	static void AircraftDebugAtLocation(const UObject* WorldContext,
	                                const FString& Message,
	                                const FVector& Location,
	                                const FColor Color = FColor::White,
	                                const float Time = 10.f);
	static void BombDebug(const FString& Message, const FColor Color = FColor::White);
	static void AircraftSphereDebug(const UObject* WorldContext,
	                                const FVector& Location,
	                                const FColor Color = FColor::White,
	                                const float Radius = 50.f,
	                                const float Time = 10.f);

	static FString GetAircraftMovementStateString(const EAircraftMovementState MovementState);
	static FString GetAircraftLandingStateString(const EAircraftLandingState LandingState);
	static FString GetAircraftPostLifOffString(const EPostLiftOffAction PostLifOffState);
};
