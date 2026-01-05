#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"

USTRUCT()
struct FFieldConstructionCandidate
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> FieldConstructionActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UFieldConstructionAbilityComponent> FieldConstructionAbilityComponent = nullptr;

	// Actors that failed (dead, cooldown, etc.) get their GUID cached here so we can skip them next selection pass.
	UPROPERTY()
	TArray<FGuid> ExcludedCandidateActorGuids;

	inline void Reset()
	{
		FieldConstructionAbilityComponent = nullptr;
		FieldConstructionActor = nullptr;
		ExcludedCandidateActorGuids.Reset();
	}

	inline void AddExcludedCandidateActor(AActor* const ActorToExclude)
	{
		if (not IsValid(ActorToExclude))
		{
			return;
		}

		const FGuid ActorGuid = ActorToExclude->GetActorGuid();
		if (not ActorGuid.IsValid())
		{
			return;
		}

		ExcludedCandidateActorGuids.AddUnique(ActorGuid);
	}

	inline bool GetIsExcludedCandidateActor(AActor* const CandidateActor) const
	{
		if (not IsValid(CandidateActor))
		{
			return false;
		}

		const FGuid ActorGuid = CandidateActor->GetActorGuid();
		if (not ActorGuid.IsValid())
		{
			return false;
		}

		return ExcludedCandidateActorGuids.Contains(ActorGuid);
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
