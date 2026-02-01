// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerOutlineComponent.h"

#include "RTSOutlineHelpers/FRTSOutlineHelpers.h"
#include "RTSOutlineRules/RTSOutlineRules.h"
#include "RTSOutlineTypes/RTSOutlineTypes.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbility.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UPlayerOutlineComponent::UPlayerOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPlayerOutlineComponent::InitPlayerOutlineComponent(const ERTSOutlineRules BaseOutLineRules)
{
	M_BaseOutLineRules = BaseOutLineRules;
	M_OutlineRules = M_BaseOutLineRules;
}

void UPlayerOutlineComponent::OnNewHoverActor(AActor* ActorHovered)
{
	// If the new hover actor differs or is null then hide our old outline actor.
	CheckHidePreviousOutlinedActor(ActorHovered);
	bool bIsSameActor = IsNewActorEqualToOldOutlinedActor(ActorHovered);
	if ((not IsValid(ActorHovered)) || bIsSameActor)
	{
		return;
	}
	const ERTSOutLineTypes OutLineType = GetOutLineForActor(ActorHovered);
	if (OutLineType == ERTSOutLineTypes::None)
	{
		return;
	}
	SetOutLineOnActor(ActorHovered, OutLineType);
	M_OutlinedActor = ActorHovered;
}

void UPlayerOutlineComponent::OnActionButtonActivated(const EAbilityID AbilityID)
{
	switch (AbilityID)
	{
	case EAbilityID::IdHarvestResource:
		M_OutlineRules = ERTSOutlineRules::RadixiteMetalOnly;
		break;
	case EAbilityID::IdScavenge:
		M_OutlineRules = ERTSOutlineRules::ScavengeOnly;
		break;
	case EAbilityID::IdCapture:
		M_OutlineRules = ERTSOutlineRules::CaptureOnly;
		break;
	case EAbilityID::IdAttack:
	case EAbilityID::IdAttackGround:
	case EAbilityID::IdAimAbility:
	case EAbilityID::IdAttachedWeapon:
		M_OutlineRules = ERTSOutlineRules::DoNotShowAnyOutlines;
		break;
	default:
		M_OutlineRules = M_BaseOutLineRules;
	}
}

void UPlayerOutlineComponent::OnActionButtonDeactivated(EAbilityID AbilityID)
{
	M_OutlineRules = M_BaseOutLineRules;
}


// Called when the game starts
void UPlayerOutlineComponent::BeginPlay()
{
	Super::BeginPlay();
}

ERTSOutLineTypes UPlayerOutlineComponent::GetOutLineForActor(AActor* ValidActor) const
{
	if (ValidActor->IsA(ACPPResourceMaster::StaticClass()))
	{
		ACPPResourceMaster* ResourceActor = Cast<ACPPResourceMaster>(ValidActor);
		if (not IsValid(ResourceActor))
		{
			return ERTSOutLineTypes::None;
		}
		UResourceComponent* ResourceComp = ResourceActor->GetResourceComponent();
		if (not IsValid(ResourceComp))
		{
			return ERTSOutLineTypes::None;
		}
		return GetOutLineForValidResource(ResourceComp, M_OutlineRules);
	}
	if (ValidActor->IsA(AScavengeableObject::StaticClass()))
	{
		AScavengeableObject* ScavengeObj = Cast<AScavengeableObject>(ValidActor);
		if (not IsValid(ScavengeObj))
		{
			return ERTSOutLineTypes::None;
		}
		return GetOutLineForScavengableObject(ScavengeObj, M_OutlineRules);
	}
	if (ValidActor->GetClass()->ImplementsInterface(UCaptureInterface::StaticClass()))
	{
		return GetOutLineForCapturableActor(ValidActor, M_OutlineRules);
	}
	return ERTSOutLineTypes::None;
}

ERTSOutLineTypes UPlayerOutlineComponent::GetOutLineForValidResource(const UResourceComponent* Resource,
                                                                     const ERTSOutlineRules Rules) const
{
	if (not Resource->StillContainsResources())
	{
		return ERTSOutLineTypes::None;
	}
	ERTSOutLineTypes ByResourceOutline = ERTSOutLineTypes::None;
	switch (Resource->GetResourceType())
	{
	case ERTSResourceType::Resource_Radixite:
		ByResourceOutline = ERTSOutLineTypes::Radixite;
		break;
	case ERTSResourceType::Resource_Metal:
		ByResourceOutline = ERTSOutLineTypes::Metal;
		break;
	default:
		ByResourceOutline = ERTSOutLineTypes::VehicleParts;
	}
	switch (Rules)
	{
	case ERTSOutlineRules::None:
		return ByResourceOutline;
	case ERTSOutlineRules::RadixiteMetalOnly:
		if (ByResourceOutline != ERTSOutLineTypes::Radixite && ByResourceOutline != ERTSOutLineTypes::Metal)
		{
			return ERTSOutLineTypes::None;
		}
	case ERTSOutlineRules::ScavengeOnly:
		return ERTSOutLineTypes::None;
	case ERTSOutlineRules::CaptureOnly:
		return ERTSOutLineTypes::None;
	case ERTSOutlineRules::DoNotShowAnyOutlines:
		return ERTSOutLineTypes::None;
	}
	return ByResourceOutline;
}

ERTSOutLineTypes UPlayerOutlineComponent::GetOutLineForScavengableObject(
	AScavengeableObject* ScavObject, const ERTSOutlineRules Rules) const
{
	if (not ScavObject->GetShowOutline())
	{
		return ERTSOutLineTypes::None;
	}
	const ERTSOutLineTypes ByScavengeOutline = ERTSOutLineTypes::VehicleParts;
	switch (Rules) {
	case ERTSOutlineRules::None:
	break;	
	case ERTSOutlineRules::RadixiteMetalOnly:
		return ERTSOutLineTypes::None;
	case ERTSOutlineRules::ScavengeOnly:
		break;
	case ERTSOutlineRules::CaptureOnly:
		return ERTSOutLineTypes::None;
	case ERTSOutlineRules::DoNotShowAnyOutlines:
		return ERTSOutLineTypes::None;
	}
	return ByScavengeOutline;
}

ERTSOutLineTypes UPlayerOutlineComponent::GetOutLineForCapturableActor(
	const AActor* CapturableActor, const ERTSOutlineRules Rules) const
{
	if (not IsValid(CapturableActor))
	{
		return ERTSOutLineTypes::None;
	}
	if (not GetIsCaptureOutlineAllowed())
	{
		return ERTSOutLineTypes::None;
	}
	if (Rules != ERTSOutlineRules::CaptureOnly)
	{
		return ERTSOutLineTypes::None;
	}
	return ERTSOutLineTypes::VehicleParts;
}

bool UPlayerOutlineComponent::GetIsCaptureOutlineAllowed() const
{
	const ACPPController* PlayerController = Cast<ACPPController>(GetOwner());
	if (not IsValid(PlayerController))
	{
		return false;
	}

	const bool bHasNoSelections = PlayerController->TSelectedSquadControllers.IsEmpty()
		&& PlayerController->TSelectedActorsMasters.IsEmpty()
		&& PlayerController->TSelectedPawnMasters.IsEmpty();
	if (bHasNoSelections)
	{
		return true;
	}

	const AGameUIController* GameUIController = PlayerController->GetGameUIController();
	if (not IsValid(GameUIController))
	{
		return false;
	}

	const AActor* PrimarySelectedActor = GameUIController->GetPrimarySelectedUnit();
	if (not IsValid(PrimarySelectedActor))
	{
		return false;
	}

	return PrimarySelectedActor->IsA(ASquadController::StaticClass());
}

void UPlayerOutlineComponent::CheckHidePreviousOutlinedActor(const AActor* NewActor)
{
	const AActor* OutlinedActor = M_OutlinedActor.Get();
	if (not IsValid(OutlinedActor))
	{
		return;
	}
	if (not IsValid(NewActor))
	{
		ResetActorOutline(OutlinedActor);
		M_OutlinedActor = nullptr;
		return;
	}
	if (OutlinedActor != NewActor)
	{
		ResetActorOutline(OutlinedActor);
		M_OutlinedActor = nullptr;
	}
}

bool UPlayerOutlineComponent::IsNewActorEqualToOldOutlinedActor(const AActor* NewActor) const
{
	const AActor* OutlinedActor = M_OutlinedActor.Get();
	if (not IsValid(OutlinedActor))
	{
		return false;
	}
	return OutlinedActor == NewActor;
}

void UPlayerOutlineComponent::SetOutLineOnActor(const AActor* ActorToOutLine, const ERTSOutLineTypes OutLineType)
{
	TArray<UPrimitiveComponent*> PrimitiveComps;
	ActorToOutLine->GetComponents<UPrimitiveComponent>(PrimitiveComps);
	for (UPrimitiveComponent* PrimComp : PrimitiveComps)
	{
		if (not IsValid(PrimComp))
		{
			continue;
		}
		FRTSOutlineHelpers::SetRTSOutLineOnComponent(PrimComp, OutLineType);
	}
}

void UPlayerOutlineComponent::ResetActorOutline(const AActor* ActorToReset)
{
	TArray<UPrimitiveComponent*> PrimitiveComps;
	ActorToReset->GetComponents<UPrimitiveComponent>(PrimitiveComps);
	for (UPrimitiveComponent* PrimComp : PrimitiveComps)
	{
		if (not IsValid(PrimComp))
		{
			continue;
		}
		FRTSOutlineHelpers::SetRTSOutLineOnComponent(PrimComp, ERTSOutLineTypes::None);
	}
}
