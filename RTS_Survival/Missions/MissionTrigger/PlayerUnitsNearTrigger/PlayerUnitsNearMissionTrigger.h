// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"

#include "PlayerUnitsNearMissionTrigger.generated.h"

/**
 * @brief Starts a mission once a player unit comes within Radius of a place, so a bonus objective's plate appears on
 * discovery instead of at map start. The place is the first actor carrying CentreActorTag, or CentreLocation.
 * Set it as the MissionTrigger of a mission in the manager's Missions array.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UPlayerUnitsNearMissionTrigger : public UMissionTrigger
{
	GENERATED_BODY()

public:
	virtual EMissionTriggerType GetTriggerType() const override { return EMissionTriggerType::PlayerUnitsNear; }

	// When set, the centre is the first level actor with this tag (it follows the actor if it moves).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FName CentreActorTag = NAME_None;

	// Used when CentreActorTag is None or no actor carries it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FVector CentreLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "100.0", Units = "cm"))
	float Radius = 5000.f;

	// The overlap runs this often, not every frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger", meta = (ClampMin = "0.05"))
	float CheckIntervalSeconds = 0.5f;

protected:
	virtual bool CheckTrigger_Implementation() const override;

private:
	mutable double M_LastCheckSeconds = -1.0;

	// Found once; the tag lookup walks every actor. Not a UPROPERTY: UHT rejects mutable, and a weak pointer needs no GC
	// reference.
	mutable TWeakObjectPtr<AActor> M_CentreActor = nullptr;

	mutable bool bM_HasSearchedCentreActor = false;

	bool GetShouldRunCheck(const UWorld& World) const;
	FVector GetCentre(const UWorld& World) const;
	bool GetIsPlayerUnitInRadius(const UWorld& World, const FVector& Centre) const;
};
