// UTechItem.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "TechItem.generated.h"

class UPlayerResourceManager;
class UW_TechItemDescription;
class UW_CostDisplay;
struct FTechCost;
class UPlayerTechManager;
enum class ETechnology : uint8;


USTRUCT(Blueprintable, BlueprintType)
struct FTechCost
{
	GENERATED_BODY()

	FTechCost() : M_TechTime(0.0f)
	{
	}

	FTechCost(const TMap<ERTSResourceType, int32>& TechCosts, const float TechTime) : M_ResourceCostsMap(TechCosts),
		M_TechTime(TechTime)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<ERTSResourceType, int32> M_ResourceCostsMap;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float M_TechTime;
};


UCLASS()
class RTS_SURVIVAL_API UTechItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UTechItem(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	/** Called when the tech item is clicked */
	UFUNCTION(BlueprintCallable, Category = "TechItem")
	void OnClickedTech();

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// The cost of this technology.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TechItem")
	FTechCost TechCost;

	virtual void NativeOnInitialized() override;

	// Called from bluerpints after initializing the display widget.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnChangeAllVisuals(const bool bHasBeenResearched = false);


	/** Blueprint event called when the Technology property changes,
	 * do not call individually but use OnChangeAllVisuals instead.*/
	UFUNCTION(BlueprintImplementableEvent, Category = "TechItem")
	void OnChangeTechItemVisuals(ETechnology NewTechnology, const bool bHasBeenResearched = false);

	/** The technology represented by this tech item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TechItem")
	ETechnology Technology;

	/** Array of tech items required before this tech can be researched */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TechItem")
	TArray<TObjectPtr<UTechItem>> Requirements;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UProgressBar* TechProgressBar;

	UPROPERTY(BlueprintReadWrite)
	UW_TechItemDescription* TechItemDescription;

private:
	/** Whether this technology has been researched */
	bool bM_HasBeenResearched;

	/** Reference to the player's tech manager */
	UPROPERTY()
	TObjectPtr<UPlayerTechManager> M_PlayerTechManager;

	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;


	/** Validates and retrieves the player tech manager */
	bool GetIsValidPlayerTechManager();

	/** Validates and retrieves the player resource manager */
	bool GetIsValidPlayerResourceManager();
	

	/** Starts the research process */
	void StartResearch(const float TechResearchTime);

	/** Updates the research progress */
	void UpdateResearchProgress();

	/** Called when research is completed */
	void OnResearchCompleted();

	/** Total time required for research */
	float M_ResearchTime;

	/** Time elapsed since research started */
	float M_ElapsedResearchTime;

	/** Timer handle for updating research progress */
	FTimerHandle M_ResearchTimerHandle;
};
