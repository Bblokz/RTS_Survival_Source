#include "TrainingPreviewSettings.h"

UTrainingPreviewSettings::UTrainingPreviewSettings()
{
	CategoryName = TEXT("Game");
	SectionName  = TEXT("Training Previews");
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
