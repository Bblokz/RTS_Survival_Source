#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/UnitData/UnitCost.h"

#include "GlobalAbilityCostState.generated.h"

enum class ETechnology : uint8;
class UNiagaraSystem;

USTRUCT(BlueprintType)
struct FGLobalAbilityCostState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUnitCost Costs = FUnitCost();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CoolDownTime = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CoolDownRemaining = 0;
};


USTRUCT(BlueprintType)
struct FGlobalAbilityRequirements
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETechnology RequiredTechnology = ETechnology::Tech_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTrainingOption RequiredUnit = FTrainingOption();
};

USTRUCT(BlueprintType)
struct FGlobalAbilityAimSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerAimAbilityTypes AimType = EPlayerAimAbilityTypes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AimRadius = 500.f;
};

USTRUCT(BlueprintType)
struct FGlobalAbilitySoundSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> Sound2DOnActivate = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> AnnouncerSoundOnActivate= nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> Sound2DOnExecute = nullptr;
};

USTRUCT(BlueprintType)
struct FGlobalAbilityUISettings
{
	GENERATED_BODY()	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> AbilityIcon = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title = FText::FromString("Ability Title");
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description = FText::FromString("Ability Description");
	
};

USTRUCT(BlueprintType)
struct FGlobalAbilityMarker
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> MarkerEffect = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor MarkerColor = FLinearColor::Red;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EffectTotalTime = 15.f;
};
