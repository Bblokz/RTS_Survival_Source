// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentGarrisonSelectable.h"

#include "ApartmentDestructibleActor.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ApartmentGarrisonSelectable
{
	constexpr uint8 PlayerOwner = 1;
	const FVector SelectionAreaExtent(120.f, 120.f, 120.f);
	// How far the selection decal projects up and down from the ground the building stands on.
	constexpr float DecalProjectionDepth = 600.f;
	// The decal reaches a little past the walls so its edge is not hidden under them.
	constexpr float DecalMargin = 150.f;
	const FRotator DecalFacingDown(-90.f, 0.f, 0.f);
	const TCHAR* const DefaultSelectedDecalPath =
		TEXT("/Game/RTS_Survival/Materials/Selection/M_Selection_Box.M_Selection_Box");
	const TCHAR* const DefaultDeselectedDecalPath = TEXT(
		"/Game/RTS_Survival/Materials/Selection/Deselected/M_Selection_Box_Deselected.M_Selection_Box_Deselected");
}

UApartmentGarrisonRTSComponent::UApartmentGarrisonRTSComponent()
{
	OwningPlayer = ApartmentGarrisonSelectable::PlayerOwner;
	UnitType = EAllUnitType::UNType_GarrisonBuilding;
	bAddToGameState = false;
	FormationUnitInnerRadius = 0.f;
}

void UApartmentGarrisonSelectionComponent::SetupGarrisonSelection(
	UBoxComponent* SelectionArea, UDecalComponent* SelectionDecal, UMaterialInterface* NewSelectedMaterial,
	UMaterialInterface* NewDeselectedMaterial)
{
	SelectedMaterial = NewSelectedMaterial;
	DeselectedMaterial = NewDeselectedMaterial;
	InitSelectionComponent(SelectionArea, SelectionDecal);
}

AApartmentGarrisonSelectable::AApartmentGarrisonSelectable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace ApartmentGarrisonSelectable;
	PrimaryActorTick.bCanEverTick = false;

	M_SelectionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionArea"));
	SetRootComponent(M_SelectionArea);
	M_SelectionArea->SetBoxExtent(SelectionAreaExtent);
	M_SelectionArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_SelectionArea->SetCanEverAffectNavigation(false);

	M_SelectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("SelectionDecal"));
	M_SelectionDecal->SetupAttachment(M_SelectionArea);
	M_SelectionDecal->SetRelativeRotation(DecalFacingDown);
	M_SelectionDecal->SetVisibility(false);

	M_GarrisonRTSComponent = CreateDefaultSubobject<UApartmentGarrisonRTSComponent>(TEXT("RTSComponent"));
	M_GarrisonHealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	M_GarrisonSelectionComponent = CreateDefaultSubobject<UApartmentGarrisonSelectionComponent>(
		TEXT("SelectionComponent"));

	SelectedDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultSelectedDecalPath));
	DeselectedDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(DefaultDeselectedDecalPath));
}

void AApartmentGarrisonSelectable::InitGarrison(AApartmentDestructibleActor* Apartment, const FVector& FootprintExtent)
{
	using namespace ApartmentGarrisonSelectable;
	M_Apartment = Apartment;
	if (not GetIsValidSelectionDecal())
	{
		return;
	}
	// The decal looks down: its X axis is the projection depth, Y and Z lie on the ground.
	M_SelectionDecal->DecalSize = FVector(
		DecalProjectionDepth, FootprintExtent.Y + DecalMargin, FootprintExtent.X + DecalMargin);
}

void AApartmentGarrisonSelectable::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupSelection();
	BeginPlay_SetupCommandCard();
}

void AApartmentGarrisonSelectable::BeginPlay_SetupSelection()
{
	if (not GetIsValidGarrisonSelectionComponent() || not GetIsValidSelectionDecal())
	{
		return;
	}
	M_GarrisonSelectionComponent->SetupGarrisonSelection(
		M_SelectionArea,
		M_SelectionDecal,
		SelectedDecalMaterial.LoadSynchronous(),
		DeselectedDecalMaterial.LoadSynchronous());
	M_GarrisonSelectionComponent->SetUnitDeselected();
}

void AApartmentGarrisonSelectable::BeginPlay_SetupCommandCard()
{
	constexpr int32 CardSlotCount = DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI;
	TArray<EAbilityID> CardAbilities;
	CardAbilities.Init(EAbilityID::IdNoAbility, CardSlotCount);
	CardAbilities[FMath::Clamp(ExitCargoAbilityIndex, 0, CardSlotCount - 1)] = EAbilityID::IdExitCargo;
	InitAbilityArray(CardAbilities);
}

void AApartmentGarrisonSelectable::ExecuteExitCargoCommand()
{
	if (GetIsValidApartment())
	{
		M_Apartment->EvacuateGarrison();
	}
	DoneExecutingCommand(EAbilityID::IdExitCargo);
}

bool AApartmentGarrisonSelectable::GetIsValidGarrisonSelectionComponent() const
{
	if (IsValid(M_GarrisonSelectionComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_GarrisonSelectionComponent", "GetIsValidGarrisonSelectionComponent", this);
	return false;
}

bool AApartmentGarrisonSelectable::GetIsValidSelectionDecal() const
{
	if (IsValid(M_SelectionDecal))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_SelectionDecal", "GetIsValidSelectionDecal", this);
	return false;
}

bool AApartmentGarrisonSelectable::GetIsValidApartment() const
{
	if (M_Apartment.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_Apartment", "GetIsValidApartment", this);
	return false;
}
