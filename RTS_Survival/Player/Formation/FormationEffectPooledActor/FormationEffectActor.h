// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/Player/Formation/FormationPositionEffects/PlayerFormationPositionEffects.h"
#include "FormationEffectActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FFormationEffectColorParam
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor ColorValue = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ColorParamName = "FloatValue";
};

USTRUCT(BlueprintType)
struct FFormationPositionEffect
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UNiagaraSystem* EffectSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EFormationEffect EffectType = EFormationEffect::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FFormationEffectColorParam> ColorParams;
};

UCLASS()
class RTS_SURVIVAL_API AFormationEffectActor : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFormationEffectActor(const FObjectInitializer& ObjectInitializer);

	void SetFormationEffectActivated(const bool bActivate,
		const EFormationEffect EffectType);

protected:
	virtual void PostInitializeComponents() override;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FFormationPositionEffect> FormationPositionEffects;

private:
	TMap<EFormationEffect, FFormationPositionEffect> M_EffectsMap;

	UNiagaraSystem* GetValidEffectFromMap(const EFormationEffect EffectType, TArray<FFormationEffectColorParam>& OutColorParams) const;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_NiagaraComp = nullptr;

	bool EnsureNiagaraCompIsValid()const;

	void ApplyColorParams(const TArray<FFormationEffectColorParam>& ColorParams) const;
};
