#include "EmbededTurretInterface.h"

float IEmbeddedTurretInterface::GetCurrentTurretAngle_Implementation() const
{
	return 0.0f;
}

void IEmbeddedTurretInterface::SetTurretAngle_Implementation(const float NewAngle)
{
}

void IEmbeddedTurretInterface::UpdateTargetPitch_Implementation(const float NewPitch)
{
}

bool IEmbeddedTurretInterface::TurnBase_Implementation(const float Degrees)
{
	return false;
}

void IEmbeddedTurretInterface::PlaySingleFireAnimation_Implementation(const int32 WeaponIndex)
{
}

void IEmbeddedTurretInterface::PlayBurstFireAnimation_Implementation(const int32 WeaponIndex)
{
}
