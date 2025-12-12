#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftSocketState/AircraftSocketState.h"

#include "SocketRecord.generated.h"
enum class EAircraftLandingState : uint8;
enum class EAircraftSubtype : uint8;
class AAircraftMaster;
/** Record per "Landing_*" socket we support. */
USTRUCT()
struct FSocketRecord
{
	GENERATED_BODY()

	/** Logical state of the bay door. */
	EAircraftSocketState SocketState = EAircraftSocketState::Closed;

	/** The aircraft assigned to this bay (alive or out flying). */
	TWeakObjectPtr<AAircraftMaster> AssignedAircraft;

	/** True if an aircraft is physically inside the bay (landed/parked). */
	bool bM_IsInside = false;

	/** True if this socket is reserved by the training queue (before the unit exists). */
	bool bM_ReservedByTraining = false;

	/** Subtype reserved by training (only valid if bM_ReservedByTraining == true). */
	EAircraftSubtype ReservedType = static_cast<EAircraftSubtype>(0);

	/** Name of the Landing_* socket on the building mesh. */
	FName SocketName = NAME_None;

	/** Pending request flags to fulfill once door finishes opening. */
	bool bM_PendingVTO = false;

	bool bM_PendingLanding = false;

	/** Timer that flips Opening->Open and signals aircraft. */
	FTimerHandle AnimTimerHandle;

	FString GetDebugStr();
};

USTRUCT()
struct FAircraftRecordPacking
{
	GENERATED_BODY()

	// The aircraft associated with this base when we started packing up.
	TWeakObjectPtr<AAircraftMaster> AssignedAircraft;

	// Whether this aircraft was landing or landed when we attempted to pack up.
	// If so we land the aircraft immediately when we cancel the pack up.
	bool bLandAircraftIfPackUpCancelled = false;

	int32 SocketIndex = -1;

	EAircraftSocketState PreviousSocketState = EAircraftSocketState::None;
};

UENUM()
enum class EAirbasePackingState :uint8
{
	Unpacked_Building,
	PackingUp,
	PackedUp_Vehicle
};
