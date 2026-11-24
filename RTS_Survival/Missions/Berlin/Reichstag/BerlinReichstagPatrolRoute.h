// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BerlinReichstagPatrolRoute.generated.h"

class UBillboardComponent;

/**
 * @brief A patrol route round the Reichstag for units the base trains (UBerlinReichstagTrainingSubsystem). Place it
 * in Ger_Berlin, then drag the patrol point diamonds; patrols visit the points at random and guard each one.
 * Routes are used in turn.
 */
UCLASS()
class RTS_SURVIVAL_API ABerlinReichstagPatrolRoute : public AActor
{
	GENERATED_BODY()

public:
	ABerlinReichstagPatrolRoute();

	/** @return The patrol points in world space. */
	UFUNCTION(BlueprintPure, Category = "Berlin|Reichstag")
	TArray<FVector> GetWorldPatrolPoints() const;

protected:
	// Relative to the actor; at least two.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (MakeEditWidget = "true"))
	TArray<FVector> PatrolPoints = {FVector(3000.f, 0.f, 0.f), FVector(-3000.f, 0.f, 0.f)};

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> M_EditorSprite = nullptr;
#endif
};
