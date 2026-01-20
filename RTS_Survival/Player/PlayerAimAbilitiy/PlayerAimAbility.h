// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "PlayerAimAbility.generated.h"

enum class EGrenadeAbilityType : uint8;
enum class EAbilityID : uint8;

UCLASS()
class RTS_SURVIVAL_API APlayerAimAbility : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APlayerAimAbility(const FObjectInitializer& ObjectInitializer);
	void InitPlayerAimAbility(ACPPController* PlayerController);

	void DetermineShowAimRadiusForAbility(
		const EAbilityID MainAbility,
		const int32 AbilitySubType, AActor* PrimarySelectedActor);
	void HideRadius();
	[[nodiscard]] bool IsPlayerAimActive() const { return bM_IsPlayerAimActive; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly)
	UStaticMeshComponent* AimMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<EPlayerAimAbilityTypes, UMaterialInterface*> MaterialPerAbilityAim;

	// How 1 unit of scale increase translates to world units.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float UnitsPerScale = 100.0f;

private:
	[[nodiscard]] bool GetIsValidAimMeshComponent() const;
	[[nodiscard]] bool GetIsValidMaterialForAimType(const EPlayerAimAbilityTypes AimType,
	                                                UMaterialInterface*& OutMaterial);
	UPROPERTY()
	bool bM_IsPlayerAimActive = false;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	[[nodiscard]] bool GetIsValidPlayerController() const;

	void OnAimActivated_PlayAnnouncerVl();


	EPlayerAimAbilityTypes GetAimTypeForAbility(const EAbilityID MainAbility, const int32 AbilitySubType,
	                                            const AActor* RTSValid_PrimarySelectedActor,
	                                            float& OutAbilityRadius) const;

	EPlayerAimAbilityTypes GetAimTypeFromGrenadeAbility(const EGrenadeAbilityType GrenadeAbilityType,
	                                                    const AActor* RTSValid_PrimarySelectedActor,
	                                                    float& OutAbilityRadius) const;

	void FailedToShowAimRadius();
	void OnNoRadiusForValidAimAbility(const float Radius, const EPlayerAimAbilityTypes AimType,
	                                  const EAbilityID MainAbility, const int32 AbilitySubType) const;

	void SetMaterialParameter(const float Radius, const EPlayerAimAbilityTypes TypeSet);
};
