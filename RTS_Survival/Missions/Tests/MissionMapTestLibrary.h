// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/GameUI/LocationIntro/LocationIntroTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "MissionMapTestLibrary.generated.h"

class ANomadicVehicle;
class UMissionBase;
enum class EBuildingExpansionType : uint8;

/**
 * @brief Lets headless mission tests (python in a throwaway editor) play the player's part through the same code the
 * UI runs: a training click on a trainer's menu item, a building expansion option click and its placement click.
 * Every function does nothing and returns false in builds without map tests (Shipping).
 */
UCLASS()
class RTS_SURVIVAL_API UMissionMapTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Clicks a training option on the trainer's menu as the player would (mission locks and costs apply).
	 * @param Trainer A player nomadic with a trainer, e.g. the HQ.
	 * @param Option The option to train.
	 * @return 1 queued, 2 shown as locked by the mission (not queued), 3 shown but not queued, 0 not on the menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_ClickTrainingOption(ANomadicVehicle* Trainer, FTrainingOption Option);

	/**
	 * @brief Clicks a building expansion option of the owner as the player would; the expansion then loads and waits
	 * for its placement click (MapTest_PlaceLoadedBxp).
	 * @param ExpansionOwner The player nomadic building that offers the expansion.
	 * @param ExpansionType The expansion to build.
	 * @return False when the owner does not offer it or has no free slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static bool MapTest_StartBxpPlacement(ANomadicVehicle* ExpansionOwner, EBuildingExpansionType ExpansionType);

	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_IsBxpWaitingForPlacement(const UObject* WorldContextObject);

	/**
	 * @brief The placement click: snaps like a real click (sockets included), pays and starts the construction.
	 * @return False when nothing waits for placement or the player cannot pay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_PlaceLoadedBxp(const UObject* WorldContextObject, FVector ClickLocation);

	/**
	 * @brief The player converts a nomadic truck: selects it, presses convert and clicks the preview at Location.
	 * @param NomadicTruck A player nomadic in truck mode.
	 * @param Location Ground location of the placement click.
	 * @return 1 ordered, 2 not a truck, 3 no nomadic preview started, 4 the spot was rejected, 5 the click was not
	 * taken, 0 invalid input; +10 when the preview demanded a build radius (never for the registered player HQ).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_ConvertNomadicAt(ANomadicVehicle* NomadicTruck, FVector Location);

	/**
	 * @brief Spawns any actor class in the game world (python cannot: the deferred-spawn nodes are internal-only), e.g.
	 * a player tank Blueprint for a trigger test. Collision: adjust if possible, always spawn.
	 * @return The spawned actor or null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static AActor* MapTest_SpawnActor(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass,
	                                  FVector Location, FRotator Rotation);

	/** @return True when the unit's command card holds the ability. */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static bool MapTest_HasAbility(AActor* Unit, EAbilityID Ability);

	/**
	 * @brief Stops every turret and hull weapon of a tank from firing, for clips and tests that need a silent tank (the
	 * run-over clip: the Tiger must crush the squad, not shoot it). A new command may wake the turrets again, so call
	 * it after every order (or every tick).
	 * @return How many turrets and hull weapons were stopped; 0 for no tank.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_HoldTankFire(AActor* Tank);

	/**
	 * @brief Puts a squad's soldiers on the given ground spots, one per spot in soldier order, standing still, and gives
	 * each the squad's owner (soldiers keep their own Blueprint's owner otherwise).
	 * @param Squad A squad controller whose soldiers have spawned.
	 * @param GroundSpots Ground locations; the soldier's feet are put on them.
	 * @return How many soldiers were placed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests")
	static int32 MapTest_PlaceSquadSoldiers(AActor* Squad, const TArray<FVector>& GroundSpots);

	/**
	 * @brief Runs UMissionBase::ShowLocationIntro on a fresh mission activated on the map's mission manager.
	 * @param bCompleteMissionRightAway Give the mission a next mission and complete it right after the node, as a
	 * mission whose only job is the intro would.
	 * @return False without a mission manager or when the intro was refused.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_StartLocationIntro(const UObject* WorldContextObject, FLocationIntroTexts Texts,
	                                       ELocationIntroDriftAxis DriftAxis, float DriftDistance,
	                                       float DurationSeconds, bool bPanCamera,
	                                       FLocationIntroPanSettings PanSettings,
	                                       FLocationIntroPresentation Presentation,
	                                       bool bCompleteMissionRightAway);

	/**
	 * @brief State a location intro test checks: whether the intro still runs, whether the screen is handed back
	 * (no takeover active, so the HUD shows and the camera takes input) and the camera pawn pose.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static void MapTest_GetLocationIntroState(const UObject* WorldContextObject, bool& bOutIntroPlaying,
	                                          bool& bOutScreenHandedBack, FVector& OutCameraLocation,
	                                          FRotator& OutCameraRotation);

	// ---- Mission chain walking (C++ only, for the mission chain audit) ----

	/** @return The missions the map's mission manager has active now; empty without a manager or map tests. */
	static TArray<UMissionBase*> MapTest_GetActiveMissions(const UObject* WorldContextObject);

	/** @return True when the mission started but waits for its trigger (an area the player must reach, a timer). */
	static bool MapTest_GetIsMissionWaitingForTrigger(const UMissionBase* Mission);

	/** @return True when the mission is past its trigger and running (not complete). */
	static bool MapTest_GetIsMissionRunning(const UMissionBase* Mission);

	/**
	 * @brief Fires a waiting mission's trigger the way UMissionTrigger does when its condition is met, so an
	 * unattended run walks a mission chain whose next step waits for the player.
	 * @return False when the mission was not waiting for its trigger.
	 */
	static bool MapTest_FireMissionTrigger(UMissionBase* Mission);

	// ---- Player global abilities ----

	/**
	 * @brief Uses one of the player's loaded global abilities at a location the way an aimed click does: it pays,
	 * starts the cooldown and takes a charge, so tests can spend a mission-granted ability.
	 * @param WorldContextObject Any object of the PIE world.
	 * @param Ability The loaded ability to use.
	 * @param Location Ground location the ability is aimed at.
	 * @return True when the ability was used (it was loaded and not blocked); always false without map tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static bool MapTest_UsePlayerGlobalAbility(const UObject* WorldContextObject, EGlobalAbility Ability,
	                                           FVector Location);

	/**
	 * @brief What the player sees of one global ability: whether it is on the side panel and its charges, cooldown,
	 * cost and strike size.
	 * @param WorldContextObject Any object of the PIE world.
	 * @param Ability The ability to report.
	 * @param bOutLoaded The ability is among the player's loaded (panel) abilities.
	 * @param bOutPanelVisible The global ability side panel is shown.
	 * @param OutLoadedCount Number of abilities on the panel.
	 * @param OutCharges Uses left; -1 = unlimited.
	 * @param OutCooldownTime Seconds between uses.
	 * @param OutCooldownRemaining Seconds until the next use.
	 * @param OutResourceCostCount Resource types the ability costs (0 = free).
	 * @param OutStrafeAircraft Aircraft per strike for strafing abilities, else 0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|MapTests", meta = (WorldContext = "WorldContextObject"))
	static void MapTest_GetPlayerGlobalAbilityState(const UObject* WorldContextObject, EGlobalAbility Ability,
	                                                bool& bOutLoaded, bool& bOutPanelVisible, int32& OutLoadedCount,
	                                                int32& OutCharges, int32& OutCooldownTime,
	                                                int32& OutCooldownRemaining, int32& OutResourceCostCount,
	                                                int32& OutStrafeAircraft);
};
