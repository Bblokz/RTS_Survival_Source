// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Lifetime/BehaviourLifeTime.h"
#include "StackRule/BehaviourStackRule.h"
#include "Icons/BehaviourIcon.h"
#include "UI/BehaviourUIData.h"

#include "Behaviour.generated.h"

class UBehaviourComp;
class AActor;

/**
* @brief Base behaviour class driving gameplay modifiers managed by BehaviourComp.
*/
UCLASS()
class RTS_SURVIVAL_API UBehaviour : public UObject
{
	GENERATED_BODY()

public:
	UBehaviour();

	virtual void OnAdded();
	virtual void OnRemoved();
	virtual void OnTick(const float DeltaTime);
	virtual void OnStack(UBehaviour* StackedBehaviour);


	/**
	* @brief Retrieve UI data for this behaviour.
	* @param OutUIData Struct that will be filled with UI friendly values.
	*/
	void GetUIData(FBehaviourUIData& OutUIData) const;

	EBehaviourLifeTime GetLifeTimeType() const;
	EBehaviourStackRule GetStackRule() const;
	int32 GetMaxStackCount() const;
	bool UsesTick() const;
	bool IsTimedBehaviour() const;

	void InitializeBehaviour(UBehaviourComp* InOwningComponent);
	void RefreshLifetime();
	void AdvanceLifetime(const float DeltaTime);
	bool HasExpired() const;
	bool IsSameBehaviour(const UBehaviour& OtherBehaviour) const;

protected:
	virtual bool IsSameAs(const UBehaviour* OtherBehaviour) const;

	UBehaviourComp* GetOwningBehaviourComp() const;
	AActor* GetOwningActor() const;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour")
	EBehaviourLifeTime BehaviourLifeTime = EBehaviourLifeTime::None;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour")
	EBehaviourStackRule BehaviourStackRule = EBehaviourStackRule::None;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour")
	int32 M_MaxStackCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour")
	bool bM_UsesTick = false;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour|UI")
	EBehaviourIcon BehaviourIcon = EBehaviourIcon::None;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour|UI")
	FString M_DisplayText;

	UPROPERTY(EditDefaultsOnly, Category = "Behaviour")
	float M_LifeTimeDuration = 0.f;

private:
	UPROPERTY()
	TWeakObjectPtr<UBehaviourComp> M_OwningComponent;

	float M_RemainingLifeTime = 0.f;
};
