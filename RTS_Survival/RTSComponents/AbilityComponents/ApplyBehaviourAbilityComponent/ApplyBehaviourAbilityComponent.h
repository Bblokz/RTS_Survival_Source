// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ApplyBehaviourAbilityComponent.generated.h"

class ASquadController;
class UBehaviour;
class ICommands;
class UBehaviourComp;
enum class EBehaviourAbilityType : uint8;

USTRUCT(Blueprintable)
struct FApplyBehaviourAbilitySettings
{
	GENERATED_BODY()

	// Determines what type of ability this actually is (base is EAbilityID::IdApplyBehaviour).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBehaviourAbilityType BehaviourAbility;

	// Attempts to add the abilty to this index of the Unit's Ability Array.
	// Reverts to first empty index if negative or already occupied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PreferredAbilityIndex = INDEX_NONE;

	// How long the ability is on cooldown after use. (does not affect behaviour duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cooldown = 5;

	// Behaviour applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UBehaviour> BehaviourApplied;
};

// THe done executing is done in the icommands as this ability is instantly complete.
// Adds an abilty to the command card of the unit with a specfic sub type of this ability set with EBehaviourAbilityType.
// execute is adding the behaviour to the unit's behaviour component.
// Terminate is no-op.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UApplyBehaviourAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UApplyBehaviourAbilityComponent();

	void ExecuteBehaviourAbility() const;
	void TerminateBehaviourAbility() const;

	EBehaviourAbilityType GetBehaviourAbilityType() const;

protected:
	virtual void PostInitProperties() override;
	// Called when the game starts
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FApplyBehaviourAbilitySettings BehaviourAbilitySettings;

private:
	TWeakObjectPtr<UBehaviourComp> M_OwnerBehaviourComponent;
	bool GetIsValidOwnerBehaviourComponent() const;
	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;
	bool GetIsValidOwnerCommandsInterface() const;

	FString GetDebugName() const;
	void BeginPlay_CheckSettings() const;

	void BeginPlay_AddAbility();
	void AddAbilityToSquad(ASquadController* Squad);
	void AddAbilityToCommands();
	

};
