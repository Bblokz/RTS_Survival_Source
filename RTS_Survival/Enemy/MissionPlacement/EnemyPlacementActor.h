// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"

#include "EnemyPlacementActor.generated.h"

class AEnemyController;
class UBillboardComponent;
class UEnemyPlacementShapesComponent;
class UTextRenderComponent;
class UTexture2D;
struct FEnemyPlacementShapes;

/**
 * @brief Base of the actors a mission map places to author enemy behaviour in the level (wave spawners, convoy
 * routes, strongpoints). Gives each one an editor sprite, a coloured label with configuration problems, a summary and
 * a selection-only detail drawing, and waits for the enemy controller and game difficulty before it acts.
 * Nothing of it shows in game; the subclasses drive the existing enemy AI systems.
 */
UCLASS(Abstract, HideCategories = (Rendering, Collision, Input, HLOD, Physics, LOD, Cooking, Replication))
class RTS_SURVIVAL_API AEnemyPlacementActor : public AActor
{
	GENERATED_BODY()

public:
	AEnemyPlacementActor();

	/** @return The editor colour of this actor (one hue per kind of placed enemy actor). */
	virtual FColor GetPlacementColor() const;

	/** @return A short problem shown on the editor label in yellow; empty when the setup is usable. */
	virtual FString GetConfigurationProblem() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ----- Editor -----
	/** @return The first word of the editor label ("ATTACK", "CONVOY"...). */
	virtual FString GetPlacementKindName() const;
	/** @return The designer ID shown on the label after the kind. */
	virtual FName GetPlacementId() const;
	virtual UTexture2D* GetEditorSpriteTexture() const;

	/**
	 * @brief Fills the two drawing layers; called whenever the actor is moved or edited in the editor.
	 * @param OutSummary Always drawn: what the actor is and where it acts.
	 * @param OutDetail Drawn only while the actor is selected.
	 */
	virtual void BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const;

	void RefreshEditorVisuals();
	TArray<FVector> GetWorldPoints(const TArray<FVector>& LocalPoints) const;

	// ----- Waiting for the enemy AI -----
	/**
	 * @brief The enemy controller and the game difficulty are set up during the first frames of a mission; this waits
	 * DelaySeconds, then retries until both are ready and calls OnEnemyAIReady (Normal when the difficulty never comes).
	 * @param DelaySeconds Wait before the first check.
	 */
	void WaitForEnemyAI(const float DelaySeconds);
	void CancelWaitForEnemyAI();
	bool GetIsWaitingForEnemyAI() const;

	virtual void OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty);
	// Called when no enemy controller was found while GetNeedsEnemyController is true; the error is already reported.
	virtual void OnEnemyAINeverReady();
	virtual bool GetNeedsEnemyController() const { return true; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> M_EditorSprite = nullptr;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> M_EditorLabel = nullptr;

	UPROPERTY()
	TObjectPtr<UEnemyPlacementShapesComponent> M_SummaryShapes = nullptr;

	UPROPERTY()
	TObjectPtr<UEnemyPlacementShapesComponent> M_DetailShapes = nullptr;
#endif

	FTimerHandle M_EnemyAIReadyTimerHandle;
	// Checks made while the enemy controller or the game difficulty was not ready yet.
	int32 M_EnemyAIReadyRetryCount = 0;

	void ScheduleEnemyAIReadyCheck(const float DelaySeconds);
	void CheckEnemyAIReady();
	FString GetEditorLabelText() const;
};
