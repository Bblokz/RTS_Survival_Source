// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftCannonPodsEditorLibrary.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"

bool UAircraftCannonPodsEditorLibrary::SetSkeletalMeshSocket(USkeletalMesh* Mesh, const FName SocketName,
                                                             const FName BoneName, const FVector RelativeLocation,
                                                             const FRotator RelativeRotation)
{
#if WITH_EDITOR
	if (not IsValid(Mesh) || SocketName.IsNone())
	{
		return false;
	}
	USkeletalMeshSocket* Socket = Mesh->FindSocket(SocketName);
	const bool bIsNewSocket = Socket == nullptr;
	if (bIsNewSocket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Mesh);
		Socket->SocketName = SocketName;
	}
	Mesh->Modify();
	// AddSocket refuses a socket whose bone is not on the mesh, so the bone is set first.
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = RelativeLocation;
	Socket->RelativeRotation = RelativeRotation;
	Socket->RelativeScale = FVector::OneVector;
	if (bIsNewSocket)
	{
		constexpr bool bAddToSkeleton = false;
		Mesh->AddSocket(Socket, bAddToSkeleton);
	}
	Mesh->RebuildSocketMap();
	return Mesh->FindSocket(SocketName) != nullptr;
#else
	return false;
#endif
}
