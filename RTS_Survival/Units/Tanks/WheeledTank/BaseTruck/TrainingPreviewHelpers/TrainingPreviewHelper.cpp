#include "TrainingPreviewHelper.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "TrainingPreviewSettings/TrainingPreviewSettings.h"

namespace TrainingPreview
{
	static const FName TrainingPreviewSocketName = TEXT("TrainingPreview");
}

void FTrainingPreviewHelper::SetupTrainingPreviewIfNotExisting(ANomadicVehicle* NomadTraining)
{
	if (not IsValid(NomadTraining) || not IsValid(NomadTraining->BuildingMeshComponent))
	{
		return;
	}

	if (IsValid(M_TrainingPreview))
	{
		return;
	}

	if (not NomadTraining->BuildingMeshComponent->DoesSocketExist(TrainingPreview::TrainingPreviewSocketName))
	{
		RTSFunctionLibrary::ReportError(
			"The nomadic vehicle: " + NomadTraining->GetName() +
			" is missing the required socket: " + TrainingPreview::TrainingPreviewSocketName.ToString() +
			" for the training preview.");
		return;
	}

	M_TrainingPreview = NewObject<UStaticMeshComponent>(NomadTraining, UStaticMeshComponent::StaticClass(),
	                                                    TEXT("TrainingPreviewMesh"));

	if (not IsValid(M_TrainingPreview))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create TrainingPreviewMesh component on: " + NomadTraining->GetName());
		return;
	}

	M_TrainingPreview->AttachToComponent(
		NomadTraining->BuildingMeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TrainingPreview::TrainingPreviewSocketName);

	// Cosmetic only: no collision, no nav, no ticking.
	M_TrainingPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_TrainingPreview->SetCanEverAffectNavigation(false);
	M_TrainingPreview->SetGenerateOverlapEvents(false);
	M_TrainingPreview->SetCastShadow(false);
	M_TrainingPreview->SetVisibility(false, true);
	M_TrainingPreview->SetComponentTickEnabled(false);
	M_TrainingPreview->RegisterComponent();
}

void FTrainingPreviewHelper::OnStartOrResumeTraining(const FTrainingOption& StartedOption, ANomadicVehicle* Nomad)
{
	bM_ShouldBeVisible = true;
	SetupTrainingPreviewIfNotExisting(Nomad);
	if (not GetIsValidTrainingPreview())
	{
		return;
	}

	SetPreviewVisibility(true);

	if (StartedOption == M_ActivePreviewTrainingOption)
	{
		// Only Set visibility in case we resumed training after packing up.
		return;
	}

	// New/changed option: reset old state and resolve new soft mesh.
	ResetPreviewState(/*bHide*/false);
	M_ActivePreviewTrainingOption = StartedOption;

	TSoftObjectPtr<UStaticMesh> SoftMesh = GetStaticMeshFromSettings(StartedOption);
	if (SoftMesh.IsNull())
	{
		// Missing mapping; keep it hidden to avoid stale preview.
		SetPreviewVisibility(false);
		return;
	}

	M_CachedSoftMesh = SoftMesh;
	BeginAsyncLoadPreview(SoftMesh, Nomad);
}

void FTrainingPreviewHelper::HandleTrainingDisabledOrNomadTruck()
{
	// Do not clear the active option; just hide the preview while training is disabled.
	SetPreviewVisibility(false);
	bM_ShouldBeVisible = false;
}

void FTrainingPreviewHelper::OnActiveTrainingCancelled(const FTrainingOption& CancelledOption)
{
	// Only reset/hide if the cancelled option is the one we’re currently previewing.
	if (CancelledOption == M_ActivePreviewTrainingOption)
	{
		ResetPreviewState(true);
	}
}

void FTrainingPreviewHelper::OnTrainingCompleted(const FTrainingOption& CompletedOption)
{
	// Only reset/hide if the completed option is the one we’re currently previewing.
	if (CompletedOption == M_ActivePreviewTrainingOption)
	{
		ResetPreviewState(true);
	}
}


void FTrainingPreviewHelper::ResetAsyncState()
{
	if (M_StreamableHandle.IsValid())
	{
		// Some engine versions have CancelHandle; otherwise reset is enough.
		M_StreamableHandle.Reset();
	}
}

bool FTrainingPreviewHelper::GetIsValidTrainingPreview() const
{
	if (IsValid(M_TrainingPreview))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Training preview mesh component is not valid.");
	return false;
}

const UTrainingPreviewSettings* FTrainingPreviewHelper::GetSettingsCached()
{
	if (M_CachedSettings.IsValid())
	{
		return M_CachedSettings.Get();
	}

	const UTrainingPreviewSettings* Settings = GetDefault<UTrainingPreviewSettings>();
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError("TrainingPreviewSettings (Developer Settings) could not be resolved.");
		return nullptr;
	}
	M_CachedSettings = Settings;
	return Settings;
}

void FTrainingPreviewHelper::SetPreviewVisibility(const bool bIsVisible) const
{
	if (not IsValid(M_TrainingPreview))
	{
		return;
	}
	M_TrainingPreview->SetVisibility(bIsVisible, true);
}

TSoftObjectPtr<UStaticMesh> FTrainingPreviewHelper::GetStaticMeshFromSettings(const FTrainingOption& TrainingOption)
{
	const UTrainingPreviewSettings* Settings = GetSettingsCached();
	if (not IsValid(Settings))
	{
		return TSoftObjectPtr<UStaticMesh>();
	}

	TSoftObjectPtr<UStaticMesh> SoftMesh = Settings->ResolvePreviewSoftMesh(TrainingOption);
	if (SoftMesh.IsNull())
	{
		const FString OptName = TrainingOption.GetTrainingName();
		RTSFunctionLibrary::ReportError("No training preview mesh mapped for option: " + OptName +
			"\nAdd a mapping in Project Settings -> Game -> Training Previews.");
	}
	return SoftMesh;
}

void FTrainingPreviewHelper::BeginAsyncLoadPreview(const TSoftObjectPtr<UStaticMesh>& SoftMesh, ANomadicVehicle* Nomad)
{
	if (not IsValid(M_TrainingPreview) || SoftMesh.IsNull())
	{
		return;
	}

	// Capture which option we are loading for; guards against out-of-order async completes.
	const FTrainingOption OptionBeingLoaded = M_ActivePreviewTrainingOption;

	TWeakObjectPtr<UStaticMeshComponent> WeakPreview = M_TrainingPreview;
	TWeakObjectPtr<ANomadicVehicle> WeakNomad = Nomad;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath Path = SoftMesh.ToSoftObjectPath();

	ResetAsyncState();

	M_StreamableHandle = Streamable.RequestAsyncLoad(
		Path,
		FStreamableDelegate::CreateLambda([ this, WeakPreview, WeakNomad, SoftMesh, OptionBeingLoaded ]()
		{
			if (not WeakPreview.IsValid() || not WeakNomad.IsValid())
			{
				return;
			}
			UStaticMeshComponent* StrongPreviewMeshComponent = WeakPreview.Get();
			ANomadicVehicle* StrongNomad = WeakNomad.Get();

			// If our active option changed while loading, ignore this callback.
			if (not (OptionBeingLoaded == this->M_ActivePreviewTrainingOption))
			{
				return;
			}

			// If we’re not in Building mode or should be hidden, set mesh but keep invisible.
			const bool bInBuilding = StrongNomad->GetNomadicStatus() == ENomadStatus::Building;
			UStaticMesh* Loaded = SoftMesh.Get();

			if (not IsValid(Loaded))
			{
				RTSFunctionLibrary::ReportError("Training preview failed to load soft mesh: " + SoftMesh.ToString());
				StrongPreviewMeshComponent->SetVisibility(false, true);
				return;
			}

			StrongPreviewMeshComponent->SetStaticMesh(Loaded);
			StrongPreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StrongPreviewMeshComponent->SetCanEverAffectNavigation(false);
			StrongPreviewMeshComponent->SetGenerateOverlapEvents(false);

			StrongPreviewMeshComponent->SetVisibility(bInBuilding && this->bM_ShouldBeVisible, true);
		})
	);
}

void FTrainingPreviewHelper::ResetPreviewState(const bool bHide)
{
	if (bHide)
	{
		SetPreviewVisibility(false);
	}

	// Clear any pending async load.
	ResetAsyncState();

	// Drop references so we don’t accidentally reuse a stale mesh/option.
	M_CachedSoftMesh.Reset();
	M_ActivePreviewTrainingOption.Reset();

	// Also clear the component’s mesh to avoid stale visuals when toggling visibility later.
	if (IsValid(M_TrainingPreview))
	{
		M_TrainingPreview->SetStaticMesh(nullptr);
	}
}
