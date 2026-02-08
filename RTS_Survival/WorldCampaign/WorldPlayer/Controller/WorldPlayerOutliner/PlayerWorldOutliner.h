// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/PlayerOutlineComponent/RTSOutlineTypes/RTSOutlineTypes.h"
#include "PlayerWorldOutliner.generated.h"

class AActor;

/**
 * @brief Handles campaign-map outline updates from cursor hover hits each player tick.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerWorldOutliner : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerWorldOutliner();

	/**
	 * @brief Updates hover outlines for world campaign objects driven by cursor hit results.
	 * @param HitActor The actor under the cursor trace, or nullptr when nothing was hit.
	 * @param HitLocation The projected hit location used by other campaign hover systems.
	 */
	void OnPlayerTick(AActor* HitActor, const FVector& HitLocation);

private:
	ERTSOutLineTypes GetOutlineTypeForActor(const AActor* Actor) const;
	void ResetOutlineOnPreviousActor();
	void ResetOutlineIfActorChanged(const AActor* NewActor);
	void SetOutLineOnActor(const AActor* ActorToOutLine, ERTSOutLineTypes OutLineType) const;
	void ResetActorOutline(const AActor* ActorToReset) const;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_OutlinedActor;
};
