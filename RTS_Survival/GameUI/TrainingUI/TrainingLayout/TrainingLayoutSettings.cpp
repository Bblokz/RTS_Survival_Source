// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrainingLayoutSettings.h"

#include "TrainingLayoutDataAsset.h"

const UTrainingLayoutDataAsset* UTrainingLayoutSettings::LoadTrainingLayout() const
{
	if (const UTrainingLayoutDataAsset* LoadedLayout = TrainingLayout.LoadSynchronous())
	{
		return LoadedLayout;
	}
	return GetDefault<UTrainingLayoutDataAsset>();
}
