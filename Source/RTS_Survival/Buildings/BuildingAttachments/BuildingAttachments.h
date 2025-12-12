#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundCue.h"
#include "NiagaraSystem.h"

#include "BuildingAttachments.generated.h"

// A building attachment to attach to the building mesh once it is constructed.
USTRUCT(BlueprintType)
struct FBuildingAttachment
{
	GENERATED_BODY()

	FBuildingAttachment()
		: ActorToSpawn(nullptr),
		  SocketName(NAME_None),
		  Scale(FVector(1.0f, 1.0f, 1.0f))
	{
	}

	// Class reference to the skeletal mesh actor or any actor class you want to spawn and attach.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment")
	TSubclassOf<AActor> ActorToSpawn;

	// Name of the socket to attach the spawned actor to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment")
	FName SocketName;

	// The scale set to the spawned actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attachment")
	FVector Scale;
};

USTRUCT(BlueprintType)
struct FBuildingNiagaraAttachment
{
	GENERATED_BODY()

	FBuildingNiagaraAttachment()
		: NiagaraSystemToSpawn(nullptr),
		  SocketName(NAME_None),
		  Scale(FVector(1.0f, 1.0f, 1.0f))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Attachment")
	UNiagaraSystem* NiagaraSystemToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Attachment")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara Attachment")
	FVector Scale; 
};

USTRUCT(BlueprintType)
struct FBuildingSoundAttachment
{
	GENERATED_BODY()

	FBuildingSoundAttachment()
		: SoundCueToSpawn(nullptr),
		  SocketName(NAME_None)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Attachment")
	USoundCue* SoundCueToSpawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Attachment")
	FName SocketName;
};

USTRUCT(BlueprintType)
struct FBuildingNavModifierAttachment
{
	GENERATED_BODY()

	FBuildingNavModifierAttachment()
		: SocketName(NAME_None),
		  Scale(FVector(1.0f, 1.0f, 1.0f))
	{
	}

	// Name of the socket to attach the nav modifier to.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavModifierAttachment")
	FName SocketName;

	// The scale of the nav modifier area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavModifierAttachment")
	FVector Scale;
};
