#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "RoadSplineActor.generated.h"

class USplineComponent;
class USplineMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UPhysicalMaterial;

UCLASS()
class RTS_SURVIVAL_API ARoadSplineActor : public AActor
{
	GENERATED_BODY()

public:
	ARoadSplineActor();

	// Called when properties are changed in the editor (or actor is spawned/modified)
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Spline component defining the road path. Designers can edit this spline in the level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spline")
	USplineComponent* RoadSpline;

	/** Mesh to be used for each spline segment of the road. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
	UStaticMesh* RoadMesh;

	/** Material override for the road mesh (for visual appearance). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
	UMaterialInterface* RoadMaterial;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
	bool bAllowDecals = true;


	// Editor function to force rebuild.
	UFUNCTION(CallInEditor, Category = "Spline")
	void RebuildRoad()
	{
		BuildRoad();
	}
	

	/** Forward axis used by the spline mesh (commonly X or Z, depending on mesh orientation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spline")
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

protected:
	/** Array holding the spline mesh components for each segment. Managed at construction time. */
	UPROPERTY()
	TArray<USplineMeshComponent*> SplineMeshComponents;

	void BuildRoad();

	virtual void BeginPlay() override;
};

