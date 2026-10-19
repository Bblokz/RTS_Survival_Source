// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "PitMG42VisualComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * @brief Added to an MG42 pit turret to retain the DShK animation rig while presenting an MG42 weapon.
 * The proxy stays responsible for aiming and firing; only its rendered mesh is replaced per turret instance.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPitMG42VisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPitMG42VisualComponent();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Pit MG42 Visual", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USkeletalMesh> M_MG42Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "Pit MG42 Visual", meta = (AllowPrivateAccess = "true"))
	FTransform M_MG42RelativeTransform = FTransform(
		FRotator::ZeroRotator,
		FVector::ZeroVector,
		FVector(0.2f));

	UPROPERTY(EditDefaultsOnly, Category = "Pit MG42 Visual", meta = (AllowPrivateAccess = "true"))
	FName M_ProxyComponentName = TEXT("Turret");

	UPROPERTY(EditDefaultsOnly, Category = "Pit MG42 Visual", meta = (AllowPrivateAccess = "true"))
	FName M_AttachmentBoneName = TEXT("Gun");

	USkeletalMeshComponent* FindProxyTurretMesh() const;
	void CreateMG42Visual(USkeletalMeshComponent& ProxyTurretMesh, USkeletalMesh& MG42Mesh) const;
};
