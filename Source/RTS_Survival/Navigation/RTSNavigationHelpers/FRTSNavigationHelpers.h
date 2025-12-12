#pragma once
#include "CoreMinimal.h"
enum class ERTSNavAgents : uint8;
class IRTSNavAgent;

class FRTSNavigationHelpers
{
public:
	static void SetupNavDataForTypeOnPawn(UWorld* World, APawn* InPawn);

private:
	/**
    	 * @brief Apply the default nav query filter to the pawn's AIController, based on its ERTSNavAgents type.
    	 * @param World Owning world (used for context/logging).
    	 * @param InPawn Target pawn (must be possessed by an AAIController to be effective).
    	 * @param AgentType Agent enum previously validated (not NONE).
    	 */
	static void SetupDefaultFilterForTypeOnPawn(UWorld* World, APawn* InPawn, ERTSNavAgents AgentType);
};

