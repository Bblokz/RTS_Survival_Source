// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerAimAbility.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityTypes/AimAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace PlayerAimAbilityAreaConstants
{
	// /Engine/BasicShapes/Plane is 100 x 100 cm.
	constexpr float PlaneSizeCm = 100.f;
	// Room around the area inside the plane, for the outlines and the glow of the shaders.
	constexpr float PlanePaddingCm = 300.f;
	constexpr float HeightAboveGroundCm = 30.f;
	const FName PlaneLengthParameter(TEXT("PlaneLength"));
	const FName PlaneWidthParameter(TEXT("PlaneWidth"));
	const FName StripLengthParameter(TEXT("StripLength"));
	const FName AreaRadiusParameter(TEXT("AreaRadius"));
	const FName InnerRadiusParameter(TEXT("InnerRadius"));
	const FName MarkerCountParameter(TEXT("MarkerCount"));
	const FName IsDrawingParameter(TEXT("IsDrawing"));
}

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

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	M_AreaMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AreaMeshComponent"));
	M_AreaMeshComponent->SetupAttachment(AimMeshComponent);
	M_AreaMeshComponent->SetStaticMesh(PlaneMesh.Object);
	M_AreaMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_AreaMeshComponent->SetGenerateOverlapEvents(false);
	M_AreaMeshComponent->SetReceivesDecals(false);
	M_AreaMeshComponent->SetCanEverAffectNavigation(false);
	M_AreaMeshComponent->SetCastShadow(false);
	M_AreaMeshComponent->SetVisibility(false);

	M_DropPointMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DropPointMarkers"));
	M_DropPointMarkers->SetupAttachment(AimMeshComponent);
	M_DropPointMarkers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_DropPointMarkers->SetGenerateOverlapEvents(false);
	M_DropPointMarkers->SetReceivesDecals(false);
	M_DropPointMarkers->SetCanEverAffectNavigation(false);
	M_DropPointMarkers->SetCastShadow(false);
	M_DropPointMarkers->SetVisibility(false);
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
	if (MainAbility == EAbilityID::IdAimAbility)
	{
		OnAimActivated_PlayAnnouncerVl();
	}
}

void APlayerAimAbility::ShowGlobalAbilityRadius(const float Radius, const EPlayerAimAbilityTypes AimType)
{
	if (not GetIsValidAimMeshComponent())
	{
		return;
	}
	if (Radius <= 1.f || AimType == EPlayerAimAbilityTypes::None)
	{
		HideRadius();
		return;
	}
	UMaterialInterface* AimMaterial = nullptr;
	if (not GetIsValidMaterialForAimType(AimType, AimMaterial))
	{
		HideRadius();
		return;
	}
	bM_IsPlayerAimActive = true;
	SetActorHiddenInGame(false);
	const float ScaleValue = Radius / UnitsPerScale;
	SetActorScale3D(FVector(ScaleValue, ScaleValue, 1));
	AimMeshComponent->SetMaterial(0, AimMaterial);
	SetMaterialParameter(Radius, AimType);
}

void APlayerAimAbility::HideRadius()
{
	bM_IsPlayerAimActive = false;
	HideArea();
	SetActorHiddenInGame(true);
}

void APlayerAimAbility::ShowGlobalAbilityArea(const FGlobalAbilityAimSettings& AimSettings,
                                              const UGlobalAbility* Ability)
{
	const FGlobalAbilityAreaAimSettings& AreaAim = AimSettings.AreaAim;
	if (not GetIsValidAimMeshComponent() || not IsValid(M_AreaMeshComponent) || not IsValid(AreaAim.AreaMaterial))
	{
		RTSFunctionLibrary::ReportError("APlayerAimAbility::ShowGlobalAbilityArea has no area material or plane.");
		HideRadius();
		return;
	}

	M_AreaAimShown.Shape = AreaAim.Shape;
	M_AreaAimShown.AreaRadius = AreaAim.AreaRadius;
	M_AreaAimShown.InnerRadius = AreaAim.InnerRadius;
	M_AreaAimShown.MinStripLength = AreaAim.MinStripLength;
	M_AreaAimShown.MaxStripLength = FMath::Max(AreaAim.MinStripLength, AreaAim.MaxStripLength);
	M_AreaAimShown.LengthBeforeAnchor = AreaAim.LengthBeforeAnchor;
	M_AreaAimShown.ShaderMarkerCount = AreaAim.ShaderMarkerCount;
	M_AreaAimShown.DropPointScale = AreaAim.DropPointScale;
	M_AreaAimAbility = Ability;
	M_AreaAimState = EGlobalAbilityAreaAimState::FollowingCursor;

	bM_IsPlayerAimActive = true;
	SetActorHiddenInGame(false);
	// The plane carries the size: a scaled actor would also scale the world-space drop point instances.
	SetActorScale3D(FVector::OneVector);
	AimMeshComponent->SetVisibility(false);
	M_AreaMaterialInstance = UMaterialInstanceDynamic::Create(AreaAim.AreaMaterial, this);
	M_AreaMeshComponent->SetMaterial(0, M_AreaMaterialInstance);
	M_AreaMeshComponent->SetVisibility(true);

	const bool bHasDropPoints = AreaAim.Shape == EGlobalAbilityAimShape::Strip && IsValid(AreaAim.DropPointMesh);
	M_DropPointMarkers->ClearInstances();
	M_DropPointMarkers->SetVisibility(bHasDropPoints);
	if (bHasDropPoints)
	{
		M_DropPointMarkers->SetStaticMesh(AreaAim.DropPointMesh);
		M_DropPointMarkers->SetMaterial(0, AreaAim.DropPointMaterial);
	}
}

void APlayerAimAbility::UpdateGlobalAbilityArea(const FVector& CursorLocation)
{
	const float CameraYaw = GetCameraYaw();
	if (M_AreaAimShown.Shape == EGlobalAbilityAimShape::Circle)
	{
		PlaceArea(CursorLocation, CursorLocation, CameraYaw);
		return;
	}
	if (M_AreaAimState == EGlobalAbilityAreaAimState::DrawingStrip)
	{
		const FVector StripEnd = GetClampedStripEnd(CursorLocation);
		PlaceArea(M_StripStart, StripEnd, CameraYaw);
		UpdateDropPointMarkers(M_StripStart, StripEnd);
		return;
	}
	// Before the first click the strip leaves the cursor up the screen, so its size can be judged.
	const FVector PreviewEnd = CursorLocation
		+ FRotator(0.f, CameraYaw, 0.f).Vector() * M_AreaAimShown.LengthBeforeAnchor;
	PlaceArea(CursorLocation, PreviewEnd, CameraYaw);
	UpdateDropPointMarkers(CursorLocation, PreviewEnd);
}

bool APlayerAimAbility::TryAnchorStrip(const FVector& ClickedLocation)
{
	if (M_AreaAimState != EGlobalAbilityAreaAimState::FollowingCursor
		|| M_AreaAimShown.Shape != EGlobalAbilityAimShape::Strip)
	{
		return false;
	}
	M_StripStart = ClickedLocation;
	M_AreaAimState = EGlobalAbilityAreaAimState::DrawingStrip;
	return true;
}

bool APlayerAimAbility::GetStripLine(const FVector& ClickedLocation, FVector& OutLineStart, FVector& OutLineEnd) const
{
	if (M_AreaAimState != EGlobalAbilityAreaAimState::DrawingStrip)
	{
		return false;
	}
	OutLineStart = M_StripStart;
	OutLineEnd = GetClampedStripEnd(ClickedLocation);
	return true;
}

void APlayerAimAbility::HideArea()
{
	M_AreaAimState = EGlobalAbilityAreaAimState::Inactive;
	M_AreaAimAbility.Reset();
	if (IsValid(AimMeshComponent))
	{
		AimMeshComponent->SetVisibility(true);
	}
	if (IsValid(M_AreaMeshComponent))
	{
		M_AreaMeshComponent->SetVisibility(false);
	}
	if (IsValid(M_DropPointMarkers))
	{
		M_DropPointMarkers->ClearInstances();
		M_DropPointMarkers->SetVisibility(false);
	}
}

FVector APlayerAimAbility::GetClampedStripEnd(const FVector& CursorLocation) const
{
	FVector ToCursor = CursorLocation - M_StripStart;
	ToCursor.Z = 0.f;
	const float CursorDistance = ToCursor.Size();
	const FVector Direction = CursorDistance > KINDA_SMALL_NUMBER
		                          ? ToCursor / CursorDistance
		                          : FRotator(0.f, GetCameraYaw(), 0.f).Vector();
	const float StripLength = FMath::Clamp(CursorDistance, M_AreaAimShown.MinStripLength,
	                                       M_AreaAimShown.MaxStripLength);
	FVector StripEnd = M_StripStart + Direction * StripLength;
	FVector GroundLocation;
	if (GlobalAbilityHelpers::TryFindGroundLocation(this, StripEnd, GroundLocation))
	{
		StripEnd = GroundLocation;
	}
	return StripEnd;
}

float APlayerAimAbility::GetCameraYaw() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(M_PlayerController.Get());
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return GetActorRotation().Yaw;
	}
	return PlayerController->PlayerCameraManager->GetCameraRotation().Yaw;
}

void APlayerAimAbility::PlaceArea(const FVector& LineStart, const FVector& LineEnd, const float Yaw)
{
	using namespace PlayerAimAbilityAreaConstants;
	const FVector Line = LineEnd - LineStart;
	const float StripLength = Line.Size2D();
	const float EnclosingSize = 2.f * (M_AreaAimShown.AreaRadius + PlanePaddingCm);
	const float PlaneLength = StripLength + EnclosingSize;
	const float PlaneWidth = EnclosingSize;

	const FVector Centre = (LineStart + LineEnd) * 0.5f + FVector(0.f, 0.f, HeightAboveGroundCm);
	const FRotator Rotation = StripLength > KINDA_SMALL_NUMBER ? Line.Rotation() : FRotator(0.f, Yaw, 0.f);
	SetActorLocationAndRotation(Centre, Rotation);
	M_AreaMeshComponent->SetRelativeScale3D(FVector(PlaneLength / PlaneSizeCm, PlaneWidth / PlaneSizeCm, 1.f));
	ApplyAreaParameters(StripLength, PlaneLength, PlaneWidth);
}

void APlayerAimAbility::ApplyAreaParameters(const float StripLength, const float PlaneLength,
                                            const float PlaneWidth) const
{
	using namespace PlayerAimAbilityAreaConstants;
	if (not IsValid(M_AreaMaterialInstance))
	{
		return;
	}
	const bool bIsDrawing = M_AreaAimState == EGlobalAbilityAreaAimState::DrawingStrip;
	M_AreaMaterialInstance->SetScalarParameterValue(PlaneLengthParameter, PlaneLength);
	M_AreaMaterialInstance->SetScalarParameterValue(PlaneWidthParameter, PlaneWidth);
	M_AreaMaterialInstance->SetScalarParameterValue(StripLengthParameter, StripLength);
	M_AreaMaterialInstance->SetScalarParameterValue(AreaRadiusParameter, M_AreaAimShown.AreaRadius);
	M_AreaMaterialInstance->SetScalarParameterValue(InnerRadiusParameter, M_AreaAimShown.InnerRadius);
	M_AreaMaterialInstance->SetScalarParameterValue(MarkerCountParameter,
	                                                static_cast<float>(M_AreaAimShown.ShaderMarkerCount));
	M_AreaMaterialInstance->SetScalarParameterValue(IsDrawingParameter, bIsDrawing ? 1.f : 0.f);
}

void APlayerAimAbility::UpdateDropPointMarkers(const FVector& LineStart, const FVector& LineEnd)
{
	using namespace PlayerAimAbilityAreaConstants;
	if (not IsValid(M_DropPointMarkers) || not M_DropPointMarkers->IsVisible() || not M_AreaAimAbility.IsValid())
	{
		return;
	}

	TArray<FVector> DropPoints;
	M_AreaAimAbility->GetStripDropPoints(LineStart, LineEnd, DropPoints);
	const FVector Scale(M_AreaAimShown.DropPointScale);
	const FRotator Rotation = (LineEnd - LineStart).GetSafeNormal2D().Rotation();
	if (M_DropPointMarkers->GetInstanceCount() != DropPoints.Num())
	{
		M_DropPointMarkers->ClearInstances();
		for (const FVector& DropPoint : DropPoints)
		{
			M_DropPointMarkers->AddInstance(FTransform(Rotation, DropPoint, Scale), true);
		}
		return;
	}
	for (int32 PointIndex = 0; PointIndex < DropPoints.Num(); ++PointIndex)
	{
		const bool bIsLastPoint = PointIndex == DropPoints.Num() - 1;
		M_DropPointMarkers->UpdateInstanceTransform(PointIndex, FTransform(Rotation, DropPoints[PointIndex], Scale),
		                                            true, bIsLastPoint);
	}
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
	UMaterialInterface* const* FoundMaterial = MaterialPerAbilityAim.Find(AimType);
	if (FoundMaterial && IsValid(*FoundMaterial))
	{
		OutMaterial = *FoundMaterial;
		return true;
	}

	const FString TypeName = UEnum::GetValueAsString(AimType);
	RTSFunctionLibrary::ReportError("APlayerAimAbility::GetIsValidMaterialForAimType"
		"No valid material found for given AimType!"
		"\n type: " + TypeName);
	return false;
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
	case EAbilityID::IdAttachedWeapon:
		{
			const UAttachedWeaponAbilityComponent* AbilityComponent =
				FAbilityHelpers::GetAttachedWeaponAbilityComponent(
					static_cast<EAttachWeaponAbilitySubType>(AbilitySubType), RTSValid_PrimarySelectedActor);
			if (not IsValid(AbilityComponent))
			{
				return EPlayerAimAbilityTypes::None;
			}
			OutAbilityRadius = AbilityComponent->GetAttachedWeaponAbilityRadius();
			return AbilityComponent->GetAimAssistType();
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
	case EGrenadeAbilityType::RusRPG43:
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
	if (not Material)
	{
		RTSFunctionLibrary::ReportError(
			"APlayerAimAbility::SetMaterialParameter - No material found on AimMeshComponent.");
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
	if (not DynamicMaterial)
	{
		DynamicMaterial = AimMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
		if (not DynamicMaterial)
		{
			RTSFunctionLibrary::ReportError(
				"APlayerAimAbility::SetMaterialParameter - Failed to create dynamic material instance.");
			return;
		}
	}

	// Set the scalar parameter for the radius
	DynamicMaterial->SetScalarParameterValue(TEXT("Radius"), Radius);
}
