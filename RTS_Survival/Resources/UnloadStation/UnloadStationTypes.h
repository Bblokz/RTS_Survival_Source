// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"

#include "UnloadStationTypes.generated.h"

class UHarvester;
class UResourceDropOff;
class UStaticMeshComponent;
class USceneComponent;

/** Why a crane unload ended; tells the harvester what to do next. */
UENUM()
enum class EUnloadEndReason : uint8
{
	// Every chunk landed in the bins; the harvester goes back to harvesting.
	CargoEmpty,
	// The bin could not take the whole chunk; the harvester looks for another drop-off with what is left.
	StorageFull,
	// The station packed up or died mid-unload; the cargo still on the harvester goes elsewhere.
	StationGone
};

/** What crediting one chunk of a docked harvester's cargo led to. */
enum class EUnloadChunkResult : uint8
{
	// Credited; the harvester still carries more.
	Delivered,
	// Credited and the harvester is empty; the unload ended (CargoEmpty).
	CargoEmpty,
	// Nothing more goes in: the bin was full or no harvester is docked; the unload ended or never ran.
	Stopped
};

/** Which way along the rail a bay's trolley runs from the bay to the bins. */
UENUM()
enum class EUnloadTrolleyRunDirection : uint8
{
	AlongRail,
	AgainstRail
};

UENUM()
enum class EUnloadTrolleyPhase : uint8
{
	Idle,
	MovingToBay,
	LoweringToPick,
	Grabbing,
	HoistingFromPick,
	MovingToBin,
	LoweringToDrop,
	Releasing,
	HoistingFromDrop,
	ReturningToPark
};

/** One bay a harvester reverses into, in the station's frame (the HQ building mesh's frame, cm). */
USTRUCT(BlueprintType)
struct FUnloadBaySettings
{
	GENERATED_BODY()

	// Where the docked harvester's cargo (hopper) stands; the trolley grabs above it.
	UPROPERTY(EditAnywhere, Category="Unload Bay")
	FVector PickPoint = FVector::ZeroVector;

	// Yaw the docked harvester faces (deg, station frame); it reverses in against this direction.
	UPROPERTY(EditAnywhere, Category="Unload Bay")
	float DockYaw = 0.f;

	// How far ahead of the dock the harvester lines up before it reverses in.
	UPROPERTY(EditAnywhere, Category="Unload Bay", meta=(ClampMin="300"))
	float ApproachDistance = 900.f;

	// The bay's own trolley runs this way from the bay to the bins; the two bays' runs must not overlap.
	UPROPERTY(EditAnywhere, Category="Unload Bay")
	EUnloadTrolleyRunDirection RunDirection = EUnloadTrolleyRunDirection::AlongRail;

	// Where the idle trolley waits, measured from the bay towards the bins (cm along the rail).
	UPROPERTY(EditAnywhere, Category="Unload Bay", meta=(ClampMin="0"))
	float ParkOffset = 350.f;
};

/** A storage bin the trolleys drop one resource into. */
USTRUCT(BlueprintType)
struct FUnloadBinSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Unload Bin")
	ERTSResourceType Resource = ERTSResourceType::Resource_None;

	// Socket on the building mesh at the bin floor; its fill mesh (storage level) stands there.
	UPROPERTY(EditAnywhere, Category="Unload Bin")
	FName SocketName = NAME_None;

	// Used when the socket is missing (station frame, cm).
	UPROPERTY(EditAnywhere, Category="Unload Bin")
	FVector FallbackLocation = FVector::ZeroVector;
};

/** Runtime state of one bay. */
struct FUnloadBayState
{
	TWeakObjectPtr<UHarvester> Harvester;
	TWeakObjectPtr<UResourceDropOff> DropOff;
	ERTSResourceType Resource = ERTSResourceType::Resource_None;
	bool bIsDocked = false;
	int32 ChunkAmount = 0;
	// World time of docking; measures the unload for GetExpectedUnloadSeconds.
	double DockedWorldSeconds = 0.0;
	// Rail distance of the bay's grab stop, and per resource the trolley offset (cm from the bay) of its bin stop.
	float BayRailDistance = 0.f;
	TMap<ERTSResourceType, float> BinOffsets;
	TMap<ERTSResourceType, FVector> BinWorldLocations;
};

/** A payload item falling from the opened grab into the bin. */
struct FUnloadFallingItem
{
	TWeakObjectPtr<UStaticMeshComponent> Component;
	// The grab head it hangs from again (hidden) once it landed, ready for the next load.
	TWeakObjectPtr<USceneComponent> HomeParent;
	float VelocityZ = 0.f;
	float StopZ = 0.f;
};

/** Runtime state and parts of one trolley; one trolley per bay. */
struct FUnloadTrolley
{
	int32 BayIndex = INDEX_NONE;
	EUnloadTrolleyPhase Phase = EUnloadTrolleyPhase::Idle;
	// Offset from the bay's grab stop towards the bins along the rail (cm, >= 0).
	float Offset = 0.f;
	float TargetOffset = 0.f;
	// How far the grab hangs below its travel height (cm).
	float HookDrop = 0.f;
	float TargetHookDrop = 0.f;
	// 0 = jaws open, 1 = closed.
	float JawAlpha = 0.f;
	float PhaseSeconds = 0.f;
	ERTSResourceType LoadResource = ERTSResourceType::Resource_None;
	bool bIsCarryingLoad = false;

	TWeakObjectPtr<UStaticMeshComponent> Body;
	TWeakObjectPtr<UStaticMeshComponent> Rope;
	TWeakObjectPtr<UStaticMeshComponent> Head;
	TWeakObjectPtr<UStaticMeshComponent> JawLeft;
	TWeakObjectPtr<UStaticMeshComponent> JawRight;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Crystals;
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Bars;
};
