// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SelectionComponent.h"

#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/GameState.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// Sets default values for this component's properties
USelectionComponent::USelectionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), SelectedMaterial(nullptr), DeselectedMaterial(nullptr), bM_IsSelected(false),
	  // Important; makes sure any unit with this component will be selectable from the start.
	  bM_CanBeSelected(true),
	  M_SelectionArea(nullptr),
	  M_SelectedDecalRef(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USelectionComponent::SetIsInPrimarySameType(const bool bIsInPrimarySameType)
{
	// Early-out to avoid redundant broadcasts 
	if (bM_IsPrimarySelectedSameType == bIsInPrimarySameType)
	{
		return;
	}

	bM_IsPrimarySelectedSameType = bIsInPrimarySameType;

	// Notify listeners
	OnPrimarySameTypeChanged.Broadcast(bM_IsPrimarySelectedSameType);
}

bool USelectionComponent::GetIsInPrimarySameType() const
{
	return bM_IsPrimarySelectedSameType;
}

void USelectionComponent::OnUnitHoverChange(const bool bHovered) const
{
	if (bHovered)
	{
		OnUnitHovered.Broadcast();
	}
	else
	{
		OnUnitUnHovered.Broadcast();
	}
}


void USelectionComponent::SetDecalRelativeLocation(const FVector NewLocation)
{
	if (not IsValid(M_SelectedDecalRef))
	{
		return;
	}

	M_SelectedDecalRef->SetRelativeLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
}


void USelectionComponent::SetDeselectedDecalSetting(const bool bNewUseDeselectedDecal)
{
	bM_UseDeselectDecal = bNewUseDeselectedDecal;
	if (!bM_IsSelected)
	{
		if (M_SelectedDecalRef)
		{
			if (DeselectedMaterial)
			{
				SetDecalDeselected();
			}
			else
			{
				FString OwnerName = GetOwner() ? GetOwner()->GetName() : "nullptr";
				RTSFunctionLibrary::ReportError(
					"UPDATE ERROR, No deselected material was set on " + OwnerName +
					"\n See function SetDeselectedDecalSetting in SelectionComponent.cpp");
			}
		}
		else
		{
			UWorld* World = GetWorld();
			if (not IsValid(World))
			{
				return;
			}

			// set timer; wait 2 seconds for the init call.
			FTimerHandle RecheckTimerHandle;
			World->GetTimerManager().SetTimer(
				RecheckTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [WeakSelectionComponent = TWeakObjectPtr<USelectionComponent>(this), bNewUseDeselectedDecal]()
				{
					if (not WeakSelectionComponent.IsValid())
					{
						return;
					}

					USelectionComponent* StrongSelectionComponent = WeakSelectionComponent.Get();
					if (IsValid(StrongSelectionComponent->M_SelectedDecalRef))
					{
						StrongSelectionComponent->SetDeselectedDecalSetting(bNewUseDeselectedDecal);
					}
				}),
				2.0f,
				false
			);
		}
	}
}

void USelectionComponent::UpdateSelectionArea(const FVector& NewSize, const FVector& NewPosition) const
{
	if (not IsValid(M_SelectionArea))
	{
		return;
	}

	M_SelectionArea->SetRelativeScale3D(NewSize);
	M_SelectionArea->SetRelativeLocation(NewPosition);
}

void USelectionComponent::SetUnitSelected()
{
	SetDecalSelected();
	bM_IsSelected = true;
	OnUnitSelected.Broadcast();
}

void USelectionComponent::SetUnitDeselected()
{
	SetDecalDeselected();
	bM_IsSelected = false;
	OnUnitDeselected.Broadcast();
}

void USelectionComponent::HideDecals()
{
	if (not IsValid(M_SelectedDecalRef))
	{
		return;
	}

	M_SelectedDecalRef->SetVisibility(false, false);
	M_SelectedDecalRef->SetHiddenInGame(true, false);
}

void USelectionComponent::UpdateSelectionMaterials(UMaterialInterface* NewSelectedMaterial,
                                                   UMaterialInterface* NewDeselectedMaterial)
{
	if (NewSelectedMaterial)
	{
		SelectedMaterial = NewSelectedMaterial;
	}
	if (NewDeselectedMaterial)
	{
		DeselectedMaterial = NewDeselectedMaterial;
	}
	if (bM_IsSelected)
	{
		SetDecalSelected();
	}
	else
	{
		SetDecalDeselected();
	}
}

TPair<UMaterialInterface*, UMaterialInterface*> USelectionComponent::GetMaterials() const
{
	return TPair<UMaterialInterface*, UMaterialInterface*>(SelectedMaterial.Get(), DeselectedMaterial.Get());
}

void USelectionComponent::UpdateDecalScale(const FVector NewScale)
{
	if (not IsValid(M_SelectedDecalRef))
	{
		return;
	}

	M_SelectedDecalRef->SetRelativeScale3D(NewScale);
}


// Called when the game starts
void USelectionComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("nullptr");
	if (AGameStateBase* GameStateBase = GetWorld() != nullptr ? GetWorld()->GetGameState() : nullptr)
	{
		if (ACPPGameState* GameState = Cast<ACPPGameState>(GameStateBase))
		{
			SetDeselectedDecalSetting(GameState->GetGameSetting<bool>(ERTSGameSetting::UseDeselectedDecals));
		}
		else
		{
			RTSFunctionLibrary::ReportError("GameState is not of type ACPPGameState in " + OwnerName);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("GameState is nullptr in " + OwnerName);
	}
}

void USelectionComponent::BeginDestroy()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::BeginDestroy();
}


void USelectionComponent::InitSelectionComponent(
	UBoxComponent* NewSelectionArea,
	UDecalComponent* NewSelectedDecal)
{
	if (NewSelectionArea)
	{
		M_SelectionArea = NewSelectionArea;
	}
	else
	{
		const AActor* Owner = GetOwner();
		const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("nullptr");
		RTSFunctionLibrary::ReportError("selection area was not set on " + OwnerName);
	}
	if (NewSelectedDecal)
	{
		M_SelectedDecalRef = NewSelectedDecal;
	}
	else
	{
		const AActor* Owner = GetOwner();
		const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("nullptr");
		RTSFunctionLibrary::ReportError("selected decal was not set on " + OwnerName);
	}
	if (!SelectedMaterial)
	{
		const AActor* Owner = GetOwner();
		const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("nullptr");
		RTSFunctionLibrary::ReportError("No selected material was set on " + OwnerName);
	}
	if (!DeselectedMaterial)
	{
		const AActor* Owner = GetOwner();
		const FString OwnerName = IsValid(Owner) ? Owner->GetName() : TEXT("nullptr");
		RTSFunctionLibrary::ReportError("No deselected material was set on " + OwnerName);
	}
}

void USelectionComponent::SetDecalSelected() const
{
	if (not IsValid(M_SelectedDecalRef) || not IsValid(SelectedMaterial))
	{
		return;
	}

	M_SelectedDecalRef->SetVisibility(true, false);
	M_SelectedDecalRef->SetHiddenInGame(false, false);
	M_SelectedDecalRef->SetMaterial(0, SelectedMaterial);
}

void USelectionComponent::SetDecalDeselected() const
{
	if (!IsValid(M_SelectedDecalRef))
	{
		return;
	}
	if (bM_UseDeselectDecal && IsValid(DeselectedMaterial))
	{
		M_SelectedDecalRef->SetVisibility(true, false);
		M_SelectedDecalRef->SetHiddenInGame(false, false);
		M_SelectedDecalRef->SetMaterial(0, DeselectedMaterial);
	}
	else
	{
		M_SelectedDecalRef->SetVisibility(false, false);
		M_SelectedDecalRef->SetHiddenInGame(true, false);
	}
}
