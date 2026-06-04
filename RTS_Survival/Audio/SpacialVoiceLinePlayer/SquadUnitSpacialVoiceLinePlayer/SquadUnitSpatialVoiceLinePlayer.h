// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SpatialVoiceLinePlayer.h"
#include "SquadUnitSpatialVoiceLinePlayer.generated.h"


class ASquadUnit;

/**
 * @brief Extends spatial voice playback for squad units that schedule idle and combat chatter.
 * The owning squad unit calls OnSquadSet_SetupIdleVoiceLineCheck after squad membership is assigned.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USquadUnitSpatialVoiceLinePlayer : public USpatialVoiceLinePlayer
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	USquadUnitSpatialVoiceLinePlayer();

	void OnSquadSet_SetupIdleVoiceLineCheck();
	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool ShouldPlaySpatialVoiceLine(ERTSVoiceLine VoiceLineType) const override;

	virtual float GetBaseIntervalBetweenSpatialVoiceLines(float& OutFluxPercentage) const override;

	virtual bool BeginPlay_CheckEnabledForSpatialAudio() override;

private:
	FTimerHandle M_SquadIdleTimer;


	void BeginPlay_SetupOwnerAsSquadUnit();

	void CheckPlayIdleOrCombatVoiceLine();
	void StopIdleVoiceLineTimer();
	bool GetIsValidOwningSquadUnit() const;

	UPROPERTY()
	TWeakObjectPtr<ASquadUnit> M_OwningSquadUnit = nullptr;

	int32 GetSquadIndex() const;
	
	
};
