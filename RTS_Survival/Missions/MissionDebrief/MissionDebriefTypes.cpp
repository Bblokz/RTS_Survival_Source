// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionDebriefTypes.h"

float UMissionDebriefBurnField::SampleField(const EMissionDebriefOutcome Outcome, const float U, const float V) const
{
	const TArray<float>& Field = GetField(Outcome);
	if (not GetHasGrid(Outcome))
	{
		return 0.f;
	}
	const float X = FMath::Clamp(U, 0.f, 1.f) * static_cast<float>(GridWidth - 1);
	const float Y = FMath::Clamp(V, 0.f, 1.f) * static_cast<float>(GridHeight - 1);
	const int32 X0 = FMath::Min(FMath::FloorToInt32(X), GridWidth - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt32(Y), GridHeight - 2);
	const float Fx = X - static_cast<float>(X0);
	const float Fy = Y - static_cast<float>(Y0);
	const float A = Field[Y0 * GridWidth + X0];
	const float B = Field[Y0 * GridWidth + X0 + 1];
	const float C = Field[(Y0 + 1) * GridWidth + X0];
	const float D = Field[(Y0 + 1) * GridWidth + X0 + 1];
	return FMath::Lerp(FMath::Lerp(A, B, Fx), FMath::Lerp(C, D, Fx), Fy);
}

bool UMissionDebriefBurnField::GetHasGrid(const EMissionDebriefOutcome Outcome) const
{
	return GridWidth > 1 && GridHeight > 1 && GetField(Outcome).Num() == GridWidth * GridHeight;
}

const TArray<float>& UMissionDebriefBurnField::GetField(const EMissionDebriefOutcome Outcome) const
{
	return Outcome == EMissionDebriefOutcome::Victory ? VictoryField : DefeatField;
}
