// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TechWheelEffectProbeDumpSubsystem.generated.h"

class UTechWheelEffectProbes;

/**
 * @brief Checks the tech wheel's effect data without the screen. Start the world campaign map with
 * -TechWheelEffectProbeDump (-game -nullrhi -RTSWorldCampaignSaveSlot=<name>): it probes every technology one after
 * the other and logs a TECHPROBE line per technology (effect class, affected units, each changed weapon with its
 * values and pip levels before and after), then quits. Not created in a shipping build.
 */
UCLASS()
class RTS_SURVIVAL_API UTechWheelEffectProbeDumpSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

private:
	void ProbeNextTechnology();
	void LogProbedTechnology() const;
	void Finish();

	UPROPERTY()
	TObjectPtr<UTechWheelEffectProbes> M_Probes;

	double M_StartSeconds = 0.0;
	// Enum index of the technology being probed; 0 (none) until the dump starts.
	int32 M_TechnologyIndex = 0;
	bool bM_IsWaitingForProbe = false;
	bool bM_IsDone = false;
};
