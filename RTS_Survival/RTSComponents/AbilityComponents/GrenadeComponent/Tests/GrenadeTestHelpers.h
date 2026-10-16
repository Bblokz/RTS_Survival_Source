// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

class ACPPController;
class ASceneCapture2D;
class ASquadController;
class ASquadUnit;
class UTextureRenderTarget2D;
class UWorld;

/**
 * @brief Shared plumbing of the grenade map tests (-MolotovValidate, -GrenadeBenchmark): starting the paused game,
 * finding a spot on the navmesh, spawning units for a player and saving scene-capture pictures.
 */
namespace FGrenadeTestHelpers
{
	/**
	 * @brief Presses the start-game control once the HUD exists.
	 * @param World Game world.
	 * @param OutPlayerController The player controller, valid when the game could be started.
	 * @return True when the game is running.
	 */
	bool TryStartGame(UWorld& World, TWeakObjectPtr<ACPPController>& OutPlayerController);

	// Answers the difficulty picker and unpauses; call every tick, the game pauses itself again on some events.
	void KeepGameRunning(UWorld& World, ACPPController* PlayerController);

	/**
	 * @brief Finds the test spot: the X,Y given on the command line, a unit-image anchor or the player start.
	 * @param World Game world.
	 * @param LocationSwitch Command line switch with "X,Y", e.g. TEXT("MolotovAt=").
	 * @param OutLocation The spot, dropped to the ground when there is ground below it.
	 * @return False when the map has none of them.
	 */
	bool FindTestSpot(UWorld& World, const TCHAR* LocationSwitch, FVector& OutLocation);

	// Map anchors sit at camera height; returns false when there is no ground (yet) below the location.
	bool DropToGround(UWorld& World, FVector& InOutLocation);

	/**
	 * @brief Moves a location onto the closest navmesh point; squads must be able to walk into throw range.
	 * @param World Game world.
	 * @param InOutLocation Location to move.
	 * @param SearchExtent Half extent of the search box.
	 * @return False when there is no navmesh within the extent.
	 */
	bool ProjectOntoNavMesh(UWorld& World, FVector& InOutLocation, const FVector& SearchExtent);

	/**
	 * @brief Spawns a unit with its owner set before BeginPlay, so it registers, collides and fights as that player.
	 * Soldiers a squad spawns later keep the owner of their own Blueprint (player 1).
	 * @param World Game world.
	 * @param ClassPath Blueprint class path of the squad, tank or other unit.
	 * @param Transform Spawn transform; the location is dropped to the ground and onto the navmesh.
	 * @param OwningPlayer Player the unit belongs to.
	 * @return The unit, or null when the class could not be loaded or spawned.
	 */
	AActor* SpawnUnitForPlayer(UWorld& World, const TCHAR* ClassPath, const FTransform& Transform, uint8 OwningPlayer);

	int32 GetLivingSoldierCount(ASquadController& Squad);
	bool GetIsLivingSoldier(ASquadUnit* Soldier);

	/**
	 * @brief Creates a scene capture that renders every frame like the game view (no fog of war post process).
	 * @param World Game world.
	 * @param FieldOfView Horizontal field of view in degrees.
	 * @param OutCapture The capture actor.
	 * @param OutRenderTarget Its render target.
	 * @return False when either could not be created.
	 */
	bool CreateCaptureCamera(UWorld& World, float FieldOfView, ASceneCapture2D*& OutCapture,
	                         UTextureRenderTarget2D*& OutRenderTarget);

	// Aims a capture down at a spot like the RTS camera (spring arm 3000 at -70 degrees).
	void AimLikeGameCamera(ASceneCapture2D& Capture, const FVector& LookAt);

	void SaveCapture(UWorld& World, UTextureRenderTarget2D& RenderTarget, const FString& Directory,
	                 const FString& FileName);
}

#endif
