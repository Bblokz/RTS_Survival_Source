// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftDeathMath.h"

namespace AircraftDeathMathPrivate
{
	// A nose fire stalls the engine that pulls the aircraft, so the nose drops faster than with a wing fire.
	constexpr float NoseFireNoseDropMultiplier = 1.5f;
	// How quickly a burning wing reaches its full roll speed.
	constexpr float WingFireRollBuildUpSpeed = 1.2f;
	// Rocking frequency of a nose fire, in radians per second.
	constexpr float NoseFireWobbleFrequency = 2.4f;
	// Engines closer than this to the centre line count as nose engines when choosing a wing engine.
	constexpr float CentreLineTolerance = 30.f;
	constexpr float MinimumSpeedForDirection = 1.f;
}

EAircraftFireOrigin FAircraftDeathMath::ResolveFireOrigin(const EAircraftFireOrigin Setting, const int32 EngineCount)
{
	if (Setting != EAircraftFireOrigin::Automatic)
	{
		return Setting;
	}
	return EngineCount > 1 ? EAircraftFireOrigin::WingEngine : EAircraftFireOrigin::NoseEngine;
}

EAircraftDamagedSide FAircraftDeathMath::GetSideOfLocalPoint(const FVector& LocalPoint)
{
	if (FMath::Abs(LocalPoint.Y) <= AircraftDeathMathPrivate::CentreLineTolerance)
	{
		return EAircraftDamagedSide::None;
	}
	return LocalPoint.Y > 0.f ? EAircraftDamagedSide::Right : EAircraftDamagedSide::Left;
}

int32 FAircraftDeathMath::PickWingEngineIndex(const TArray<FVector>& EngineLocalLocations, const int32 RandomSeed)
{
	TArray<int32> WingEngines;
	for (int32 EngineIndex = 0; EngineIndex < EngineLocalLocations.Num(); ++EngineIndex)
	{
		if (GetSideOfLocalPoint(EngineLocalLocations[EngineIndex]) != EAircraftDamagedSide::None)
		{
			WingEngines.Add(EngineIndex);
		}
	}
	if (WingEngines.IsEmpty())
	{
		return EngineLocalLocations.IsEmpty() ? INDEX_NONE : 0;
	}
	const FRandomStream Stream(RandomSeed);
	return WingEngines[Stream.RandRange(0, WingEngines.Num() - 1)];
}

int32 FAircraftDeathMath::PickNoseEngineIndex(const TArray<FVector>& EngineLocalLocations)
{
	int32 NoseIndex = INDEX_NONE;
	for (int32 EngineIndex = 0; EngineIndex < EngineLocalLocations.Num(); ++EngineIndex)
	{
		if (NoseIndex == INDEX_NONE || EngineLocalLocations[EngineIndex].X > EngineLocalLocations[NoseIndex].X)
		{
			NoseIndex = EngineIndex;
		}
	}
	return NoseIndex;
}

FVector FAircraftDeathMath::GetInitialCrashVelocity(const FVector& CurrentVelocity, const FVector& Forward,
                                                    const float MinCrashSpeed)
{
	if (CurrentVelocity.Size() >= MinCrashSpeed)
	{
		return CurrentVelocity;
	}
	const FVector LevelForward = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
	const FVector Direction = LevelForward.IsNearlyZero() ? FVector::ForwardVector : LevelForward;
	// Keep the sink or climb it had; only the forward speed is topped up.
	return Direction * MinCrashSpeed + FVector(0.f, 0.f, CurrentVelocity.Z);
}

float FAircraftDeathMath::GetDiveDegrees(const FVector& Velocity)
{
	const float HorizontalSpeed = Velocity.Size2D();
	return FMath::RadiansToDegrees(FMath::Atan2(-Velocity.Z, HorizontalSpeed));
}

void FAircraftDeathMath::StepCrashFlight(FAircraftCrashFlightState& State, const FAircraftDeathSettings& Settings,
                                         const EAircraftFireOrigin FireOrigin, const float DeltaSeconds,
                                         const float GravityZ)
{
	using namespace AircraftDeathMathPrivate;
	if (DeltaSeconds <= 0.f)
	{
		return;
	}
	const bool bIsNoseFire = FireOrigin == EAircraftFireOrigin::NoseEngine;
	const float Speed = State.Velocity.Size();
	FVector HorizontalDirection = FVector(State.Velocity.X, State.Velocity.Y, 0.f).GetSafeNormal();
	if (HorizontalDirection.IsNearlyZero())
	{
		HorizontalDirection = FVector::ForwardVector;
	}

	// A burning wing loses lift: the wreck turns towards it while it rolls over.
	const float SideSign = State.DamagedSide == EAircraftDamagedSide::Right
		                       ? 1.f
		                       : (State.DamagedSide == EAircraftDamagedSide::Left ? -1.f : 0.f);
	if (not bIsNoseFire && SideSign != 0.f)
	{
		const float TurnDegrees = SideSign * Settings.WingFireTurnDegreesPerSecond * DeltaSeconds;
		HorizontalDirection = HorizontalDirection.RotateAngleAxis(TurnDegrees, FVector::UpVector);
	}

	// The nose drops into a dive while the speed it had is kept.
	const float NoseDropRate = Settings.NoseDropDegreesPerSecond * (bIsNoseFire ? NoseFireNoseDropMultiplier : 1.f);
	const float CurrentDive = GetDiveDegrees(State.Velocity);
	const float NewDive = CurrentDive >= Settings.MaxDiveDegrees
		                      ? CurrentDive
		                      : FMath::Min(CurrentDive + NoseDropRate * DeltaSeconds, Settings.MaxDiveDegrees);
	const float NewDiveRadians = FMath::DegreesToRadians(NewDive);
	FVector NewVelocity = HorizontalDirection * (Speed * FMath::Cos(NewDiveRadians));
	NewVelocity.Z = -Speed * FMath::Sin(NewDiveRadians);

	NewVelocity.Z += GravityZ * Settings.GravityScale * DeltaSeconds;
	NewVelocity *= FMath::Max(0.f, 1.f - Settings.DragPerSecond * DeltaSeconds);
	if (NewVelocity.Size() < MinimumSpeedForDirection)
	{
		NewVelocity = HorizontalDirection * MinimumSpeedForDirection;
	}

	if (bIsNoseFire || SideSign == 0.f)
	{
		State.RollRateDegreesPerSecond = 0.f;
		State.RollDegrees = Settings.NoseFireWobbleDegrees * FMath::Sin(State.SecondsFalling * NoseFireWobbleFrequency);
	}
	else
	{
		State.RollRateDegreesPerSecond = FMath::FInterpTo(State.RollRateDegreesPerSecond,
		                                                  SideSign * Settings.WingFireRollDegreesPerSecond,
		                                                  DeltaSeconds, WingFireRollBuildUpSpeed);
		State.RollDegrees = FMath::UnwindDegrees(State.RollDegrees + State.RollRateDegreesPerSecond * DeltaSeconds);
	}

	State.Location += (State.Velocity + NewVelocity) * 0.5f * DeltaSeconds;
	State.Velocity = NewVelocity;
	State.SecondsFalling += DeltaSeconds;
}

FRotator FAircraftDeathMath::GetCrashRotation(const FAircraftCrashFlightState& State, const FRotator& FallbackRotation)
{
	if (State.Velocity.SizeSquared() < FMath::Square(AircraftDeathMathPrivate::MinimumSpeedForDirection))
	{
		return FallbackRotation;
	}
	const FVector Forward = State.Velocity.GetSafeNormal();
	const FQuat NoseAlongVelocity = FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat();
	// A positive quaternion angle about the nose lifts the right wing; positive roll in the state drops it.
	const FQuat Roll(Forward, FMath::DegreesToRadians(-State.RollDegrees));
	return (Roll * NoseAlongVelocity).Rotator();
}

FString FAircraftDeathMath::GetCrashCollectionAssetName(const TArray<FString>& StaticMeshNames,
                                                        const TArray<FString>& SkeletalMeshNames)
{
	TArray<FString> SortedStatic = StaticMeshNames;
	SortedStatic.Sort();
	TArray<FString> SortedSkeletal = SkeletalMeshNames;
	SortedSkeletal.Sort();
	const FString& NamingMesh = SortedStatic.Num() > 0
		                            ? SortedStatic[0]
		                            : (SortedSkeletal.Num() > 0 ? SortedSkeletal[0] : FString());
	if (NamingMesh.IsEmpty())
	{
		return FString();
	}
	return FString(CrashCollectionPrefix) + NamingMesh;
}

FString FAircraftDeathMath::GetCrashCollectionObjectPath(const FString& AssetName)
{
	if (AssetName.IsEmpty())
	{
		return FString();
	}
	return FString::Printf(TEXT("%s/%s.%s"), CrashCollectionFolder, *AssetName, *AssetName);
}
