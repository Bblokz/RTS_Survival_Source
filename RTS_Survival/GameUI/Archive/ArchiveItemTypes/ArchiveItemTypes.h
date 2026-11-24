#pragma once


#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "ArchiveItemTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSArchiveItem :uint8
{
	None,
	// If the archive item is a unit, then to determine if this item is already in the archive you need to check the training option
	// set on it.
	Unit,
	Radixite,
	Metal,
	VehicleParts,
	ArmorMechanic,
	RotateTowards,
	SwitchingWeapons,
	MovingTheCamera,
	PlatedSovietTanks,
	NomadicBuildings,
	DifferentAmmoTypes,
	UnitExperience,
	// Text and picture come with the item (FArchiveCustomContent) instead of DT_ArchiveItem, e.g. from an info takeover.
	CustomContent,
};

static FName Global_GetArchiveItemName(const ERTSArchiveItem Item)
{
	const FName Name = StaticEnum<ERTSArchiveItem>()->GetNameByValue((int64)Item);
	const FName CleanName = FName(*Name.ToString().RightChop(FString("ERTSArchiveItem::").Len()));
	return CleanName;
}

// Used to sort archive items into categories.
UENUM(BlueprintType)
enum class ERTSArchiveType : uint8
{
	None,
	GameMechanics,
	Lore,
	Units
};

/**
 * @brief An archive entry that carries its own title, description and picture, so systems such as the info takeovers
 * can archive what they showed without a DT_ArchiveItem row. Entries with the same Key are added once.
 */
USTRUCT(BlueprintType)
struct FArchiveCustomContent
{
	GENERATED_BODY()

	// Identifies the entry; a second entry with the same key is ignored.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FName Key;

	// Plain text; the archive wraps it in its title style.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FText Title;

	// Rich text with the archive's <Text_...> tags.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FText Description;

	// Used when set; otherwise ImageBrush.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	TObjectPtr<UTexture2D> ImageTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	FSlateBrush ImageBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	bool bHasImage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archive")
	ERTSArchiveType SortType = ERTSArchiveType::Units;
};
