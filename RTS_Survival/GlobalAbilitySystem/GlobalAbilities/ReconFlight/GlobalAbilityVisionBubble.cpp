// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GlobalAbilityVisionBubble.h"

#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"

AGlobalAbilityVisionBubble::AGlobalAbilityVisionBubble()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	M_FowComponent = CreateDefaultSubobject<UFowComp>(TEXT("FowComponent"));
}

void AGlobalAbilityVisionBubble::StartVision(const float VisionRadius, const float Seconds, const uint8 OwningPlayer)
{
	const EFowBehaviour FowBehaviour = OwningPlayer == 1
		                                   ? EFowBehaviour::Fow_Active
		                                   : EFowBehaviour::Fow_PassiveEnemyVision;
	M_FowComponent->SetVisionRadius(VisionRadius);
	M_FowComponent->StartFow(FowBehaviour, true);
	SetLifeSpan(Seconds);
}

void AGlobalAbilityVisionBubble::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(M_FowComponent))
	{
		M_FowComponent->StopFowParticipation();
	}
	Super::EndPlay(EndPlayReason);
}
