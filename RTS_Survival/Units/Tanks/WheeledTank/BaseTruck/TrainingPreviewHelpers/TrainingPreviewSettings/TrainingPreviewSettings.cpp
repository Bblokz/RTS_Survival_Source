#include "TrainingPreviewSettings.h"

#include "Materials/MaterialInterface.h"

namespace TrainingPreviewSettingsDefaults
{
	// The German previews (MI_WhileConstructing) get the holo progress fill. DefaultGame.ini sets the same pair; this
	// default keeps it when an editor that predates the property rewrites the section without it.
	const TCHAR* const GermanPreviewMeshMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_WhileConstructing.MI_WhileConstructing");
	const TCHAR* const GermanProgressMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/TrainingPreview/"
		"MI_TrainingPreview_Holo_Ger.MI_TrainingPreview_Holo_Ger");
}

UTrainingPreviewSettings::UTrainingPreviewSettings()
{
	CategoryName = TEXT("Game");
	SectionName  = TEXT("Training Previews");

	FTrainingPreviewProgressMaterial GermanProgressMaterial;
	GermanProgressMaterial.PreviewMeshMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TrainingPreviewSettingsDefaults::GermanPreviewMeshMaterial));
	GermanProgressMaterial.ProgressMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TrainingPreviewSettingsDefaults::GermanProgressMaterial));
	ProgressMaterials.Add(GermanProgressMaterial);
}

TSoftObjectPtr<UMaterialInterface> UTrainingPreviewSettings::ResolveProgressMaterial(
	const UMaterialInterface* PreviewMeshMaterial) const
{
	if (not IsValid(PreviewMeshMaterial))
	{
		return TSoftObjectPtr<UMaterialInterface>();
	}
	const FSoftObjectPath PreviewMeshMaterialPath(PreviewMeshMaterial);
	for (const FTrainingPreviewProgressMaterial& Entry : ProgressMaterials)
	{
		if (Entry.PreviewMeshMaterial.ToSoftObjectPath() == PreviewMeshMaterialPath)
		{
			return Entry.ProgressMaterial;
		}
	}
	return TSoftObjectPtr<UMaterialInterface>();
}

TArray<FSoftObjectPath> UTrainingPreviewSettings::GetProgressMaterialPaths() const
{
	TArray<FSoftObjectPath> Paths;
	for (const FTrainingPreviewProgressMaterial& Entry : ProgressMaterials)
	{
		if (not Entry.ProgressMaterial.IsNull())
		{
			Paths.AddUnique(Entry.ProgressMaterial.ToSoftObjectPath());
		}
	}
	return Paths;
}

TSoftObjectPtr<UStaticMesh> UTrainingPreviewSettings::ResolvePreviewSoftMesh(const FTrainingOption& Option) const
{
	const FName KeyName = FName(*Option.GetTrainingName());
	if (const TSoftObjectPtr<UStaticMesh>* Found = OptionNameToPreviewMesh.Find(KeyName))
	{
		return *Found;
	}
	return TSoftObjectPtr<UStaticMesh>();
}
