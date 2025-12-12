#include "EnvironmentActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "RTS_Survival/Procedural/SceneManipulationLibrary/FRTS_SceneManipulationLibrary.h"

AEnvironmentActor::AEnvironmentActor(const FObjectInitializer& ObjectInitializer)
	: AActorObjectsMaster(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnvironmentActor::BeginPlay()
{
	Super::BeginPlay();
}

float AEnvironmentActor::SetCompRandomRotation(const float MinRotXY, const float MaxRotXY, const float MinRotZ,
	const float MaxRotZ, USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomRotation(MinRotXY, MaxRotXY, MinRotZ, MaxRotZ, Component);
}

float AEnvironmentActor::SetCompRandomScale(const float MinScale, const float MaxScale, USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomScale(MinScale, MaxScale, Component);
}

float AEnvironmentActor::SetComponentScaleOnAxis(const float MinScale, const float MaxScale, const bool bScaleX,
	const bool bScaleY, const bool bScaleZ, USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetComponentScaleOnAxis(MinScale, MaxScale, bScaleX, bScaleY, bScaleZ, Component);
}

float AEnvironmentActor::SetCompRandomOffset(const float MinOffsetXY, const float MaxOffsetXY,
	USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomOffset(MinOffsetXY, MaxOffsetXY, Component);
}

TArray<int32> AEnvironmentActor::SetRandomDecalsMaterials(const TArray<FWeightedDecalMaterial>& DecalMaterials,
	TArray<UDecalComponent*> DecalComponents)
{
	return FRTS_SceneManipulationLibrary::SetRandomDecalsMaterials(DecalMaterials, DecalComponents);
}

TArray<int32> AEnvironmentActor::SetRandomStaticMesh(const TArray<FWeightedStaticMesh>& Meshes,
	TArray<UStaticMeshComponent*> Components)
{
	return FRTS_SceneManipulationLibrary::SetRandomStaticMesh(Meshes, Components);
}


