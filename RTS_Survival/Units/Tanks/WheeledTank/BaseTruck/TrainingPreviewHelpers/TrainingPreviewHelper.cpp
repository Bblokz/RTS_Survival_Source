#include "TrainingPreviewHelper.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "TimerManager.h"
#include "TrainingPreviewSettings/TrainingPreviewSettings.h"

namespace TrainingPreview
{
	static const FName TrainingPreviewSocketName = TEXT("TrainingPreview");

	// Scalar on the progress material: 0 = nothing filled, 1 = the whole preview filled.
	static const FName BuildProgressParameterName = TEXT("BuildProgress");

	// 10 Hz keeps the fill line smooth without ticking the preview every frame.
	constexpr float ProgressUpdateIntervalSeconds = 0.1f;

	// UTrainerComponent pays for and advances its active item once per second.
	constexpr float TrainerQueueTickSeconds = 1.f;
}

void FTrainingPreviewHelper::SetupTrainingPreviewIfNotExisting(ANomadicVehicle* NomadTraining)
{
	if (not IsValid(NomadTraining) || not IsValid(NomadTraining->BuildingMeshComponent))
	{
		return;
	}
	M_Nomad = NomadTraining;

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
		// The progress fill picks up again from the trainer's current progress.
		StartProgressUpdates();
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
	StartProgressUpdates();
}

void FTrainingPreviewHelper::HandleTrainingDisabledOrNomadTruck()
{
	// Do not clear the active option; just hide the preview while training is disabled.
	SetPreviewVisibility(false);
	bM_ShouldBeVisible = false;
	StopProgressUpdates();
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

float FTrainingPreviewHelper::GetDisplayedBuildProgress(
	const float TrainedFraction,
	const int32 TotalTrainingTimeSeconds,
	const float SecondsSinceFractionChanged)
{
	const float ClampedFraction = FMath::Clamp(TrainedFraction, 0.f, 1.f);
	if (TotalTrainingTimeSeconds <= 0)
	{
		return ClampedFraction;
	}
	const float PartOfRunningTick = FMath::Clamp(
		SecondsSinceFractionChanged / TrainingPreview::TrainerQueueTickSeconds, 0.f, 1.f);
	return FMath::Clamp(
		ClampedFraction + PartOfRunningTick / static_cast<float>(TotalTrainingTimeSeconds), 0.f, 1.f);
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
	// The progress materials load with the mesh so the swap can happen in the same callback.
	TArray<FSoftObjectPath> PathsToLoad = {SoftMesh.ToSoftObjectPath()};
	if (const UTrainingPreviewSettings* Settings = GetSettingsCached())
	{
		PathsToLoad.Append(Settings->GetProgressMaterialPaths());
	}

	ResetAsyncState();

	M_StreamableHandle = Streamable.RequestAsyncLoad(
		PathsToLoad,
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
			this->ApplyProgressMaterial(StrongPreviewMeshComponent, Loaded);

			StrongPreviewMeshComponent->SetVisibility(bInBuilding && this->bM_ShouldBeVisible, true);
		})
	);
}

void FTrainingPreviewHelper::ApplyProgressMaterial(UStaticMeshComponent* PreviewComponent, const UStaticMesh* PreviewMesh)
{
	ClearProgressMaterial();
	const UTrainingPreviewSettings* Settings = GetSettingsCached();
	if (not IsValid(PreviewComponent) || not IsValid(PreviewMesh) || not IsValid(Settings))
	{
		return;
	}
	UMaterialInterface* PreviewMeshMaterial = PreviewMesh->GetMaterial(0);
	const TSoftObjectPtr<UMaterialInterface> SoftProgressMaterial = Settings->ResolveProgressMaterial(PreviewMeshMaterial);
	if (SoftProgressMaterial.IsNull())
	{
		// This preview keeps its own material (e.g. the Soviet previews).
		return;
	}
	UMaterialInterface* ProgressMaterial = SoftProgressMaterial.Get();
	if (not IsValid(ProgressMaterial))
	{
		RTSFunctionLibrary::ReportError("Training preview progress material failed to load: " +
			SoftProgressMaterial.ToString() + "\nCheck Project Settings -> Game -> Training Previews -> Progress Materials.");
		return;
	}

	M_ProgressState.ProgressMaterial = UMaterialInstanceDynamic::Create(ProgressMaterial, PreviewComponent);
	if (not GetHasProgressMaterial())
	{
		return;
	}
	// Every slot that uses the replaced material gets the progress fill (the previews have one slot).
	for (int32 SlotIndex = 0; SlotIndex < PreviewMesh->GetStaticMaterials().Num(); ++SlotIndex)
	{
		if (PreviewMesh->GetMaterial(SlotIndex) == PreviewMeshMaterial)
		{
			PreviewComponent->SetMaterial(SlotIndex, M_ProgressState.ProgressMaterial);
		}
	}
	// Start from the trainer's progress: the factory may redeploy halfway through a unit.
	M_ProgressState.LastTrainedFraction = -1.f;
	UpdateProgressMaterial();
}

void FTrainingPreviewHelper::ClearProgressMaterial()
{
	M_ProgressState.ProgressMaterial = nullptr;
	if (IsValid(M_TrainingPreview))
	{
		M_TrainingPreview->EmptyOverrideMaterials();
	}
}

bool FTrainingPreviewHelper::GetHasProgressMaterial() const
{
	return IsValid(M_ProgressState.ProgressMaterial);
}

void FTrainingPreviewHelper::StartProgressUpdates()
{
	ANomadicVehicle* Nomad = M_Nomad.Get();
	if (not IsValid(Nomad) || not Nomad->GetWorld())
	{
		return;
	}
	// A resume restarts the glide from the trainer's value instead of jumping ahead by the time spent packed up.
	M_ProgressState.LastTrainedFraction = -1.f;
	FTimerManager& TimerManager = Nomad->GetWorld()->GetTimerManager();
	if (TimerManager.IsTimerActive(M_ProgressState.UpdateTimerHandle))
	{
		return;
	}
	// Weak lambda on the nomad: the helper lives inside the nomad, so `this` is valid whenever the nomad is.
	TimerManager.SetTimer(
		M_ProgressState.UpdateTimerHandle,
		FTimerDelegate::CreateWeakLambda(Nomad, [this]()
		{
			UpdateProgressMaterial();
		}),
		TrainingPreview::ProgressUpdateIntervalSeconds,
		true);
}

void FTrainingPreviewHelper::StopProgressUpdates()
{
	const ANomadicVehicle* Nomad = M_Nomad.Get();
	if (not IsValid(Nomad) || not Nomad->GetWorld())
	{
		return;
	}
	Nomad->GetWorld()->GetTimerManager().ClearTimer(M_ProgressState.UpdateTimerHandle);
}

void FTrainingPreviewHelper::UpdateProgressMaterial()
{
	if (not GetHasProgressMaterial())
	{
		// Mesh still loading, or a preview without a progress material.
		return;
	}
	const ANomadicVehicle* Nomad = M_Nomad.Get();
	if (not IsValid(Nomad) || not IsValid(Nomad->M_TrainerComponent) || not Nomad->GetWorld())
	{
		return;
	}
	FTrainingOption ActiveOption;
	float TrainedFraction = 0.f;
	int32 TotalTrainingTime = 0;
	const bool bHasActiveItem = Nomad->M_TrainerComponent->GetActiveTrainingProgressDetails(
		ActiveOption, TrainedFraction, TotalTrainingTime);
	if (not bHasActiveItem || not (ActiveOption == M_ActivePreviewTrainingOption))
	{
		return;
	}

	const float WorldTime = Nomad->GetWorld()->GetTimeSeconds();
	if (not FMath::IsNearlyEqual(TrainedFraction, M_ProgressState.LastTrainedFraction))
	{
		M_ProgressState.LastTrainedFraction = TrainedFraction;
		M_ProgressState.LastFractionChangeTime = WorldTime;
	}
	const float DisplayedProgress = GetDisplayedBuildProgress(
		TrainedFraction, TotalTrainingTime, WorldTime - M_ProgressState.LastFractionChangeTime);
	M_ProgressState.ProgressMaterial->SetScalarParameterValue(
		TrainingPreview::BuildProgressParameterName, DisplayedProgress);
}

void FTrainingPreviewHelper::ResetPreviewState(const bool bHide)
{
	if (bHide)
	{
		SetPreviewVisibility(false);
	}

	// Clear any pending async load.
	ResetAsyncState();

	// The next unit starts its fill from zero with a fresh material.
	StopProgressUpdates();
	ClearProgressMaterial();

	// Drop references so we don’t accidentally reuse a stale mesh/option.
	M_CachedSoftMesh.Reset();
	M_ActivePreviewTrainingOption.Reset();

	// Also clear the component’s mesh to avoid stale visuals when toggling visibility later.
	if (IsValid(M_TrainingPreview))
	{
		M_TrainingPreview->SetStaticMesh(nullptr);
	}
}
