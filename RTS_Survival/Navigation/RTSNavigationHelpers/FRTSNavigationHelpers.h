#pragma once
#include "CoreMinimal.h"
enum class ERTSNavAgents : uint8;
struct FAIMoveRequest;
class IRTSNavAgent;

/**
 * @brief Centralizes navigation policies shared by RTS unit AI controllers.
 */
class FRTSNavigationHelpers
{
public:
	static void SetupNavDataForTypeOnPawn(UWorld* World, APawn* InPawn);

	/**
	 * @brief Allows units to approach an unreachable goal instead of rejecting the move before pathfinding.
	 * @param MoveRequest Request to configure for closest-reachable movement.
	 */
	static void ConfigureMoveRequestForPartialPathFinding(FAIMoveRequest& MoveRequest);

private:
	/**
    	 * @brief Apply the default nav query filter to the pawn's AIController, based on its ERTSNavAgents type.
    	 * @param World Owning world (used for context/logging).
    	 * @param InPawn Target pawn (must be possessed by an AAIController to be effective).
    	 * @param AgentType Agent enum previously validated (not NONE).
    	 */
	static void SetupDefaultFilterForTypeOnPawn(UWorld* World, APawn* InPawn, ERTSNavAgents AgentType);
};

