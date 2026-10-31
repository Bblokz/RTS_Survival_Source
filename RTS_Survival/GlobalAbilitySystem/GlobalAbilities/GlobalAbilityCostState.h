#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/UnitData/UnitCost.h"

#include "GlobalAbilityCostState.generated.h"

enum class ETechnology : uint8;

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

/** @brief The custom target area of an ability that does not use the crosshair textures. */
USTRUCT(BlueprintType)
struct FGlobalAbilityAreaAimSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGlobalAbilityAimShape Shape = EGlobalAbilityAimShape::Reticle;

	// Procedural shader of the target area; it gets its sizes in cm, see APlayerAimAbility::ApplyAreaParameters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> AreaMaterial = nullptr;

	// Circle: radius of the area. Strip: half the width of the affected strip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	float AreaRadius = 1000.f;

	// Circle only: radius of the inner zone the shader marks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float InnerRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	float MinStripLength = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	float MaxStripLength = 6000.f;

	// Length shown before the first click; 0 shows only the round end of the strip at the cursor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float LengthBeforeAnchor = 0.f;

	// How many evenly spaced markers the shader draws along the strip (flares); 0 draws none.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 ShaderMarkerCount = 0;

	// Drawn at every point of UGlobalAbility::GetStripDropPoints, so the player sees exactly where things will land.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> DropPointMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> DropPointMaterial = nullptr;

	// The real meshes are too small to read from the RTS camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01"))
	float DropPointScale = 3.f;
};

USTRUCT(BlueprintType)
struct FGlobalAbilityAimSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EPlayerAimAbilityTypes AimType = EPlayerAimAbilityTypes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AimRadius = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGlobalAbilityAreaAimSettings AreaAim;
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
	UNiagaraSystem* MarkerEffect = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor MarkerColor = FLinearColor::Red;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EffectTotalTime = 15.f;
};
