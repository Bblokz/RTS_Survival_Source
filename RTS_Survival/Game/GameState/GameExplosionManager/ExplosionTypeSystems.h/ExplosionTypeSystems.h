// ExplosionTypeSystems.h
#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Sound/SoundCue.h"
#include "ExplosionTypeSystems.generated.h"

UENUM(BlueprintType)
enum class ERTS_ExplosionType : uint8
{
	Large UMETA(DisplayName="Large"),
	Medium UMETA(DisplayName="Medium"),
	Gas UMETA(DisplayName="Gas"),
	Tank UMETA(DisplayName="Tank"),
	Tank_Large UMETA(DisplayName="Tank_Large"),
	Small UMETA(DisplayName="Small"),
	TankTurret UMETA(DisplayName="Tank Turret"),
	Huge UMETA(DisplayName="Huge"),
	Huge_Debris UMETA(DisplayName="Huge Debris"),
	Medium_Debris UMETA(DisplayName="Medium Debris"),
};

static FString Global_GetExplosionTypeString(const ERTS_ExplosionType Type)
{
	if (StaticEnum<ERTS_ExplosionType>())
	{
		const FName Name = StaticEnum<ERTS_ExplosionType>()->GetNameByValue((int64)Type);
		const FName CleanName = FName(*Name.ToString().RightChop(FString("ERTS_ExplosionType::").Len()));
		return CleanName.ToString();
	}
	return FString("Could not translate ERTS_ExplosionType to string");
}

UENUM()
enum class ERTS_DecalType : uint8
{
	Scorch UMETA(DisplayName="Scorch"),
	DirtDestroyed UMETA(DisplayName="Dirt Destroyed"),
	StoneDestroyed UMETA(DisplayName="Stone Destroyed"),
	EmberCrater UMETA(DisplayName="Ember Crater"),
	EmberScatter UMETA(DisplayName="Ember Scatter"),
};

USTRUCT(BlueprintType)
struct FDecalTypeSystems
{
	GENERATED_BODY()

public:
	// What type of decal these decals correspond to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decal")
	ERTS_DecalType DecalType = ERTS_DecalType::Scorch;

	// The decals.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decal")
	TArray<UMaterialInterface*> DecalMaterials;
};

USTRUCT(BlueprintType)
struct FExplosionTypeSystems
{
	GENERATED_BODY()

public:
	// Which type of explosion these systems correspond to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Explosion")
	ERTS_ExplosionType ExplosionType = ERTS_ExplosionType::Medium;

	// The actual NiagaraSystems that can be used for this ExplosionType
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Explosion")
	TArray<UNiagaraSystem*> Systems;

	// The sound effects to be played for a particular explosion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Explosion")
	TArray<USoundCue*> SoundCues;
};
