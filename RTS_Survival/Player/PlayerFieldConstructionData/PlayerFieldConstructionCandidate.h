#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"
#include "RTS_Survival/Interfaces/Commands.h"


#include "PlayerFieldConstructionCandidate.generated.h"

USTRUCT()
struct FFieldConstructionCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> FieldConstructionActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UFieldConstructionAbilityComponent> FieldConstructionAbilityComponent = nullptr;

	// Actors that failed (dead, cooldown, etc.) are cached here so we can skip them next selection pass.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ExcludedCandidateActors;

	inline void Reset()
	{
		FieldConstructionAbilityComponent = nullptr;
		FieldConstructionActor = nullptr;
		ExcludedCandidateActors.Reset();
	}

	inline void AddExcludedCandidateActor(AActor* const ActorToExclude)
	{
		if (not IsValid(ActorToExclude))
		{
			return;
		}

		ExcludedCandidateActors.RemoveAll([](const TWeakObjectPtr<AActor>& CachedActor)
		{
			return not CachedActor.IsValid();
		});

		if (GetIsExcludedCandidateActor(ActorToExclude))
		{
			return;
		}

		ExcludedCandidateActors.Add(ActorToExclude);
	}

	inline bool GetIsExcludedCandidateActor(AActor* const CandidateActor) const
	{
		if (not IsValid(CandidateActor))
		{
			return false;
		}

		for (const TWeakObjectPtr<AActor>& ExcludedCandidateActor : ExcludedCandidateActors)
		{
			if (ExcludedCandidateActor.Get() == CandidateActor)
			{
				return true;
			}
		}

		return false;
	}

	inline ICommands* GetCommandInterface() const
	{
		if (not FieldConstructionActor.IsValid())
		{
			return nullptr;
		}
		return Cast<ICommands>(FieldConstructionActor.Get());
	}

	bool IsAlive() const
	{
		if (not FieldConstructionActor.IsValid())
		{
			return false;
		}

		const ICommands* const CommandInterface = Cast<ICommands>(FieldConstructionActor.Get());
		if (CommandInterface == nullptr)
		{
			return false;
		}

		return RTSFunctionLibrary::RTSIsValid(FieldConstructionActor.Get()) && FieldConstructionAbilityComponent.IsValid();
	}
};
