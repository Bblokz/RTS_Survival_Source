
#include "FRTSNavigationHelpers.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
// include the pawn movement component  for casting.
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/NavMovementComponent.h"
#include "RTS_Survival/Navigation/NavQueryFilterSettings/RTSNavQueryFilterSettings.h"

#include "RTS_Survival/Navigation/RTSNavAgentRegistery/RTSNavAgentRegistery.h"
#include "RTS_Survival/Navigation/RTSNavAgents/IRTSNavAgent/IRTSNavAgent.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"


void FRTSNavigationHelpers::SetupNavDataForTypeOnPawn(UWorld* World, APawn* InPawn)
{
	if (not World || not InPawn)
	{
		return;
	}

	URTSNavAgentRegistry* Reg = URTSNavAgentRegistry::Get(InPawn);
	if (not IsValid(Reg))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(InPawn, "URTSNavAgentRegistry",
			"FRTSNavigationHelpers::SetupNavDataForTypeOnPawn");
		return;
	}

	IRTSNavAgentInterface* NavAgent = Cast<IRTSNavAgentInterface>(InPawn);
	if (not NavAgent)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("FRTSNavigationHelpers::SetupNavDataForTypeOnPawn: Pawn does not implement IRTSNavAgent interface."));
		return;
	}

	const ERTSNavAgents AgentType = NavAgent->GetRTSNavAgentType();
	if (AgentType == ERTSNavAgents::NONE)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("FRTSNavigationHelpers::SetupNavDataForTypeOnPawn: Pawn's GetRTSNavAgentType returned NONE."));
		return;
	}

	const FNavDataConfig* const AgentCfg = Reg->FindAgentConfig(World, AgentType);
	if (not AgentCfg)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("FRTSNavigationHelpers::SetupNavDataForTypeOnPawn: Could not find NavDataConfig for Pawn's agent type.\n Name of type:")
			+ Global_GetRTSNavAgentName(AgentType));
		return;
	}

	// Override the pawn's NavAgentProps to the character's config.
	if (UNavMovementComponent* const NavComp = Cast<UNavMovementComponent>(InPawn->GetMovementComponent()))
	{
		NavComp->NavAgentProps = static_cast<const FNavAgentProperties&>(*AgentCfg);
		NavComp->SetUpdateNavAgentWithOwnersCollisions(false);
	}

	// Ensure default query filter is set on its controller so all future pathfinding uses it automatically.
	SetupDefaultFilterForTypeOnPawn(World, InPawn, AgentType);
}

void FRTSNavigationHelpers::SetupDefaultFilterForTypeOnPawn(UWorld* World, APawn* InPawn, const ERTSNavAgents AgentType)
{
	if (not World || not InPawn)
	{
		return;
	}

	// Controller must be AI for DefaultNavigationFilterClass to apply to MoveTo, BT tasks, etc.
	AAIController* const AICon = Cast<AAIController>(InPawn->GetController());
	if (not AICon)
	{
		// Not an error in all cases (e.g., not possessed yet), but worth a breadcrumb in logs while integrating.
		RTSFunctionLibrary::ReportError(TEXT("SetupDefaultFilterForTypeOnPawn: Pawn is not possessed by an AAIController."));
		return;
	}

	const URTSNavQueryFilterSettings* const Settings = URTSNavQueryFilterSettings::Get();
	if (not Settings)
	{
		RTSFunctionLibrary::ReportError(TEXT("SetupDefaultFilterForTypeOnPawn: URTSNavQueryFilterSettings not found."));
		return;
	}

	const TSubclassOf<UNavigationQueryFilter> FilterClass = Settings->ResolveFilterClassForAgent(AgentType);
	if (not *FilterClass)
	{
		// No mapping configured: do nothing; controller/NavData defaults will be used.
		return;
	}
	IRTSNavAIInterface* NavAI = Cast<IRTSNavAIInterface>(AICon);
	if (not NavAI)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("SetupDefaultFilterForTypeOnPawn: AIController does not implement IRTSNavAI interface."));
		return;
	}

	// This property is the source of truth for default pathfinding filters on AAIController.
	// All MoveTo* calls (and BT MoveTo) fall back to this when no FilterClass is provided. (See UE docs.)
	NavAI->SetDefaultQueryFilter(FilterClass);

}
