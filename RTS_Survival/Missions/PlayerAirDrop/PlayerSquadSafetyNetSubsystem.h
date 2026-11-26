// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#include "PlayerSquadSafetyNetSubsystem.generated.h"

class AMissionManager;
class UMissionBase;

/**
 * @brief Map setting on the mission manager: while the player depends on a few starting squads, a paratrooper plane
 * tops them up again when nearly all of them are lost, so an early mistake does not end the mission.
 */
USTRUCT(BlueprintType)
struct FPlayerSquadSafetyNetSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net")
	bool bEnabled = false;

	// The squads that are counted and dropped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net", meta = (EditCondition = "bEnabled"))
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_Ger_Vultures;

	// A drop is sent once the player has this many of those squads or fewer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net",
		meta = (EditCondition = "bEnabled", ClampMin = "0"))
	int32 HelpAtOrBelowSquadCount = 1;

	// The drop brings the player back up to this many squads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net",
		meta = (EditCondition = "bEnabled", ClampMin = "1"))
	int32 TargetSquadCount = 3;

	// The safety net ends for good once a mission of this class has completed (the stage before the player has a
	// base of their own). Empty keeps it running for the whole map.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net", meta = (EditCondition = "bEnabled"))
	TSubclassOf<UMissionBase> StopWhenMissionClassCompleted;

	// Safe drop zones along the player's route; the one nearest the player's squads (or where they were last seen)
	// is used. Empty drops at the squads themselves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net", meta = (EditCondition = "bEnabled"))
	TArray<FVector> LandingPoints;

	// German paratrooper ability whose plane and jump behaviour are used; the payload is replaced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net", meta = (EditCondition = "bEnabled"))
	EGlobalAbility ParachuteAbility = EGlobalAbility::GA_GerParatroopers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net",
		meta = (EditCondition = "bEnabled", ClampMin = "0.5"))
	float CheckIntervalSeconds = 2.f;

	// Minimum time between two drops, so squads still in the air are not answered with another plane.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad Safety Net",
		meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float MinSecondsBetweenDrops = 60.f;
};

/**
 * @brief Runs the mission manager's squad safety net: counts the player's squads of one type and asks
 * UPlayerAirDropSubsystem for paratroopers when too few are left.
 * @note Armed only after the player has had more than HelpAtOrBelowSquadCount squads once, so the start of a map
 * (squads still spawning, start screen) never triggers a drop.
 */
UCLASS()
class RTS_SURVIVAL_API UPlayerSquadSafetyNetSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * @brief Starts watching the player's squads with the map's settings (called by the mission manager).
	 * @param Settings The mission manager's safety net settings.
	 * @param MissionManager Answers whether the stop mission has completed.
	 */
	void StartSafetyNet(const FPlayerSquadSafetyNetSettings& Settings, AMissionManager* MissionManager);

	static UPlayerSquadSafetyNetSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net", meta = (WorldContext = "WorldContextObject"))
	static UPlayerSquadSafetyNetSubsystem* GetPlayerSquadSafetyNetSubsystem(const UObject* WorldContextObject)
	{
		return Get(WorldContextObject);
	}

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net")
	bool GetIsWatching() const { return bM_IsWatching; }

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net")
	bool GetIsArmed() const { return bM_IsArmed; }

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net")
	int32 GetDropCount() const { return M_DropCount; }

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net")
	int32 GetLastCountedSquads() const { return M_LastCountedSquads; }

	UFUNCTION(BlueprintPure, Category = "Mission|Squad Safety Net")
	FVector GetLastDropLocation() const { return M_LastDropLocation; }

	/**
	 * @brief Map tests only: kills every soldier of a squad the way a run-over does (no scream), so a test can take
	 * the player below the safety net's threshold.
	 * @param Squad A squad controller.
	 * @return Soldiers killed; 0 without map tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission|Squad Safety Net")
	static int32 MapTest_KillSquad(AActor* Squad);

protected:
	virtual void Deinitialize() override;

private:
	FPlayerSquadSafetyNetSettings M_Settings;

	UPROPERTY()
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	FTimerHandle M_CheckTimer;
	bool bM_IsWatching = false;
	bool bM_IsArmed = false;
	int32 M_DropCount = 0;
	int32 M_LastCountedSquads = 0;
	float M_LastDropWorldSeconds = -1.f;
	FVector M_LastDropLocation = FVector::ZeroVector;
	// Centre of the player's squads at the last check that found any; the drop goes there when none are left.
	FVector M_LastKnownSquadCentre = FVector::ZeroVector;
	bool bM_HasKnownSquadCentre = false;

	void CheckSquads();
	bool GetShouldStop() const;
	void StopWatching();
	/**
	 * @brief Living player squads of the configured type and the centre of their soldiers.
	 * @param OutCentre Average location of those squads; unchanged when there are none.
	 * @return Number of squads that still have a living soldier.
	 */
	int32 CountPlayerSquads(FVector& OutCentre) const;
	bool GetIsDropOnCooldown() const;
	void SendDrop(const int32 SquadsToDrop);
	FVector PickLandingPoint() const;
};
