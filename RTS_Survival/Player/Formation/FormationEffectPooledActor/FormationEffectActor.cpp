// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FormationEffectActor.h"

#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


AFormationEffectActor::AFormationEffectActor(const FObjectInitializer& ObjectInitializer)
	: AActorObjectsMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AFormationEffectActor::SetFormationEffectActivated(const bool bActivate, const EFormationEffect EffectType)
{
	if (not EnsureNiagaraCompIsValid())
	{
		SetActorHiddenInGame(true);
		return;
	}
	if (bActivate)
	{
		TArray<FFormationEffectColorParam> ColorParams = {};
		SetActorHiddenInGame(false);
		UNiagaraSystem* EffectSystem = GetValidEffectFromMap(EffectType, ColorParams);
		if (EffectSystem)
		{
			M_NiagaraComp->SetAsset(EffectSystem);
			M_NiagaraComp->Activate(true);
			ApplyColorParams(ColorParams);
		}
		return;
	}
	SetActorHiddenInGame(true);
}

void AFormationEffectActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	M_NiagaraComp = FindComponentByClass<UNiagaraComponent>();
	// Error check.
	(void)EnsureNiagaraCompIsValid();
}

void AFormationEffectActor::BeginPlay()
{
	Super::BeginPlay();
	if (FormationPositionEffects.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("No effects set in the array of the FormationEffectActor!"
			"\n Check defaults!");
		return;
	}
	for (const FFormationPositionEffect& Effect : FormationPositionEffects)
	{
		if (IsValid(Effect.EffectSystem))
		{
			M_EffectsMap.Add(Effect.EffectType, Effect);
			continue;
		}
		RTSFunctionLibrary::ReportError("Invalid effect system in FormationEffectActor for effect type: " +
			UEnum::GetValueAsString(Effect.EffectType) +
			"\n Check defaults of FormationEffectActor!");
	}
}

UNiagaraSystem* AFormationEffectActor::GetValidEffectFromMap(const EFormationEffect EffectType,
                                                             TArray<FFormationEffectColorParam>& OutColorParams) const
{
	if (not EnsureNiagaraCompIsValid())
	{
		return nullptr;
	}
	const FFormationPositionEffect* EffectPtr = M_EffectsMap.Find(EffectType);
	if (not EffectPtr || not IsValid(EffectPtr->EffectSystem))
	{
		RTSFunctionLibrary::ReportError("No effect found for type: " + UEnum::GetValueAsString(EffectType));
		return nullptr;
	}
	OutColorParams = EffectPtr->ColorParams;
	return EffectPtr->EffectSystem;
}

bool AFormationEffectActor::EnsureNiagaraCompIsValid() const
{
	if (not IsValid(M_NiagaraComp))
	{
		RTSFunctionLibrary::ReportError("No valid niagara component found on FormationEffectActor");
		return false;
	}
	return true;
}

void AFormationEffectActor::ApplyColorParams(const TArray<FFormationEffectColorParam>& ColorParams) const
{
	if (not EnsureNiagaraCompIsValid())
	{
		return;
	}
	for (const FFormationEffectColorParam& ColorParam : ColorParams)
	{
		M_NiagaraComp->SetColorParameter(ColorParam.ColorParamName, ColorParam.ColorValue);
	}
}
