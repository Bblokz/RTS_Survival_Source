#include "CrewPosition.h"

ECrewPositionType UCrewPosition::GetCrewPositionType() const
{
	return M_CrewPositionType;
}

FTransform UCrewPosition::GetCrewWorldTransform() const
{
	return GetComponentTransform();
}

float UCrewPosition::GetAcceptanceRadius() const
{
	return M_AcceptanceRadius;
}
