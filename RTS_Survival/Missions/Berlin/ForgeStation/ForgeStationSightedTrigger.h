// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"

#include "ForgeStationSightedTrigger.generated.h"

/**
 * @brief Trigger of the Forge station bonus: fires when a player unit comes within Radius of the station (a stand-in
 * for first sight; there is no fog-of-war callback) or when any counted target already died, e.g. to artillery.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UForgeStationSightedTrigger : public UMissionTrigger
{
	GENERATED_BODY()

public:
	virtual EMissionTriggerType GetTriggerType() const override { return EMissionTriggerType::PlayerUnitsNear; }

	// Outermost guard post + guard reach + 10 m (BerlinForgeStation.md section 5).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "1000.0", Units = "cm"))
	float Radius = 10000.f;

	// Used when the map has no ABerlinForgeStation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FVector FallbackCentre = FVector(363.f, 1382.f, 0.f);

	// The overlap runs this often, not every frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "0.05"))
	float CheckIntervalSeconds = 0.5f;

protected:
	virtual bool CheckTrigger_Implementation() const override;

private:
	mutable double M_LastCheckSeconds = -1.0;

	bool GetIsPlayerUnitInRadius(const UWorld& World, const FVector& Centre) const;
};
