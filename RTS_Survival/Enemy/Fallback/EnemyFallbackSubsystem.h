// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"

#include "EnemyFallbackSubsystem.generated.h"

class AEnemyController;
class ASquadController;

/** When a beaten enemy squad gives up the fight and runs to its rally point (BerlinMissionFlow.md 4.2). */
USTRUCT(BlueprintType)
struct FEnemyFallbackSettings
{
	GENERATED_BODY()

	// Where fleeing units run to (e.g. the Reichstag forecourt); they disappear there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback")
	FVector RallyLocation = FVector::ZeroVector;

	// Only squads of this player are watched (the enemy AI player).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0"))
	int32 OwningPlayerToWatch = RTSPlayers::FirstAIPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0.2"))
	float CheckIntervalSeconds = 1.f;

	// Seconds since the squad lost its first man before it may give up: it has been fighting for a while.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0.0"))
	float MinCombatSeconds = 20.f;

	// The squad gives up once it has lost at least this share of its men.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float LossFractionToFallBack = 0.5f;

	// Squads smaller than this at full strength never fall back (single soldiers, crews).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "1"))
	int32 MinSquadSizeToWatch = 2;

	// Squads already this close to the rally point stay and fight (the base defends itself).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fallback", meta = (ClampMin = "0.0", Units = "cm"))
	float StayAndFightRadius = 4000.f;
};

/** What the subsystem remembers about one watched squad. */
USTRUCT()
struct FEnemyFallbackSquadTrack
{
	GENERATED_BODY()

	// Most men the squad was seen with (its full strength; reinforcements raise it).
	int32 FullStrength = 0;

	// World seconds of the first loss; negative while it has lost nobody.
	double FirstLossSeconds = -1.0;
};

/** A spawned group whose infantry gives up once its last vehicle is gone (patrols with bInfantryFleesWhenVehiclesLost). */
USTRUCT()
struct FEnemyFallbackEscortedGroup
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Vehicles;

	// Squad controllers of the group.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Squads;
};

/**
 * @brief Makes beaten enemy squads give up and run to a rally point, where they disappear; a mission raises the alarm
 * when one arrives. A mission starts it with StartFallbackWatch, can push any unit with ForceUnitToFallBack (patrol
 * infantry whose vehicles died) and reads GetIsFleeing / GetFleeingCount.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyFallbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback", meta = (WorldContext = "WorldContextObject"))
	static UEnemyFallbackSubsystem* GetEnemyFallbackSubsystem(const UObject* WorldContextObject);

	/** Starts (or restarts with new settings) watching the enemy squads. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Fallback")
	void StartFallbackWatch(const FEnemyFallbackSettings& Settings);

	/** Stops deciding new fallbacks; units already running keep running. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Fallback")
	void StopFallbackWatch();

	/**
	 * @brief Sends one enemy unit (squad controller, squad unit or vehicle) to the rally point now.
	 * @param Unit The unit to send; a squad unit sends its whole squad.
	 * @return True when it is now fleeing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Fallback")
	bool ForceUnitToFallBack(AActor* Unit);

	/**
	 * @brief Watches a spawned group (vehicles escorting infantry): once its last vehicle is destroyed, the squads still
	 * alive run to the rally point. Checked while the fallback watch runs; a group without vehicles is ignored.
	 * @param Units The spawned units; squad men resolve to their squad.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Fallback")
	void WatchVehicleEscortedGroup(const TArray<AActor*>& Units);

	/** @return Whether a unit (squad controller, squad man or vehicle) still lives: squads with a living man, vehicles not dying. */
	static bool GetIsUnitActorAlive(const AActor* UnitActor);

	/** @return Whether the actor (a squad controller, one of its men, or a vehicle) is running to the rally point. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	bool GetIsFleeing(const AActor* Actor) const;

	/** @return Fleeing units that are still alive and on their way. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	int32 GetFleeingCount() const;

	/** @return Every unit that has started to flee since the watch began, alive or not. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	int32 GetTotalFledCount() const { return M_TotalFledCount; }

	/** @return The fleeing units still alive (squad controllers and vehicles). */
	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	TArray<AActor*> GetFleeingUnits() const;

	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	FVector GetRallyLocation() const { return M_Settings.RallyLocation; }

	UFUNCTION(BlueprintPure, Category = "Enemy|Fallback")
	bool GetIsWatching() const { return bM_IsWatching; }

	virtual void Deinitialize() override;

private:
	FEnemyFallbackSettings M_Settings;

	// Squads seen so far; weak, the squads die and get destroyed.
	TMap<TWeakObjectPtr<ASquadController>, FEnemyFallbackSquadTrack> M_SquadTracks;

	// Units sent to the rally point (squad controllers and vehicles).
	TArray<TWeakObjectPtr<AActor>> M_FleeingUnits;

	// Groups whose infantry runs when their vehicles are gone; dropped once that happened or the group is wiped out.
	UPROPERTY()
	TArray<FEnemyFallbackEscortedGroup> M_EscortedGroups;

	int32 M_TotalFledCount = 0;
	bool bM_IsWatching = false;
	FTimerHandle M_CheckTimerHandle;

	void CheckSquads();
	void CheckEscortedGroups();
	// @return True when the group is done (its infantry was sent off, or nothing of it is left to watch).
	bool CheckOneEscortedGroup(const FEnemyFallbackEscortedGroup& Group);
	void CheckOneSquad(ASquadController* Squad, const double NowSeconds);
	bool GetShouldSquadFallBack(const ASquadController* Squad, FEnemyFallbackSquadTrack& Track,
	                            const double NowSeconds) const;
	bool SendUnitToRallyPoint(AActor* UnitActor);
	AEnemyController* GetEnemyControllerForUnit(const AActor* UnitActor) const;
	bool GetIsUnitAlive(const AActor* UnitActor) const;
	void PruneTracks();
	static AActor* ResolveFallbackUnit(const AActor* Actor);
};
