// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Behaviour.h"
#include "BehaviourComp.h"
#include "GameFramework/Actor.h"

UBehaviour::UBehaviour()
{
	RefreshLifetime();
}

void UBehaviour::OnAdded()
{
}

void UBehaviour::OnRemoved()
{
}

void UBehaviour::OnTick(const float DeltaTime)
{
}

void UBehaviour::GetUIData(FBehaviourUIData& OutUIData) const
{
	OutUIData.BehaviourIcon = BehaviourIcon;
	OutUIData.DisplayText = M_DisplayText;
}

EBehaviourLifeTime UBehaviour::GetLifeTimeType() const
{
	return BehaviourLifeTime;
}

EBehaviourStackRule UBehaviour::GetStackRule() const
{
	return BehaviourStackRule;
}

int32 UBehaviour::GetMaxStackCount() const
{
	return M_MaxStackCount;
}

bool UBehaviour::UsesTick() const
{
	return bM_UsesTick;
}

bool UBehaviour::IsTimedBehaviour() const
{
	return BehaviourLifeTime == EBehaviourLifeTime::Timed;
}

void UBehaviour::InitializeBehaviour(UBehaviourComp* InOwningComponent)
{
	M_OwningComponent = InOwningComponent;
	RefreshLifetime();
}

void UBehaviour::RefreshLifetime()
{
	M_RemainingLifeTime = M_LifeTimeDuration;
}

void UBehaviour::AdvanceLifetime(const float DeltaTime)
{
	if (BehaviourLifeTime != EBehaviourLifeTime::Timed)
	{
		return;
	}
	
	M_RemainingLifeTime -= DeltaTime;
}

bool UBehaviour::HasExpired() const
{
	return BehaviourLifeTime == EBehaviourLifeTime::Timed && M_RemainingLifeTime <= 0.f;
}

bool UBehaviour::IsSameBehaviour(const UBehaviour& OtherBehaviour) const
{
	if (GetClass() != OtherBehaviour.GetClass())
	{
		return false;
	}
	
	return IsSameAs(&OtherBehaviour);
}

void UBehaviour::OnStack(UBehaviour* StackedBehaviour)
{
}

bool UBehaviour::IsSameAs(const UBehaviour* OtherBehaviour) const
{
        return OtherBehaviour != nullptr;
}

UBehaviourComp* UBehaviour::GetOwningBehaviourComp() const
{
        return M_OwningComponent.Get();
}

AActor* UBehaviour::GetOwningActor() const
{
        const UBehaviourComp* BehaviourComponent = GetOwningBehaviourComp();
        if (BehaviourComponent == nullptr)
        {
                return nullptr;
        }

        return BehaviourComponent->GetOwner();
}
