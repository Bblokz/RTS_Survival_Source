// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/RTSComponents/AOEBehaviourComponent/RadiusAOEBehaviourComponent/RadiusAOEBehaviourComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/Derived/OnHoverShowRadiusBehaviour/OnHoverShowRadiusBehaviour.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URadiusAOEBehaviourComponent::URadiusAOEBehaviourComponent()
{
	RadiusSettings.HoverShowRadiusBehaviourClass = UOnHoverShowRadiusBehaviour::StaticClass();
}

void URadiusAOEBehaviourComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_CacheRadiusSubsystem();
	BeginPlay_CacheBehaviourComponent();
	BeginPlay_CacheSelectionComponent();
	BeginPlay_SetupHostBehaviour();
	BeginPlay_SetupSelectionBindings();
}

void URadiusAOEBehaviourComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideRadius();

	Super::EndPlay(EndPlayReason);
}

void URadiusAOEBehaviourComponent::OnHostBehaviourHovered(const bool bIsHovering)
{
	if (not GetShouldShowRadiusOnHover())
	{
		return;
	}

	if (bIsHovering)
	{
		ShowRadius();
		return;
	}

	HideRadius();
}

void URadiusAOEBehaviourComponent::SetHostBehaviourUIData(UBehaviour& Behaviour) const
{
	FBehaviourUIData UIData = Behaviour.GetUIData();
	UIData.TitleText = RadiusSettings.HostBehaviourText;
	UIData.BehaviourIcon = RadiusSettings.HostBehaviourIcon;
	Behaviour.SetCustomUIData(UIData);
}

void URadiusAOEBehaviourComponent::BeginPlay_CacheRadiusSubsystem()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_RadiusPoolSubsystem = World->GetSubsystem<URTSRadiusPoolSubsystem>();
	(void)GetIsValidRadiusSubsystem();
}

void URadiusAOEBehaviourComponent::BeginPlay_CacheBehaviourComponent()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	M_BehaviourComponent = Owner->FindComponentByClass<UBehaviourComp>();
	(void)GetIsValidBehaviourComponent();
}

void URadiusAOEBehaviourComponent::BeginPlay_CacheSelectionComponent()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	M_SelectionComponent = Owner->FindComponentByClass<USelectionComponent>();
}

void URadiusAOEBehaviourComponent::BeginPlay_SetupHostBehaviour()
{
	if (not GetIsValidBehaviourComponent())
	{
		return;
	}

	if (not RadiusSettings.HoverShowRadiusBehaviourClass)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("URadiusAOEBehaviourComponent: HoverShowRadiusBehaviourClass is null."));
		return;
	}

	M_BehaviourComponent->AddBehaviour(RadiusSettings.HoverShowRadiusBehaviourClass);

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const TWeakObjectPtr<URadiusAOEBehaviourComponent> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			if (not WeakThis->GetIsValidBehaviourComponent())
			{
				return;
			}

			UBehaviour* Behaviour = WeakThis->M_BehaviourComponent->GetBehaviourByClass(
				WeakThis->RadiusSettings.HoverShowRadiusBehaviourClass);
			if (not IsValid(Behaviour))
			{
				return;
			}

			WeakThis->SetHostBehaviourUIData(*Behaviour);
		});
}

void URadiusAOEBehaviourComponent::BeginPlay_SetupSelectionBindings()
{
	if (not GetShouldShowRadiusOnSelection())
	{
		return;
	}

	USelectionComponent* SelectionComponent = M_SelectionComponent;
	if (not IsValid(SelectionComponent))
	{
		return;
	}

	const TWeakObjectPtr<URadiusAOEBehaviourComponent> WeakThis(this);
	SelectionComponent->OnUnitSelected.AddLambda(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->ShowRadius();
		});

	SelectionComponent->OnUnitDeselected.AddLambda(
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->HideRadius();
		});
}

void URadiusAOEBehaviourComponent::ShowRadius()
{
	if (M_RadiusId > 0)
	{
		return;
	}

	if (not GetIsValidRadiusSubsystem())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	const float Radius = GetAOERadius();
	if (Radius <= 0.f)
	{
		return;
	}

	M_RadiusId = M_RadiusPoolSubsystem->CreateRTSRadius(
		Owner->GetActorLocation(),
		Radius,
		RadiusSettings.RadiusType);

	if (M_RadiusId <= 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("URadiusAOEBehaviourComponent::ShowRadius: Failed to create RTS radius."));
		return;
	}

	if (RadiusSettings.bMoveRadius)
	{
		M_RadiusPoolSubsystem->AttachRTSRadiusToActor(
			M_RadiusId,
			Owner,
			RadiusSettings.RadiusRelativeAttachmentOffset);
	}
}

void URadiusAOEBehaviourComponent::HideRadius()
{
	if (M_RadiusId <= 0)
	{
		return;
	}

	if (not GetIsValidRadiusSubsystem())
	{
		return;
	}

	M_RadiusPoolSubsystem->HideRTSRadiusById(M_RadiusId);
	M_RadiusId = InvalidRadiusId;
}

bool URadiusAOEBehaviourComponent::GetShouldShowRadiusOnHover() const
{
	switch (RadiusSettings.ShowRadiusType)
	{
	case EAOeBehaviourShowRadiusType::OnHover:
	case EAOeBehaviourShowRadiusType::OnHoverAndOnSelection:
		return true;
	default:
		return false;
	}
}

bool URadiusAOEBehaviourComponent::GetShouldShowRadiusOnSelection() const
{
	switch (RadiusSettings.ShowRadiusType)
	{
	case EAOeBehaviourShowRadiusType::OnSelectionOnly:
	case EAOeBehaviourShowRadiusType::OnHoverAndOnSelection:
		return true;
	default:
		return false;
	}
}

bool URadiusAOEBehaviourComponent::GetIsValidRadiusSubsystem() const
{
	if (M_RadiusPoolSubsystem.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_RadiusPoolSubsystem",
		"GetIsValidRadiusSubsystem",
		GetOwner());
	return false;
}

bool URadiusAOEBehaviourComponent::GetIsValidBehaviourComponent() const
{
	if (IsValid(M_BehaviourComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_BehaviourComponent",
		"GetIsValidBehaviourComponent",
		GetOwner());
	return false;
}
