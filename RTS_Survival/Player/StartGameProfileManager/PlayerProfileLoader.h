// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/CardSystem/CardUI/RTSCardWidgets/W_RTSCard.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "PlayerProfileLoader.generated.h"


struct FCardSaveData;
/** Handles loading the player profile units, resources and technologies.
* @note Units are loaded async, when done we call ACPPController::ONPlayerProfileLoadComplete()
* @note OnPlayerProfileLoadComplete will then continue with setting up the HQ reference and notifying
* the PlayerResourceManager that the profile is loaded.
*/
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerProfileLoader : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerProfileLoader();

	void LoadProfile(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner, const FVector& SpawnCenter, ACPPController* PlayerController,
		const bool bDoNotLoadPlayerUnits = false);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	void LoadInUnits(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner, const TArray<FTrainingOption>& InUnits, const FVector& SpawnCenter);
	void UpdateNomadicUnit(const TArray<FTrainingOption>& NomadicTrainingOptions, const ENomadicSubtype NomadicType) const;
	void ApplyTechnologies(const TArray<ETechnology>& Technologies) const;
	void ApplyResourceCards(const TArray<FUnitCost>& ResourceBonuses) const;
	
	TArray<FTrainingOption> ExtractUnitsToCreate(const TArray<FCardSaveData>& SelectedCards);
	TArray<FTrainingOption> ExtractTrainingOptionsOfType(const TArray<FCardSaveData>& SelectedCards, const ECardType TypeToExtract);
	TArray<FUnitCost> ExtractResources(const TArray<FCardSaveData>& SelectedCards);
	TArray<ETechnology> ExtractTechnologies(const TArray<FCardSaveData>& SelectedCards);

	void OnOptionSpawned(const FTrainingOption& Option);

	void SetupWaitForUnitsToSpawn(const TArray<FTrainingOption>& UnitsToSpawn);
	bool IsWaitingForUnitToSpawn(const FTrainingOption& Option);

	void RequestToSpawnHQ(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner, const FVector& SpawnCenter);
	void DebugOnProfileUnitSpawned(const FTrainingOption& Option);
	TMap<FTrainingOption, bool> M_IsProfileUnitSpawned;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	

};
