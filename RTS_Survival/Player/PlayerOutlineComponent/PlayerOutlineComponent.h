// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerOutlineComponent.generated.h"


enum class EAbilityID : uint8;
enum class ERTSOutlineRules : uint8;
class AScavengeableObject;
class UResourceComponent;
enum class ERTSOutLineTypes : uint8;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerOutlineComponent();

	// Determine how the outline works by default.
	void InitPlayerOutlineComponent(ERTSOutlineRules BaseOutLineRules);
	void OnNewHoverActor(AActor* ActorHovered);
	void OnActionButtonActivated(EAbilityID AbilityID);
	void OnActionButtonDeactivated(EAbilityID AbilityID);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	

private:
	ERTSOutlineRules M_OutlineRules;
	// Defines what outline rules to use when no ability is active.
	ERTSOutlineRules M_BaseOutLineRules;

	ERTSOutLineTypes GetOutLineForActor(AActor* ValidActor) const;
	ERTSOutLineTypes GetOutLineForValidResource(const UResourceComponent* Resource, const ERTSOutlineRules Rules) const;
	ERTSOutLineTypes GetOutLineForScavengableObject(AScavengeableObject* ScavObject, const ERTSOutlineRules Rules) const;
	ERTSOutLineTypes GetOutLineForCapturableActor(const AActor* CapturableActor,
	                                              const ERTSOutlineRules Rules) const;
	UPROPERTY()
	TWeakObjectPtr<AActor> M_OutlinedActor;
	void CheckHidePreviousOutlinedActor(const AActor* NewActor);
	bool IsNewActorEqualToOldOutlinedActor(const AActor* NewActor) const;

	void SetOutLineOnActor(const AActor* ActorToOutLine, const ERTSOutLineTypes OutLineType);
	void ResetActorOutline(const AActor* ActorToReset);

};
