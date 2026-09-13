#include "AnimatedIconTypes.h"

bool FRTSVerticalAnimIconSettings::IsUsable() const
{
	return FMath::IsFinite(VisibleDuration) && FMath::IsFinite(FadeOutDuration)
		&& FMath::IsFinite(DeltaZ) && VisibleDuration >= 0.0f && FadeOutDuration >= 0.0f
		&& (static_cast<double>(VisibleDuration) + FadeOutDuration) > 0.0;
}

bool FRTSVerticalAnimIconSettings::Evaluate(const double ElapsedSeconds, float& OutWorldOffsetZ,
	float& OutOpacity) const
{
	OutWorldOffsetZ = 0.0f;
	OutOpacity = 0.0f;
	if (not IsUsable() || not FMath::IsFinite(ElapsedSeconds))
	{
		return false;
	}

	const double TotalDuration = static_cast<double>(VisibleDuration) + FadeOutDuration;
	const double Elapsed = FMath::Max(0.0, ElapsedSeconds);
	OutWorldOffsetZ = DeltaZ * static_cast<float>(FMath::Clamp(Elapsed / TotalDuration, 0.0, 1.0));
	if (Elapsed >= TotalDuration)
	{
		return false;
	}

	OutOpacity = 1.0f;
	if (Elapsed >= VisibleDuration && FadeOutDuration > 0.0f)
	{
		OutOpacity = static_cast<float>(1.0 - (Elapsed - VisibleDuration) / FadeOutDuration);
	}
	return true;
}
