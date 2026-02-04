// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "FactionPlayerController.generated.h"

class AFactionUnitPreview;
class ARTSAsyncSpawner;
class ICommands;
class UAudioComponent;
class USoundBase;
class UUserWidget;
class UWorld;
class UW_FactionDifficultyPicker;
class UW_FactionPopup;
class UW_FactionSelectionMenu;
class UW_FactionWorldGenerationSettings;
enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FUnitPreviewActions
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MinTimeTillGroundAttack = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MaxTimeTillGroundAttack = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FVector> GroundAttackLocations;
};

/**
 * @brief Player controller that drives the faction setup flow and preview spawning on the selection map.
 *
 * @details Widget flow handled by this controller:
 * - Start: show the faction selection menu and preview units while the player browses.
 * - Launch Campaign: store the selected faction, remove the menu, and show the faction difficulty picker.
 * - Difficulty Selected: store the chosen difficulty and its percentage, remove the picker, and show world generation settings.
 * - World Generation Settings: when confirmed, store campaign settings, difficulty, and faction in the game instance,
 *   then load the campaign world. Back returns to the difficulty picker and repeats the flow.
 */
UCLASS()
class RTS_SURVIVAL_API AFactionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFactionPlayerController();

	virtual void BeginPlay() override;

	void SpawnPreviewForTrainingOption(const FTrainingOption& TrainingOption);
	void PlayAnnouncementSound(USoundBase* AnnouncementSound);
	void StopAnnouncementSound();
	void HandleLaunchCampaignRequested(const ERTSFaction SelectedFaction);
	void HandleFactionDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty);
	void HandleWorldGenerationBackRequested();
	void HandleWorldGenerationSettingsGenerated(const FCampaignGenerationSettings& Settings);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionPopup> M_FactionPopupClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionSelectionMenu> M_FactionSelectionMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionDifficultyPicker> M_FactionDifficultyPickerClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionWorldGenerationSettings> M_FactionWorldGenerationSettingsClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSoftObjectPtr<UWorld> M_CampaignWorld;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FRotator M_PreviewSpawnRotation;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FUnitPreviewActions M_UnitPreviewActions;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> M_AnnouncementAudioComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionPopup> M_FactionPopup;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_FactionSelectionMenu;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionDifficultyPicker> M_FactionDifficultyPicker;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionWorldGenerationSettings> M_FactionWorldGenerationSettings;

	UPROPERTY()
	TWeakObjectPtr<AFactionUnitPreview> M_FactionUnitPreview;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_RTSAsyncSpawner;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CurrentPreviewActor;

	UPROPERTY()
	TScriptInterface<ICommands> M_CurrentCommandsInterface;

	FTimerHandle M_PreviewAttackTimerHandle;

	ERTSFaction M_SelectedFaction = ERTSFaction::NotInitialised;
	FRTSGameDifficulty M_SelectedGameDifficulty;

	UFUNCTION()
	void HandleFactionPopupAccepted();

	UFUNCTION()
	void HandleAnnouncementFinished();

	void InitFactionPopup();
	void InitFactionSelectionMenu();
	void InitFactionDifficultyPicker();
	void InitFactionWorldGenerationSettings();
	void InitPreviewReferences();
	void InitInputModeForWidget(UUserWidget* WidgetToFocus);
	/**
	 * @brief Captures the async spawn completion to update preview state and start idle actions.
	 * @param TrainingOption The option that was requested for preview spawning.
	 * @param SpawnedActor The actor spawned by the async spawner.
	 * @param SpawnRequestId The request identifier provided to the spawner.
	 */
	void HandlePreviewSpawned(const FTrainingOption& TrainingOption, AActor* SpawnedActor, const int32 SpawnRequestId);
	void ResetPreviewAttackTimer();
	void HandlePreviewAttackTimer();
	FVector GetRandomGroundAttackLocation() const;
	void ClearCurrentPreview();

	bool GetIsValidAnnouncementAudioComponent() const;
	bool GetIsValidFactionPopupClass() const;
	bool GetIsValidFactionPopup() const;
	bool GetIsValidFactionSelectionMenuClass() const;
	bool GetIsValidFactionSelectionMenu() const;
	bool GetIsValidFactionDifficultyPickerClass() const;
	bool GetIsValidFactionDifficultyPicker() const;
	bool GetIsValidFactionWorldGenerationSettingsClass() const;
	bool GetIsValidFactionWorldGenerationSettings() const;
	bool GetIsValidFactionUnitPreview() const;
	bool GetIsValidRTSAsyncSpawner() const;
};
