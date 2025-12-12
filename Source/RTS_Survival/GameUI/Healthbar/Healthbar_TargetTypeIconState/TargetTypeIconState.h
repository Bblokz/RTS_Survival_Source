#pragma once

#include "CoreMinimal.h"

#include "TargetTypeIconState.generated.h"

class USlateBrushAsset;
enum class ETargetTypeIcon : uint8;
class UImage;

USTRUCT(BlueprintType)
struct FTargetTypeIconBrushes
{
	GENERATED_BODY()
	// The player brush used for the target type icon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USlateBrushAsset* PlayerBrush = nullptr;

	// The enemy brush used for the target type icon.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USlateBrushAsset* EnemyBrush= nullptr;
};

USTRUCT(Blueprintable)
struct FTargetTypeIconState
{
	GENERATED_BODY()

	bool GetImplementsTargetTypeIcon() const;

	void SetNewTargetTypeIcon(const int8 OwningPlayer, const ETargetTypeIcon NewTargetTypeIcon);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UImage* TargetTypeIconImage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ETargetTypeIcon, FTargetTypeIconBrushes> TypeToBrush;

private:
	bool IsTypeContainedAndValid(const ETargetTypeIcon Type);
};
