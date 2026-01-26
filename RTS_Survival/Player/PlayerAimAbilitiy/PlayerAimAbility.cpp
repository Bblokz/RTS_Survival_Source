// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerAimAbility.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


APlayerAimAbility::APlayerAimAbility(const FObjectInitializer& ObjectInitializer)
	: AActorObjectsMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Create default subobject for the aim mesh component.
	AimMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AimMeshComponent"));
	if (AimMeshComponent)
	{
		// Disable all collisions and physics
		AimMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimMeshComponent->SetGenerateOverlapEvents(false);
		AimMeshComponent->SetSimulatePhysics(false);
		// no decal effects
		AimMeshComponent->SetReceivesDecals(false);
		AimMeshComponent->SetCanEverAffectNavigation(false);
	}
	SetCanBeDamaged(false);
}

void APlayerAimAbility::InitPlayerAimAbility(ACPPController* PlayerController)
{
	M_PlayerController = PlayerController;
	// Error check.
	(void)GetIsValidPlayerController();
	HideRadius();
}

void APlayerAimAbility::DetermineShowAimRadiusForAbility(const EAbilityID MainAbility,
                                                         const int32 AbilitySubType, AActor* PrimarySelectedActor)
{
	if (not GetIsValidAimMeshComponent() || not RTSFunctionLibrary::RTSIsValid(PrimarySelectedActor))
	{
		return FailedToShowAimRadius();
	}
	float Radius = 0.f;
	const EPlayerAimAbilityTypes AimType = GetAimTypeForAbility(MainAbility, AbilitySubType, PrimarySelectedActor,
	                                                            Radius);
	if (AimType == EPlayerAimAbilityTypes::None)
	{
		return FailedToShowAimRadius();
	}
	if (Radius <= 1)
	{
		return OnNoRadiusForValidAimAbility(Radius, AimType, MainAbility, AbilitySubType);
	}
	bM_IsPlayerAimActive = true;
	SetActorHiddenInGame(false);
	const float ScaleValue = Radius / UnitsPerScale;
	SetActorScale3D(FVector(ScaleValue, ScaleValue, 1));
	UMaterialInterface* AimMaterial = nullptr;
	// Does error report.
	if (not GetIsValidMaterialForAimType(AimType, AimMaterial))
	{
		return FailedToShowAimRadius();
	}
	AimMeshComponent->SetMaterial(0, AimMaterial);
	// To adjust the radius.
	SetMaterialParameter(Radius, AimType);
	OnAimActivated_PlayAnnouncerVl();
}

void APlayerAimAbility::HideRadius()
{
	bM_IsPlayerAimActive = false;
	SetActorHiddenInGame(true);
}

// Called when the game starts or when spawned
void APlayerAimAbility::BeginPlay()
{
	Super::BeginPlay();

	if (AimMeshComponent)
	{
		// Disable all collisions and physics
		AimMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimMeshComponent->SetGenerateOverlapEvents(false);
		AimMeshComponent->SetSimulatePhysics(false);
		// no decal effects
		AimMeshComponent->SetReceivesDecals(false);
		AimMeshComponent->SetCanEverAffectNavigation(false);
	}
}

bool APlayerAimAbility::GetIsValidAimMeshComponent() const
{
	if (not IsValid(AimMeshComponent))
	{
		RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidAimMeshComponent"
			"AimMeshComponent is not valid!");
		return false;
	}
	return true;
}

bool APlayerAimAbility::GetIsValidMaterialForAimType(const EPlayerAimAbilityTypes AimType,
                                                     UMaterialInterface*& OutMaterial)
{
	if (not MaterialPerAbilityAim.Contains(AimType))
	{
		const FString TypeName = UEnum::GetValueAsString(AimType);
		RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidMaterialForAimType"
			"No material found for given AimType!"
			"\n type: " + TypeName);
		return false;
	}
	OutMaterial = MaterialPerAbilityAim[AimType];
	return true;
}

bool APlayerAimAbility::GetIsValidPlayerController() const
{
	if (not M_PlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidPlayerController"
			"PlayerController is not valid!");
		return false;
	}
	return true;
}

void APlayerAimAbility::OnAimActivated_PlayAnnouncerVl()
{
	if (not GetIsValidPlayerController())
	{
		return;
	}
	M_PlayerController->PlayAnnouncerVoiceLine(EAnnouncerVoiceLineType::SelectTargetForAimAbility, false, false);
}


EPlayerAimAbilityTypes APlayerAimAbility::GetAimTypeForAbility(const EAbilityID MainAbility,
                                                               const int32 AbilitySubType,
                                                               const AActor* RTSValid_PrimarySelectedActor,
                                                               float& OutAbilityRadius) const
{
	switch (MainAbility)
	{
	case EAbilityID::IdThrowGrenade:
		return GetAimTypeFromGrenadeAbility(static_cast<EGrenadeAbilityType>(AbilitySubType),
		                                    RTSValid_PrimarySelectedActor,
		                                    OutAbilityRadius);
	case EAbilityID::IdAimAbility:
		{
			const UAimAbilityComponent* AimAbilityComponent = FAbilityHelpers::GetHasAimAbilityComponent(
				static_cast<EAimAbilityType>(AbilitySubType), RTSValid_PrimarySelectedActor);
			if (not IsValid(AimAbilityComponent))
			{
				return EPlayerAimAbilityTypes::None;
			}
			// Note that we use the radius and not the range here.
			OutAbilityRadius = AimAbilityComponent->GetAimAbilityRadius();
			return AimAbilityComponent->GetAimAssistType();
		}
	case EAbilityID::IdAttackGround:
		{
			constexpr float AttackGroundAimRadius = 200.0f;
			OutAbilityRadius = AttackGroundAimRadius;
			return EPlayerAimAbilityTypes::SmallCrosshair;
		}
	default:
		return EPlayerAimAbilityTypes::None;
	}
}

EPlayerAimAbilityTypes APlayerAimAbility::GetAimTypeFromGrenadeAbility(
	const EGrenadeAbilityType GrenadeAbilityType, const AActor* RTSValid_PrimarySelectedActor,
	float& OutAbilityRadius) const
{
	const UGrenadeComponent* GrenadeComponent = FAbilityHelpers::GetGrenadeAbilityCompOfType(
		GrenadeAbilityType, RTSValid_PrimarySelectedActor);
	if (IsValid(GrenadeComponent))
	{
		// Note: if this fails because of invalid comp then later we do get a detailed error message due to the radius being invalid.
		OutAbilityRadius = GrenadeComponent->GetGrenadeAoeRange();
	}
	switch (GrenadeAbilityType)
	{
	case EGrenadeAbilityType::GerAtGrenade:
	case EGrenadeAbilityType::BundleSturmKommando:
		return EPlayerAimAbilityTypes::AntiVehicleGrenade;
	default:
		return EPlayerAimAbilityTypes::AntiInfantryGrenade;
	}
}

void APlayerAimAbility::FailedToShowAimRadius()
{
	HideRadius();
}

void APlayerAimAbility::OnNoRadiusForValidAimAbility(const float Radius, const EPlayerAimAbilityTypes AimType,
                                                     const EAbilityID MainAbility, const int32 AbilitySubType) const
{
	const FString AbilityName = UEnum::GetValueAsString(MainAbility) + " of subtype " +
		FString::FromInt(AbilitySubType);
	const FString AimTypeName = UEnum::GetValueAsString(AimType);
	RTSFunctionLibrary::ReportError("APlayerAimAbility::OnNoRadiusForValidAimAbility"
		"No radius defined for valid aim ability! (<=1)"
		"\n Ability: " + AbilityName +
		"\n AimType: " + AimTypeName +
		"\n Radius: " + FString::SanitizeFloat(Radius));
}

void APlayerAimAbility::SetMaterialParameter(const float Radius, const EPlayerAimAbilityTypes TypeSet)
{
	if (not GetIsValidAimMeshComponent())
	{
		return;
	}

	UMaterialInterface* Material = AimMeshComponent->GetMaterial(0);
	if (!Material)
	{
		RTSFunctionLibrary::ReportError(
			"APlayerAimAbility::SetMaterialParameter - No material found on AimMeshComponent.");
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
	if (!DynamicMaterial)
	{
		DynamicMaterial = AimMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
		if (!DynamicMaterial)
		{
			RTSFunctionLibrary::ReportError(
				"APlayerAimAbility::SetMaterialParameter - Failed to create dynamic material instance.");
			return;
		}
	}

	// Set the scalar parameter for the radius
	DynamicMaterial->SetScalarParameterValue(TEXT("Radius"), Radius);
}
