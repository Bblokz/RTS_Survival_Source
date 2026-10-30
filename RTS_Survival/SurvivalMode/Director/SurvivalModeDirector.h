// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyTypes.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalRewardTypes.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"

#include "SurvivalModeDirector.generated.h"

class ACPPController;
class ANomadicVehicle;
class ARTSAsyncSpawner;
class ASurvivalEdgeSpawnZone;
class ASurvivalStartLocation;
class USurvivalDeploymentComponent;
class USurvivalModeSettings;
class USurvivalSovietBaseComponent;
class USurvivalWaveDirectorComponent;
class UUserWidget;
class UW_SurvivalArmySelection;
class UW_SurvivalHud;
class UW_SurvivalRewardPicker;
enum class ESurvivalRoundState : uint8;
class UW_SurvivalStartLocationPicker;

/**
 * @brief The one actor a survival level needs: runs the phases army selection → start location → deployment →
 * grace period → waves → defeat, and owns the deployment, wave and Soviet base components.
 * @note Place exactly one per survival map and set Settings; start locations and edge spawn zones are found in
 * the level at BeginPlay.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASurvivalModeDirector : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalModeDirector();

	/** @return The director of this world, nullptr outside survival maps. */
	static ASurvivalModeDirector* Get(const UObject* WorldContextObject);

	const USurvivalModeSettings* GetSettings() const { return Settings; }
	ESurvivalPhase GetPhase() const { return M_Phase; }
	FOnSurvivalPhaseChanged OnPhaseChanged;

	const TArray<TWeakObjectPtr<ASurvivalStartLocation>>& GetStartLocations() const { return M_StartLocations; }
	ASurvivalStartLocation* GetChosenStartLocation() const { return M_ChosenStartLocation.Get(); }
	const TArray<TWeakObjectPtr<ASurvivalEdgeSpawnZone>>& GetEdgeSpawnZones() const { return M_EdgeSpawnZones; }

	/** @return Seconds since the first wave phase started; 0 before that. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetElapsedSurvivalSeconds() const;

	/** @return Seconds left in the grace period; 0 outside it. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetGraceSecondsRemaining() const;

	/**
	 * @brief Where the waves aim: the HQ while alive, else the centroid of the player's buildings, else of the
	 * player's units, else the chosen start location.
	 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	FVector GetCurrentPlayerBaseLocation() const;

	/**
	 * @return The level's async spawner, also before the player controller holds its reference (the army screen opens
	 * that early); nullptr only when the level has none.
	 */
	ARTSAsyncSpawner* FindAsyncSpawner() const;

	FRTSGameDifficulty GetDifficulty() const;
	ERTSFaction GetPlayerFaction() const;

	/** @return The army the player confirmed, HQ first. */
	const TArray<FSurvivalArmyPick>& GetConfirmedArmy() const { return M_ConfirmedArmy; }

	/**
	 * @brief Called by the army selection UI. Validates the picks and moves to start location selection.
	 * @return False (and nothing changes) when the army is invalid; OutReason says why.
	 */
	bool ConfirmArmy(const TArray<FSurvivalArmyPick>& Picks, FText& OutReason);

	/** @brief Called by the start location picker; starts the deployment there. */
	void ConfirmStartLocation(ASurvivalStartLocation* StartLocation);

	/** @brief The picker previews a start location; moves the camera there. */
	void PreviewStartLocation(const ASurvivalStartLocation* StartLocation) const;

	/** @brief Called by the deployment component when the HQ has landed and every aircraft is gone. */
	void OnDeploymentFinished();

	/** @brief Called by the deployment component once the HQ touches the ground. */
	void OnHQLanded(ANomadicVehicle* HQ);

	USurvivalWaveDirectorComponent* GetWaveDirector() const { return M_WaveDirector; }
	USurvivalDeploymentComponent* GetDeployment() const { return M_Deployment; }
	USurvivalSovietBaseComponent* GetSovietBases() const { return M_SovietBases; }
	UW_SurvivalHud* GetHudWidget() const { return M_HudWidget; }

	/** @return Waves launched so far (for the HUD and the result screen). */
	UFUNCTION(BlueprintPure, Category = "Survival")
	int32 GetWavesLaunched() const;

	/** @return Enemies of the current wave still alive or spawning; 0 between waves. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	int32 GetEnemiesRemaining() const;

	/** @return Waves fully survived. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	int32 GetRoundsSurvived() const;

	ESurvivalRoundState GetRoundState() const;

	/** @return Seconds left in the grace between two waves; 0 outside it. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetIntermissionSecondsRemaining() const;

	/** @return True between two waves (the reward can be picked). */
	bool GetIsInIntermission() const { return M_IntermissionEndTime >= 0.0; }

	/** @return True once the reward of the current intermission was picked. */
	bool GetHasChosenReward() const { return bM_HasChosenReward; }

	/**
	 * @brief Called by the reward picker: flies the chosen reinforcement set in. Only the first pick of an
	 * intermission counts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Survival")
	void ChooseReward(int32 OptionIndex);

	/** @return Best survival time stored for this map, 0 when none. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetBestSurvivalSeconds() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival")
	TObjectPtr<USurvivalModeSettings> Settings = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival|UI")
	TSubclassOf<UW_SurvivalArmySelection> ArmySelectionWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival|UI")
	TSubclassOf<UW_SurvivalStartLocationPicker> StartLocationPickerWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival|UI")
	TSubclassOf<UW_SurvivalHud> HudWidgetClass;

	// Builds its own tree; a Blueprint child can restyle it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Survival|UI")
	TSubclassOf<UW_SurvivalRewardPicker> RewardPickerWidgetClass;

	// Skip army selection and start location in PIE: deploy the default test army at the first start location.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Debug")
	bool bDebugAutoStart = false;

	// Seconds of grace for debug auto start; negative uses the settings.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Debug")
	float DebugGraceSecondsOverride = -1.f;

private:
	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<USurvivalDeploymentComponent> M_Deployment;

	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<USurvivalWaveDirectorComponent> M_WaveDirector;

	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<USurvivalSovietBaseComponent> M_SovietBases;

	ESurvivalPhase M_Phase = ESurvivalPhase::None;
	void SetPhase(ESurvivalPhase NewPhase);

	TArray<TWeakObjectPtr<ASurvivalStartLocation>> M_StartLocations;
	TWeakObjectPtr<ASurvivalStartLocation> M_ChosenStartLocation;
	TArray<TWeakObjectPtr<ASurvivalEdgeSpawnZone>> M_EdgeSpawnZones;
	TArray<FSurvivalArmyPick> M_ConfirmedArmy;
	TWeakObjectPtr<ANomadicVehicle> M_HQ;

	UPROPERTY()
	TObjectPtr<UUserWidget> M_ActiveSetupWidget;

	UPROPERTY()
	TObjectPtr<UW_SurvivalHud> M_HudWidget;

	// World time (seconds) when the grace period / waves started; negative when not started.
	double M_GraceStartTime = -1.0;
	double M_WavesStartTime = -1.0;
	double M_DefeatTime = -1.0;
	float M_GraceSeconds = 180.f;

	FTimerHandle M_GraceTimerHandle;
	FTimerHandle M_DefeatCheckTimerHandle;
	FTimerHandle M_WaitForControllerHandle;

	void BeginPlay_GatherLevelActors();
	void BeginPlay_WaitForPlayerController();
	void StartSetup();

	void ShowArmySelection();
	void ShowStartLocationPicker();
	void RemoveActiveSetupWidget();
	void SetSetupInputMode(UUserWidget* WidgetToFocus, bool bSetupActive) const;
	void ShowHud();

	void BeginDeployment();
	void GenerateStartLocationContent();
	void StartGracePeriod();
	void OnGracePeriodFinished();
	void StartWaves();

	// ---------------- Intermission and rewards
	UPROPERTY()
	TObjectPtr<UW_SurvivalRewardPicker> M_RewardPicker;

	UPROPERTY()
	TArray<FSurvivalRewardOption> M_RewardOptions;

	// World time the current intermission ends; negative outside one.
	bool bM_HasPushedDialogSuppression = false;
	double M_IntermissionEndTime = -1.0;
	int32 M_IntermissionRound = 0;
	bool bM_HasChosenReward = false;
	FTimerHandle M_IntermissionTimerHandle;
	FRandomStream M_RewardRandom;
	FDelegateHandle M_RoundSurvivedHandle;

	void OnRoundSurvived(int32 RoundNumber);
	void OnIntermissionFinished();
	void ShowRewardPicker();
	void CloseRewardPicker();
	void DeliverReward(const FSurvivalRewardOption& Option);

	// Shows the HUD banner for every wave the wave director launches.
	void OnWaveIncoming(const struct FSurvivalWaveAnnouncement& Announcement);
	FDelegateHandle M_WaveIncomingHandle;

	void CheckDefeat();
	bool GetHasPlayerLost() const;
	void OnDefeat();
	void SaveBestSurvivalTime() const;
	FString GetBestTimeSaveSlotName() const;

	void DebugAutoStart();
	TArray<FSurvivalArmyPick> BuildDebugArmy() const;

	ACPPController* GetPlayerController() const;
	bool GetIsValidSettings() const;
};
