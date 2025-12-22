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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USlateBrushAsset> PlayerBrush = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USlateBrushAsset> EnemyBrush = nullptr;
};

USTRUCT(Blueprintable)
struct FTargetTypeIconState
{
	GENERATED_BODY()

	bool GetImplementsTargetTypeIcon() const;

	void SetNewTargetTypeIcon(const int8 OwningPlayer, const ETargetTypeIcon NewTargetTypeIcon);

	UPROPERTY(Transient)
	TWeakObjectPtr<UImage> TargetTypeIconImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ETargetTypeIcon, FTargetTypeIconBrushes> TypeToBrush;

private:
	bool IsTypeContainedAndValid(const ETargetTypeIcon Type);
};
