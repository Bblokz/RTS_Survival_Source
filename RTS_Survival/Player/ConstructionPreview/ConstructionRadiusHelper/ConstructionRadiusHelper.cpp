// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "ConstructionRadiusHelper.h"

#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/ExpansionRadius/ExpansionRadiusComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


// Sets default values
AConstructionRadiusHelper::AConstructionRadiusHelper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	ExpansionRadiusComp = CreateDefaultSubobject<UExpansionRadiusComp>(TEXT("ExpansionRadiusComp"));
}

URadiusComp* AConstructionRadiusHelper::GetRadiusComp()
{
	if(GetIsValidExpansionRadiusComp())
	{
		return ExpansionRadiusComp;
	}
	return nullptr;
}

void AConstructionRadiusHelper::SetConstructionRadius(float Radius)
{	
	if(GetIsValidExpansionRadiusComp())
	{
		ExpansionRadiusComp->UpdateRadius(Radius);
	}	
}

void AConstructionRadiusHelper::SetRadiusVisibility(bool bVisible)
{
	if(GetIsValidExpansionRadiusComp())
	{
		bVisible ? ExpansionRadiusComp->ShowRadius() : ExpansionRadiusComp->HideRadius();
	}
}

// Called when the game starts or when spawned
void AConstructionRadiusHelper::BeginPlay()
{
	Super::BeginPlay();
	
}

bool AConstructionRadiusHelper::GetIsValidExpansionRadiusComp()
{
	if(IsValid(ExpansionRadiusComp))
	{
		return true;
	}
	ExpansionRadiusComp = FindComponentByClass<UExpansionRadiusComp>();
	if(IsValid(ExpansionRadiusComp))
	{
		return true;
	}
	ExpansionRadiusComp = NewObject<UExpansionRadiusComp>(this);
	if(IsValid(ExpansionRadiusComp))
	{
		ExpansionRadiusComp->RegisterComponent();
		return true;
	}
	RTSFunctionLibrary::ReportError("Could not repair expansion radius comp on Construction Radius helper"
		"\n Make sure the expansion radius is valid on : " + GetName());
	return false;
}

