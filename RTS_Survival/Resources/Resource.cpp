#include "Resource.h"

#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "ResourceComponent/ResourceComponent.h"
#include "ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Procedural/SceneManipulationLibrary/FRTS_SceneManipulationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

ACPPResourceMaster::ACPPResourceMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), M_AmountOfAttachedMeshes(0)
{
	PrimaryActorTick.bCanEverTick = false;
	ResourceComponent = CreateDefaultSubobject<UResourceComponent>(TEXT("ResourceComponent"));
}


void ACPPResourceMaster::OnResourceStorageChanged(const int32 PercentageResourcesFilled, const ERTSResourceType ResourceType)
{
	if (M_AmountOfAttachedMeshes <= 0)
	{
		OnResourceStorageChangedNoMeshes(PercentageResourcesFilled);
		return;
	}
	// Calculate the number of meshes that should remain based on the percentage of resources left.
	const int32 AmountOfMeshesToKeep = FMath::CeilToInt(M_AmountOfAttachedMeshes * (PercentageResourcesFilled / 100.f));

	// Calculate how many meshes should be removed.
	int32 AmountToRemove = AttachedMeshes.Num() + ManuallyAttachedMeshes.Num() - AmountOfMeshesToKeep;
	const int32 DebugAmountToRemove = AmountToRemove;

	// Remove generated meshes first.
	for (int32 i = AttachedMeshes.Num() - 1; i >= 0 && AmountToRemove > 0; --i)
	{
		if (IsValid(AttachedMeshes[i]))
		{
			AttachedMeshes[i]->DestroyComponent();
			AttachedMeshes.RemoveAt(i);
			--AmountToRemove;
		}
	}

	// Remove manually added meshes if necessary.
	for (int32 i = ManuallyAttachedMeshes.Num() - 1; i >= 0 && AmountToRemove > 0; --i)
	{
		if (IsValid(ManuallyAttachedMeshes[i]))
		{
			ManuallyAttachedMeshes[i]->DestroyComponent();
			ManuallyAttachedMeshes.RemoveAt(i);
			--AmountToRemove;
		}
	}

	// Adjust the member variable to reflect the new number of attached meshes.
	M_AmountOfAttachedMeshes = AttachedMeshes.Num() + ManuallyAttachedMeshes.Num();
	// For derived blueprints that handle this differently.
	Bp_OnResourceStorageChanged(PercentageResourcesFilled);
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		const FString DebugString = "Amount meshes removed: " + FString::FromInt(DebugAmountToRemove) +
			"\n Perentage : " + FString::FromInt(PercentageResourcesFilled);
		const FVector DebugLocation = GetActorLocation() + FVector(0.f, 0.f, 400.f);
		DrawDebugString(GetWorld(), DebugLocation, DebugString, nullptr, FColor::Red, 2.f, false);
	}
}

void ACPPResourceMaster::OnResourceStorageEmpty()
{
	BP_OnResourceStorageEmpty();
}

void ACPPResourceMaster::SetupResourceCollisions(TArray<UMeshComponent*> ResourceMeshComponents,
                                                 TArray<UGeometryCollectionComponent*> ResourceGeometryComponents) const
{
	for(const auto EachResourceMesh : ResourceMeshComponents)
	{
		FRTS_CollisionSetup::SetupResourceMeshCollision(EachResourceMesh);
	}
	for(auto EachResourceGeometryComponent : ResourceGeometryComponents)
	{
		FRTS_CollisionSetup::SetupDestructibleEnvActorGeometryComponentCollision(EachResourceGeometryComponent);
	}
}


void ACPPResourceMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

int32 ACPPResourceMaster::GenerateResourceAttachments(FResourceAttachmentsSetup AttachmentsSetup)
{
	ERTSResourceType ResourceTypeDebug = ERTSResourceType::Resource_None;
	int32 AmountSocketsFound = 0;
	int32 AmountMeshesCreated = 0;
	if (IsValid(ResourceComponent))
	{
		ResourceTypeDebug = ResourceComponent->ResourceType;
	}
	if (bKeepCurrentAttachments)
	{
		return 0;
	}
	if (IsValid(AttachmentsSetup.MainResourceMesh) && IsValid(AttachmentsSetup.MainResourceMesh->GetStaticMesh()))
	{
		if (AttachmentsSetup.AttachableResourceMeshes.Num() > 0)
		{
			ClearAttachments();
			const UStaticMesh* StaticMesh = AttachmentsSetup.MainResourceMesh->GetStaticMesh();

			// Retrieve all socket names from the static mesh
			TArray<FString> Sockets;
			for (const UStaticMeshSocket* Socket : StaticMesh->Sockets)
			{
				const bool IsHarvesterLocationSocket = Socket->SocketName.ToString().Contains("Harvest");
				if (IsValid(Socket) && Socket->SocketName.ToString() != "Core" && !IsHarvesterLocationSocket)
				{
					Sockets.Add(Socket->SocketName.ToString());
				}
			}
			AmountSocketsFound = Sockets.Num();

			// Calculate the number of attachments
			M_AmountOfAttachedMeshes = FMath::RandRange(AttachmentsSetup.MinAmountAttachments,
			                                            AttachmentsSetup.MaxAmountAttachments);

			for (int32 i = 0; i < M_AmountOfAttachedMeshes && !Sockets.IsEmpty(); ++i)
			{
				// Pick a random attachable resource mesh
				UStaticMesh* RandomMesh = AttachmentsSetup.AttachableResourceMeshes[FMath::RandRange(
					0, AttachmentsSetup.AttachableResourceMeshes.Num() - 1)];

				// Pick a random socket name
				FString RandomSocketName = Sockets[FMath::RandRange(0, Sockets.Num() - 1)];

				// Find the socket on the main resource mesh
				const UStaticMeshSocket* Socket = StaticMesh->FindSocket(FName(*RandomSocketName));
				if (IsValid(Socket))
				{
					// Create and attach the mesh component
					UStaticMeshComponent* NewAttachment = NewObject<UStaticMeshComponent>(this);
					if (IsValid(NewAttachment))
					{
						NewAttachment->SetStaticMesh(RandomMesh);
						NewAttachment->SetWorldScale3D(
							FVector(AttachmentsSetup.ScaleMultiplier * FMath::FRandRange(0.7f, 1.3f)));

						// Get the transform of the socket
						FTransform SocketTransform;
						Socket->GetSocketTransform(SocketTransform, AttachmentsSetup.MainResourceMesh);
						FRotator SocketRotation = SocketTransform.GetRotation().Rotator();

						// Apply a random yaw rotation
						SocketRotation.Yaw += FMath::FRandRange(-180.f, 180.f);

						NewAttachment->SetWorldLocationAndRotation(SocketTransform.GetLocation(), SocketRotation);
						NewAttachment->AttachToComponent(AttachmentsSetup.MainResourceMesh,
						                                 FAttachmentTransformRules::KeepWorldTransform);
						NewAttachment->RegisterComponent();
						AttachedMeshes.Add(NewAttachment);
						++AmountMeshesCreated;
					}
					else
					{
						RTSFunctionLibrary::ReportError("Could not generate new attachment for socket "
							+ RandomSocketName + " \n on resource " + GetName() + " of type " +
							Global_GetResourceTypeAsString(
								ResourceTypeDebug) + "!");
					}
				}
				else
				{
					RTSFunctionLibrary::ReportError("Could not find socket " + RandomSocketName + " on resource "
						+ GetName() + " of type " + Global_GetResourceTypeAsString(ResourceTypeDebug) + "!");
				}

				// Remove the used socket name to avoid overlapping attachments
				Sockets.RemoveSingleSwap(RandomSocketName);
			}
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("MainResourceMesh is not valid or has no static mesh assigned!"
			"\n On resource: " + GetName() + " of type: " + Global_GetResourceTypeAsString(ResourceTypeDebug));
	}
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		const FString Message = "Generated " + FString::FromInt(AmountMeshesCreated) + " meshes"
			"\n  on resource " + GetName() +
			"\n of type " + Global_GetResourceTypeAsString(ResourceTypeDebug) + " with " +
			FString::FromInt(AmountSocketsFound) + " sockets found.";
		const FVector DebugLocation = GetActorLocation() + FVector(0.f, 0.f, 400.f);
		DrawDebugString(GetWorld(), DebugLocation, Message, nullptr, FColor::Green, 5.f, false);
	}
	return AmountMeshesCreated;
}


void ACPPResourceMaster::AddToManuallyAddedMeshes(UStaticMeshComponent* MeshComponent)
{
	if (IsValid(MeshComponent))
	{
		ManuallyAttachedMeshes.Add(MeshComponent);
		M_AmountOfAttachedMeshes++;
	}
	else
	{
		RTSFunctionLibrary::ReportError("MeshComponent is not valid! "
			"\n On resource: " + GetName());
	}
}

float ACPPResourceMaster::SetCompRandomRotation(const float MinRotXY, const float MaxRotXY, const float MinRotZ,
                                                const float MaxRotZ, USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomRotation(MinRotXY, MaxRotXY, MinRotZ, MaxRotZ, Component);
}

float ACPPResourceMaster::SetCompRandomScale(const float MinScale, const float MaxScale,
                                             USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomScale(MinScale, MaxScale, Component);
}

float ACPPResourceMaster::SetComponentScaleOnAxis(const float MinScale, const float MaxScale, const bool bScaleX,
                                                  const bool bScaleY, const bool bScaleZ, USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetComponentScaleOnAxis(MinScale, MaxScale, bScaleX, bScaleY, bScaleZ, Component);
}

float ACPPResourceMaster::SetCompRandomOffset(const float MinOffsetXY, const float MaxOffsetXY,
                                              USceneComponent* Component)
{
	return FRTS_SceneManipulationLibrary::SetCompRandomOffset(MinOffsetXY, MaxOffsetXY, Component);
}

TArray<int32> ACPPResourceMaster::SetRandomDecalsMaterials(
	const TArray<FWeightedDecalMaterial>& DecalMaterials,
	TArray<UDecalComponent*> DecalComponents
)
{
	return FRTS_SceneManipulationLibrary::SetRandomDecalsMaterials(DecalMaterials, DecalComponents);
}


TArray<int32> ACPPResourceMaster::SetRandomStaticMesh(
	const TArray<FWeightedStaticMesh>& Meshes,
	TArray<UStaticMeshComponent*> Components
)
{
	return FRTS_SceneManipulationLibrary::SetRandomStaticMesh(Meshes, Components);
}

int32 ACPPResourceMaster::CalculateResourceWorthFromScale(
	const float BaseScale,
	const int32 BaseWorth,
	const float CurrentScale,
	const int32 NearestMultiple
)
{
	if (FMath::IsNearlyZero(BaseScale))
	{
		// Avoid divide-by-zero if BaseScale is 0 or extremely small
		UE_LOG(LogTemp, Warning, TEXT("BaseScale is near 0; defaulting to BaseWorth."));
		return BaseWorth;
	}
	const float ScaleRatio = CurrentScale / BaseScale;
	const float NewWorthFloat = static_cast<float>(BaseWorth) * ScaleRatio;

	int32 FinalWorth = FMath::RoundToInt(NewWorthFloat);

	// If NearestMultiple <= 1, no extra rounding is needed
	if (NearestMultiple <= 1)
	{
		return FinalWorth;
	}
	const int32 Remainder = FinalWorth % NearestMultiple;
	const int32 HalfMultiple = NearestMultiple / 2;

	if (FMath::Abs(Remainder) >= HalfMultiple)
	{
		FinalWorth = FinalWorth - Remainder + (Remainder > 0 ? NearestMultiple : -NearestMultiple);
	}
	else
	{
		FinalWorth = FinalWorth - Remainder;
	}
	return FinalWorth;
}

void ACPPResourceMaster::CollapseMesh(UGeometryCollectionComponent* GeoCollapseComp,
                                      const TSoftObjectPtr<UGeometryCollection> GeoCollection,
                                      UMeshComponent* MeshToCollapse,
                                      const FCollapseDuration CollapseDuration, const FCollapseForce CollapseForce,
                                      const FCollapseFX CollapseFX)
{
	FRTS_Collapse::CollapseMesh(
		this, GeoCollapseComp, GeoCollection, MeshToCollapse, CollapseDuration, CollapseForce, CollapseFX
	);
}


void ACPPResourceMaster::ClearAttachments()
{
	for (auto Attachment : AttachedMeshes)
	{
		if (IsValid(Attachment))
		{
			Attachment->DestroyComponent();
		}
	}
	AttachedMeshes.Empty();
}

void ACPPResourceMaster::OnResourceStorageChangedNoMeshes(const int32 PercentageResourcesFilled)
{
	RTSFunctionLibrary::PrintString("BP STORAGE CHANGE!");
	Bp_OnResourceStorageChanged(PercentageResourcesFilled);
}


