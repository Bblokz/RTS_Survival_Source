#include "FRTS_SceneManipulationLibrary.h"

#include "Components/DecalComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Resources/ResourceSceneSetup/ResourceSceneSetup.h"

float FRTS_SceneManipulationLibrary::SetCompRandomRotation(const float MinRotXY, const float MaxRotXY,
                                                         const float MinRotZ, const float MaxRotZ, USceneComponent* Component)
{
	
	if (not IsValid(Component))
	{
		return 0;
	}
	const float RandomRotXY = UKismetMathLibrary::RandomFloatInRange(MinRotXY, MaxRotXY);
	const float RandomRotZ = UKismetMathLibrary::RandomFloatInRange(MinRotZ, MaxRotZ);
	// Add to current rotation:
	FRotator NewRotation = Component->GetRelativeRotation();
	NewRotation.Yaw += RandomRotZ;
	NewRotation.Pitch += RandomRotXY;
	NewRotation.Roll += RandomRotXY;
	Component->SetRelativeRotation(NewRotation);
	return RandomRotXY;
}

float FRTS_SceneManipulationLibrary::SetCompRandomScale(const float MinScale, const float MaxScale,
	USceneComponent* Component)
{
	if (not IsValid(Component))
	{
		return 0;
	}

	// Random scale
	const float RandomScale = UKismetMathLibrary::RandomFloatInRange(MinScale, MaxScale);
	Component->SetWorldScale3D(FVector(RandomScale));
	return RandomScale;
}

float FRTS_SceneManipulationLibrary::SetComponentScaleOnAxis(const float MinScale, const float MaxScale,
	const bool bScaleX, const bool bScaleY, const bool bScaleZ, USceneComponent* Component)
{
	if (not IsValid(Component))
	{
		return 0;
	}
	const float RandomScale = UKismetMathLibrary::RandomFloatInRange(MinScale, MaxScale);
	FVector NewScale = Component->GetRelativeScale3D();
	if (bScaleX)
	{
		NewScale.X = RandomScale;
	}
	if (bScaleY)
	{
		NewScale.Y = RandomScale;
	}
	if (bScaleZ)
	{
		NewScale.Z = RandomScale;
	}
	Component->SetRelativeScale3D(NewScale);
	return RandomScale;
}

float FRTS_SceneManipulationLibrary::SetCompRandomOffset(const float MinOffsetXY, const float MaxOffsetXY,
	USceneComponent* Component)
{
	if (not IsValid(Component))
	{
		return 0;
	}
	const float RandomXOffset = UKismetMathLibrary::RandomFloatInRange(MinOffsetXY, MaxOffsetXY);
	const float RandomYOffset = UKismetMathLibrary::RandomFloatInRange(MinOffsetXY, MaxOffsetXY);
	FVector NewLocation = Component->GetRelativeLocation();
	NewLocation.X += RandomXOffset;
	NewLocation.Y += RandomYOffset;
	Component->SetRelativeLocation(NewLocation);
	return RandomXOffset;
}

TArray<int32> FRTS_SceneManipulationLibrary::SetRandomDecalsMaterials(
	const TArray<FWeightedDecalMaterial>& DecalMaterials, TArray<UDecalComponent*> DecalComponents)
{
	TArray<int32> ChosenIndices;
	if (DecalComponents.Num() == 0)
	{
		// If no options, all -1
		for (int32 i = 0; i < DecalComponents.Num(); i++)
		{
			ChosenIndices.Add(-1);
		}
		return ChosenIndices;
	}

	TArray<float> Weights;
	Weights.Reserve(DecalComponents.Num());
	for (const auto& Item : DecalMaterials)
	{
		Weights.Add(Item.Weight);
	}

	for (auto* EachComp : DecalComponents)
	{
		if (!IsValid(EachComp))
		{
			ChosenIndices.Add(-1);
			continue;
		}
		// Pick weighted index depending on probability distribution construction from the provided weights. 
		int32 DecalIndex = PickWeightedIndex(Weights);
		if (DecalIndex== -1)
		{
			ChosenIndices.Add(-1);
			continue;
		}

		UMaterialInterface* Decal = DecalMaterials[DecalIndex].Material;
		if (IsValid(Decal))
		{
			EachComp->SetMaterial(0, Decal);
			ChosenIndices.Add(DecalIndex);
		}
		else
		{
			ChosenIndices.Add(-1);
		}
	}

	return ChosenIndices;

}

TArray<int32> FRTS_SceneManipulationLibrary::SetRandomStaticMesh(const TArray<FWeightedStaticMesh>& Meshes,
	TArray<UStaticMeshComponent*> Components)
{
	TArray<int32> ChosenIndices;
	if (Meshes.Num() == 0)
	{
		// If no options, all -1
		for (int32 i = 0; i < Components.Num(); i++)
		{
			ChosenIndices.Add(-1);
		}
		return ChosenIndices;
	}

	TArray<float> Weights;
	Weights.Reserve(Meshes.Num());
	for (const auto& Item : Meshes)
	{
		Weights.Add(Item.Weight);
	}

	for (auto* EachComp : Components)
	{
		if (!IsValid(EachComp))
		{
			ChosenIndices.Add(-1);
			continue;
		}
		// Pick weighted index depending on probability distribution construction from the provided weights. 
		int32 MeshIndex = PickWeightedIndex(Weights);
		if (MeshIndex == -1)
		{
			ChosenIndices.Add(-1);
			continue;
		}

		UStaticMesh* ChosenMesh = Meshes[MeshIndex].Mesh;
		if (IsValid(ChosenMesh))
		{
			EachComp->SetStaticMesh(ChosenMesh);
			ChosenIndices.Add(MeshIndex);
		}
		else
		{
			ChosenIndices.Add(-1);
		}
	}

	return ChosenIndices;
	
}

int32 FRTS_SceneManipulationLibrary::PickWeightedIndex(const TArray<float>& Weights)
{
	
	// Sum weights
	float TotalWeight = 0.f;
	for (float W : Weights)
	{
		TotalWeight += FMath::Max(W, 0.f);
	}
	// If no weights, return -1
	if (TotalWeight <= 0.f)
	{
		return -1;
	}

	// Pick random in [0, TotalWeight)
	float RandomVal = FMath::FRandRange(0.f, TotalWeight);
	float Cumulative = 0.f;
	for (int32 i = 0; i < Weights.Num(); i++)
	{
		float ClampedW = FMath::Max(Weights[i], 0.f);
		Cumulative += ClampedW;
		if (RandomVal <= Cumulative)
		{
			return i;
		}
	}
	return -1; // Fallback
}

