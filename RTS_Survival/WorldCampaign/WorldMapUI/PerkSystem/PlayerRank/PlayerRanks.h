#pragma once

#include "CoreMinimal.h"

#include "PlayerRanks.generated.h"

UENUM(BlueprintType)
enum class ERTSPlayerRank : uint8
{
    Corporal		UMETA(DisplayName = "Corporal"),
    Sergeant		UMETA(DisplayName = "Sergeant"),
    StaffSergeant     UMETA(DisplayName = "Staff Sergeant"),
    SergeantFirstClass     UMETA(DisplayName = "Sergeant First Class"),
    SergeantMajor    UMETA(DisplayName = "Sergeant Major"),
    SecondLieutenant     UMETA(DisplayName = "Second Lieutenant"),
    FirstLieutenant     UMETA(DisplayName = "First Lieutenant"),
    Captain     UMETA(DisplayName = "Captain"),
    Major     UMETA(DisplayName = "Major"),
    LieutenantColonel     UMETA(DisplayName = "Lieutenant Colonel"),
    Colonel     UMETA(DisplayName = "Colonel"),
    BrigadierGeneral     UMETA(DisplayName = "Brigadier General"),
    MajorGeneral     UMETA(DisplayName = "Major General"),
    LieutenantGeneral     UMETA(DisplayName = "Lieutenant General"),
    General     UMETA(DisplayName = "General"),
    FieldMarshal     UMETA(DisplayName = "Field Marshal"),
};

USTRUCT(BlueprintType)
struct FPlayerRankProgress
{
	GENERATED_BODY()
	
};