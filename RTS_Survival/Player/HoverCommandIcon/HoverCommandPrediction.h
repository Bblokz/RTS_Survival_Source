#pragma once

#include "CoreMinimal.h"

class AActor;
class UPlayerCommandTypeDecoder;
enum class ERTSVerticalAnimatedIcon : uint8;

/**
 * @brief Mirrors the secondary-click dispatch of ACPPController for the primary selected unit only, so the cursor
 * can show which targeted command a click would issue. Movement and every fallback to movement predict None.
 */
namespace FHoverCommandPrediction
{
	/**
	 * @brief Keeps the cursor preview in step with the click dispatch without issuing anything.
	 * @param CommandTypeDecoder The controller's decoder; the same one used when the click is executed.
	 * @param HoveredActor Actor under the cursor after the child-actor redirect the click also applies.
	 * @param PrimarySelectedUnit Unit whose abilities decide the command.
	 * @return The command icon, or None when the click would move or do nothing targeted.
	 */
	ERTSVerticalAnimatedIcon PredictCommandIcon(
		const UPlayerCommandTypeDecoder& CommandTypeDecoder,
		AActor* HoveredActor,
		AActor* PrimarySelectedUnit);
}
