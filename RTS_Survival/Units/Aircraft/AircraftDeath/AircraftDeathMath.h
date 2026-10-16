// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "AircraftDeathTypes.h"

/**
 * @brief The flight of a shot-down aircraft and the choices made when it is hit, kept free of actors so the
 * automation tests (RTS.Aircraft.Death) can check them; UAircraftDeathComponent drives the aircraft with them.
 */
namespace FAircraftDeathMath
{
	// Folder of the pieces built by RTS.Aircraft.BuildCrashCollections; one collection per set of aircraft meshes.
	inline const TCHAR* const CrashCollectionFolder = TEXT("/Game/RTS_Survival/Blueprints/Aircraft/CrashCollections");
	inline const TCHAR* const CrashCollectionPrefix = TEXT("GC_Crash_");
	// Propeller bones of the aircraft skeletons start with this; the wing flaps are "B_WingRotor" and are no engines.
	inline const TCHAR* const EngineBonePrefix = TEXT("B_Rotor");

	/** @return The fire origin to use: a setting other than Automatic wins, else a single engine is in the nose. */
	RTS_SURVIVAL_API EAircraftFireOrigin ResolveFireOrigin(EAircraftFireOrigin Setting, int32 EngineCount);

	/** @return The side of the aircraft a point in mesh space lies on; +Y is the right wing. */
	RTS_SURVIVAL_API EAircraftDamagedSide GetSideOfLocalPoint(const FVector& LocalPoint);

	/**
	 * @brief Picks the wing engine that catches fire; the engine nearest the centre line is never picked when there
	 * are engines further out, a hit wing engine sits on the wing.
	 * @param EngineLocalLocations Engine (propeller bone) locations in mesh space.
	 * @param RandomSeed Seed so a given aircraft and test always pick the same engine.
	 * @return Index into EngineLocalLocations, or INDEX_NONE without engines.
	 */
	RTS_SURVIVAL_API int32 PickWingEngineIndex(const TArray<FVector>& EngineLocalLocations, int32 RandomSeed);

	/** @return The nose engine: the engine furthest forward (+X), or INDEX_NONE without engines. */
	RTS_SURVIVAL_API int32 PickNoseEngineIndex(const TArray<FVector>& EngineLocalLocations);

	/**
	 * @brief The speed the wreck starts falling with: what the aircraft flew with, but never slower than the
	 * minimum, so an aircraft hit while hovering still drops forward instead of straight down.
	 * @param CurrentVelocity Velocity of the aircraft when it was hit.
	 * @param Forward Nose direction of the aircraft.
	 * @param MinCrashSpeed Lowest speed of the fall.
	 * @return Initial velocity of the wreck.
	 */
	RTS_SURVIVAL_API FVector GetInitialCrashVelocity(const FVector& CurrentVelocity, const FVector& Forward,
	                                                 float MinCrashSpeed);

	/** @return Degrees below the horizon the velocity points; negative while climbing. */
	RTS_SURVIVAL_API float GetDiveDegrees(const FVector& Velocity);

	/**
	 * @brief Advances the falling wreck by one step: the nose drops into a dive, gravity and drag act on the
	 * speed it had, a burning wing rolls the wreck over and turns it into a spiral, a nose fire only rocks it.
	 * @param State The wreck; updated in place.
	 * @param Settings How the aircraft falls.
	 * @param FireOrigin Where it burns (resolved, never Automatic).
	 * @param DeltaSeconds Step length.
	 * @param GravityZ The world's gravity (negative).
	 */
	RTS_SURVIVAL_API void StepCrashFlight(FAircraftCrashFlightState& State, const FAircraftDeathSettings& Settings,
	                                      EAircraftFireOrigin FireOrigin, float DeltaSeconds, float GravityZ);

	/** @return Rotation of the wreck: the nose along the velocity, rolled by the state's roll. */
	RTS_SURVIVAL_API FRotator GetCrashRotation(const FAircraftCrashFlightState& State, const FRotator& FallbackRotation);

	/**
	 * @return Asset name of the crash collection for an aircraft drawn with these meshes, e.g. GC_Crash_SM_PE2_Frame;
	 * the static frame names it when there is one, the builder and the aircraft agree on it by this function.
	 * @param StaticMeshNames Asset names of the aircraft's static meshes.
	 * @param SkeletalMeshNames Asset names of the aircraft's skeletal meshes.
	 */
	RTS_SURVIVAL_API FString GetCrashCollectionAssetName(const TArray<FString>& StaticMeshNames,
	                                                    const TArray<FString>& SkeletalMeshNames);

	/** @return Object path of the crash collection with this asset name. */
	RTS_SURVIVAL_API FString GetCrashCollectionObjectPath(const FString& AssetName);
}
