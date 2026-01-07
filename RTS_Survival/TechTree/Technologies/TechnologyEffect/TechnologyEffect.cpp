#include "TechnologyEffect.h"
#include "Async/Async.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTechnologyEffect::ApplyTechnologyEffect(const UObject* WorldContextObject)
{
	// Perform the actor search synchronously
	FindActors(WorldContextObject);
}

void UTechnologyEffect::OnApplyEffectToActor(AActor* ValidActor)
{
}

void UTechnologyEffect::FindActors(const UObject* WorldContextObject)
{
	const TSet<TSubclassOf<AActor>> Classes = GetTargetActorClasses();
	if (Classes.IsEmpty())
	{
		RTSFunctionLibrary::PrintString("UTechnologyEffect::FindActors - No target class specified.");
		return;
	}

	const UWorld* World = GEngine->GetWorldFromContextObjectChecked(WorldContextObject);
	if (!World)
	{
		RTSFunctionLibrary::PrintString("UTechnologyEffect::FindActors - No world found.");
		return;
	}

	// Collect actors of the specified class
	TArray<AActor*> FoundActors;
	for (const auto EachClass : Classes)
	{
		if(!EachClass)
		{
			RTSFunctionLibrary::PrintString("UTechnologyEffect::FindActors - Invalid class specified.");
			continue;
		}
		for (TActorIterator<AActor> It(World, EachClass); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && IsValid(Actor))
			{
				FoundActors.Add(Actor);
			}
		}
	}

	// Call the child class's OnActorsFound function
	OnActorsFound(FoundActors);
}

void UTechnologyEffect::OnActorsFound(const TArray<AActor*>& FoundActors)
{
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		const FString TechName = GetName();
		for (const auto Each : FoundActors)
		{
			RTSFunctionLibrary::PrintString("Found actor: " + Each->GetName() + " for tech: " + TechName);
		}
	}
	for (const auto Each : FoundActors)
	{
		OnApplyEffectToActor(Each);
	}
}
