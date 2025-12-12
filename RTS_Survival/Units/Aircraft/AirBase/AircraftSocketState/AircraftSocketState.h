#pragma once


#include "CoreMinimal.h"

#include "AircraftSocketState.generated.h"


// The amount of aircraft supported by an airbase is defined by the number of aircraft sockets.
// Each Aircraft socket can play animations on a skeletal mesh to open and or close the bay the aircraft resides in
UENUM(BlueprintType)
enum class EAircraftSocketState: uint8
{
	None,
	Open,
	// Playing montage from Closed to Open
	Opening,
	Closed,
	// Playing montage from Open to Closed
	Closing
};