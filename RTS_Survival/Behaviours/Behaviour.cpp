// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Behaviour.h"
#include "BehaviourComp.h"
#include "GameFramework/Actor.h"

UBehaviour::UBehaviour()
{
	RefreshLifetime();
}

void UBehaviour::OnAdded(AActor* BehaviourOwner)
{
	BP_OnAdded(BehaviourOwner);
}

void UBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	BP_OnRemoved(BehaviourOwner);
}

void UBehaviour::OnTick(const float DeltaTime)
{
}

void UBehaviour::OnBehaviorHover(const bool bIsHovering)
{
	static_cast<void>(bIsHovering);
}

void UBehaviour::SetCustomUIData(const FBehaviourUIData& UIData)
{
	BehaviourIcon = UIData.BehaviourIcon;
	M_DisplayText = UIData.DescriptionText;
	M_TitleText = UIData.TitleText;
	M_BuffType = UIData.BuffDebuffType;
	BehaviourLifeTime = UIData.LifeTimeType;
	M_LifeTimeDuration = UIData.TotalLifeTime;
}

void UBehaviour::GetUIData(FBehaviourUIData& OutUIData) const
{
	OutUIData.BehaviourIcon = BehaviourIcon;
	OutUIData.DescriptionText = M_DisplayText;
	OutUIData.TitleText = M_TitleText;
	OutUIData.BuffDebuffType = M_BuffType;
	OutUIData.LifeTimeType = BehaviourLifeTime;
	OutUIData.TotalLifeTime = M_LifeTimeDuration;
}

FBehaviourUIData UBehaviour::GetUIData() const
{
	FBehaviourUIData UIData;
	UIData.BehaviourIcon = BehaviourIcon;
	UIData.DescriptionText = M_DisplayText;
	UIData.TitleText = M_TitleText;
	UIData.BuffDebuffType = M_BuffType;
	UIData.LifeTimeType = BehaviourLifeTime;
	UIData.TotalLifeTime = M_LifeTimeDuration;
	return UIData;
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

const FRepeatedBehaviourTextSettings& UBehaviour::GetAnimatedTextSettings() const
{
	return AnimatedTextSettings;
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
