// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "FactionPlayerController.generated.h"

class AFactionUnitPreview;
class ARTSAsyncSpawner;
class ICommands;
class UAudioComponent;
class USoundBase;
class UW_FactionSelectionMenu;

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
 * @brief Player controller that drives the faction selection menu and preview spawning for that map.
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

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionSelectionMenu> M_FactionSelectionMenuClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FRotator M_PreviewSpawnRotation;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FUnitPreviewActions M_UnitPreviewActions;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> M_AnnouncementAudioComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_FactionSelectionMenu;

	UPROPERTY()
	TWeakObjectPtr<AFactionUnitPreview> M_FactionUnitPreview;

	UPROPERTY()
	TWeakObjectPtr<ARTSAsyncSpawner> M_RTSAsyncSpawner;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CurrentPreviewActor;

	UPROPERTY()
	TScriptInterface<ICommands> M_CurrentCommandsInterface;

	FTimerHandle M_PreviewAttackTimerHandle;

	UFUNCTION()
	void HandleAnnouncementFinished();

	void InitFactionSelectionMenu();
	void InitPreviewReferences();
	void InitInputMode();
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
	bool GetIsValidFactionSelectionMenuClass() const;
	bool GetIsValidFactionSelectionMenu() const;
	bool GetIsValidFactionUnitPreview() const;
	bool GetIsValidRTSAsyncSpawner() const;
};
