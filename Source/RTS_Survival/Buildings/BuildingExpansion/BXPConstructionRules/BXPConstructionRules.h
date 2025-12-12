#pragma once

#include "CoreMinimal.h"


#include "BXPConstructionRules.generated.h"

UENUM()
enum class EBxpConstructionType : uint8
{
	None,
	// This type of bxp can be placed anywhere in the build radius.
	Free,
	// This bxp attaches to the building on a BXP socket.
	Socket,
	// This type of bxp can only be placed at the origin of a building.
	// This means that the bxp is unique as it can only be placed once per building (Causes overlapping otherwise).
	AtBuildingOrigin,
};

static FString Global_GetBxpConstructionTypeEnumAsString(const EBxpConstructionType Type)
{
	switch (Type)
	{
	case EBxpConstructionType::None:
		return "None";
	case EBxpConstructionType::Free:
		return "Free";
	case EBxpConstructionType::Socket:
		return "Socket";
	case EBxpConstructionType::AtBuildingOrigin:
		return "AtBuildingOrigin";
	}
	return "Unknown BXP Construction Type";
}

USTRUCT(BlueprintType)
struct FBuildingFootprint
{
	GENERATED_BODY()

	// Full width of the footprint in Unreal units (X axis in world/projection space).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Construction|Footprint", meta=(ClampMin="0.0"))
	float SizeX = 0.f;

	// Full height of the footprint in Unreal units (Y axis in world/projection space).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Construction|Footprint", meta=(ClampMin="0.0"))
	float SizeY = 0.f;

	// Helper
	bool IsSet() const { return (SizeX > KINDA_SMALL_NUMBER) && (SizeY > KINDA_SMALL_NUMBER); }
};

USTRUCT(BlueprintType)
struct FBxpConstructionRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Construction|Footprint")
	FBuildingFootprint Footprint;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	EBxpConstructionType ConstructionType = EBxpConstructionType::None;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool bUseCollision = false;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	bool bAffectNavMesh = true;

	// The name of the socket this bxp is attached to.
	UPROPERTY(BlueprintReadOnly)
	FName SocketName = NAME_None;

	// convenience constructors (keep your existing ones; add defaulted Footprint)
	FBxpConstructionRules() = default;

	FBxpConstructionRules(EBxpConstructionType InType,
	                      bool bInUseCollision,
	                      bool bAffectNavMesh,
	                      FName InSocketName,
	                      FBuildingFootprint InFootprint = FBuildingFootprint())
		: Footprint(InFootprint),
		  ConstructionType(InType),
		  bUseCollision(bInUseCollision),
		  bAffectNavMesh(bAffectNavMesh),
		  SocketName(InSocketName)
	{
	}
};
