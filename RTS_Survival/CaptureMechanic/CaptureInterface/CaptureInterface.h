#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CaptureInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UCaptureInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by actors that can be captured by squads.
 */
class RTS_SURVIVAL_API ICaptureInterface
{
	GENERATED_BODY()

public:

	/** @return Number of squad units required to start capturing this actor. */
	virtual int32 GetCaptureUnitAmountNeeded() const = 0;

	/**
	 * Called when a squad has committed the required number of units to start capturing this actor.
	 * Implementations are expected to start any capture-progress logic (timers, UI, etc.).
	 * @param Player Index of the player that started the capture.
	 */
	virtual void OnCaptureByPlayer(const int32 Player) = 0;

	/**
	 * @brief Returns the world-space location that is closest to FromLocation
	 *        for units to move to when starting a capture.
	 */
	virtual FVector GetCaptureLocationClosestTo(const FVector& FromLocation) = 0;
};
