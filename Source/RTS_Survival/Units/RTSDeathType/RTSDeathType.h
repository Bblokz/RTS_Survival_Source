#pragma once

#include "CoreMinimal.h"

#include "RTSDeathType.generated.h"

UENUM(BlueprintType, Blueprintable)
enum class ERTSDeathType : uint8
{
	Kinetic,
	FireOrLaser,
	Radiation,
	Scavenging
};

static FString Global_GetDeathTypeString(const ERTSDeathType DeathType)
{
	const auto Enum = StaticEnum<ERTSDeathType>();
	if (Enum)
	{
		const FName Name = Enum->GetNameByValue(static_cast<int64>(DeathType));
		const FName CleanName = FName(*Name.ToString().RightChop(FString("ERTSDeathType::").Len()));
		return CleanName.ToString();
	}
	return FString("INVALID DEATH TYPE ENUM");
}
