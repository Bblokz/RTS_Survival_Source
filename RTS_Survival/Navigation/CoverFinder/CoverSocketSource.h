#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CoverSocketSource.generated.h"

class ASquadUnit;

/**
 * @brief One named cover position handed to the cover finder by an actor that keeps its own list of them.
 * The name decides the kind of cover the same way a mesh socket name does (cargo, crouch, standing, prone).
 */
struct FRTSActorCoverSocket
{
	FName SocketName;
	// Where the soldier holds, looking along the forward axis.
	FTransform WorldTransform = FTransform::Identity;
	// A spot beside the navigation mesh from which the soldier steps onto the socket and to which he returns;
	// the socket itself may be inside the actor, where no navigation mesh exists.
	FVector ApproachLocation = FVector::ZeroVector;
};

UINTERFACE(MinimalAPI)
class URTSCoverSocketSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * @brief For actors whose cover cannot be read from mesh sockets, such as a destructible building made of one
 * geometry collection: list the class under "Cover From Actor Sockets" in the cover settings and implement this.
 * Call URTSCoverFinderWorldSubsystem::RefreshCoverSocketsOfActor whenever the usable sockets change.
 */
class RTS_SURVIVAL_API IRTSCoverSocketSource
{
	GENERATED_BODY()

public:
	/** @param OutCoverSockets Receives every socket a soldier can use right now; destroyed ones are left out. */
	virtual void GetUsableCoverSockets(TArray<FRTSActorCoverSocket>& OutCoverSockets) const = 0;

	/**
	 * @brief Called when a soldier is placed on one of this actor's sockets and again when he gives it up, so
	 * the actor can keep track of who is inside it.
	 * @param SquadUnit The soldier.
	 * @param bIsOnSocket True when he took the socket, false when he left it or died on it.
	 */
	virtual void OnSoldierCoverSocketChanged(ASquadUnit* SquadUnit, const bool bIsOnSocket)
	{
	}
};
