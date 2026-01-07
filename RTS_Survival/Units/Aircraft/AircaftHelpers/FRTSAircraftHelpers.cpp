#include "FRTSAircraftHelpers.h"

#include "Math/UnitConversion.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AircraftCommandsData/AircraftCommandsData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void FRTSAircraftHelpers::AircraftDebug(const FString& Message, const FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color, 10);
	}
}

void FRTSAircraftHelpers::AircraftDebugAtLocation(const UObject* WorldContext, const FString& Message,
                                                  const FVector& Location, const FColor Color, const float Time)
{
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		if (not IsValid(WorldContext))
		{
			return;
		}
		if (UWorld* World = WorldContext->GetWorld())
		{
			DrawDebugString(World,
			                Location,
			                Message,
			                nullptr,
			                Color,
			                Time,
			                false
			);
		}
	}
}

void FRTSAircraftHelpers::BombDebug(const FString& Message, const FColor Color)
{
	if constexpr (DeveloperSettings::Debugging::GBombBay_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color, 10);
	}
}

void FRTSAircraftHelpers::AircraftSphereDebug(const UObject* WorldContext, const FVector& Location, const FColor Color,
                                              const float Radius, const float Time)
{
	if constexpr (DeveloperSettings::Debugging::GAircraftMovement_Compile_DebugSymbols)
	{
		if (not WorldContext)
		{
			return;
		}

		if (UWorld* World = WorldContext->GetWorld())
		{
			DrawDebugSphere(World,
			                Location,
			                Radius, 20,
			                Color,
			                false,
			                Time
			);
		}
	}
}

FString FRTSAircraftHelpers::GetAircraftMovementStateString(const EAircraftMovementState MovementState)
{
	switch (MovementState)
	{
	case EAircraftMovementState::None: return TEXT("None");
	case EAircraftMovementState::Idle: return TEXT("Idle");
	case EAircraftMovementState::MovingTo: return TEXT("MovingTo");
	case EAircraftMovementState::AttackMove: return TEXT("AttackMove");
	default: return TEXT("Unknown");
	}
}

FString FRTSAircraftHelpers::GetAircraftLandingStateString(const EAircraftLandingState LandingState)
{
	switch (LandingState)
	{
	case EAircraftLandingState::None:
		return TEXT("None");
	case EAircraftLandingState::VerticalTakeOff:
		return TEXT("Vertical Take Off");
	case EAircraftLandingState::Airborne:
		return TEXT("Airborne");
	case EAircraftLandingState::VerticalLanding:
		return TEXT("vertical landing");
	case EAircraftLandingState::Landed:
		return TEXT("Landed");
	case EAircraftLandingState::WaitForBayToOpen:
		return TEXT("WaitForBayToOpen");
	default:
		return TEXT("Unknown");
	}
}

FString FRTSAircraftHelpers::GetAircraftPostLifOffString(const EPostLiftOffAction PostLifOffState)
{
	switch (PostLifOffState)
	{
	case EPostLiftOffAction::Attack:
		return TEXT("Attack");
	case EPostLiftOffAction::Idle:
		return TEXT("Idle");
	case EPostLiftOffAction::Move:
		return TEXT("Move");
	case EPostLiftOffAction::ChangeOwner:
		return TEXT("ChangeOwner");
	}
	return TEXT("Unknown");
}
