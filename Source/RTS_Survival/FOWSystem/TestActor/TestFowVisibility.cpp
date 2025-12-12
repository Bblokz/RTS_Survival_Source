// Copyright (C) Bas Blokzijl - All rights reserved.


#include "TestFowVisibility.h"

#include <RTS_Survival/FOWSystem/FowComponent/FowType.h>

#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values
ATestFowVisibility::ATestFowVisibility(const FObjectInitializer& ObjectInitializer): DrawTextLocation(),
	FowComponent(nullptr)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void ATestFowVisibility::OnFowVisibilityUpdated(const float Visibility)
{
	// Draw debug text at the location.
	if (Visibility > 0.15)
	{
		SetActorHiddenInGame(false);
		return;
	}
	SetActorHiddenInGame(true);
}

// Called when the game starts or when spawned
void ATestFowVisibility::BeginPlay()
{
	Super::BeginPlay();
}

void ATestFowVisibility::BeginDestroy()
{
	
	Super::BeginDestroy();
	
}

void ATestFowVisibility::PostInitializeComponents()
{
	Super::PostInitializeComponents();
		FowComponent = NewObject<UFowComp>(this, TEXT("FowComponent"));
		if (IsValid(FowComponent))
		{
			FowComponent->StartFow(EFowBehaviour::Fow_Passive, false);
		}
}
