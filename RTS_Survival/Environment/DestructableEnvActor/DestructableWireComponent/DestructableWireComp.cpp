// DestructableWireComp.cpp

#include "DestructableWireComp.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UDestructibleWire::UDestructibleWire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDestructibleWire::OnRegister()
{
	Super::OnRegister();

	if (AActor* Owner = GetOwner())
	{
		Owner->OnDestroyed.AddUniqueDynamic(this, &UDestructibleWire::HandleOwnerDestroyed);
	}
}

void UDestructibleWire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnOwningActorDies();
	Super::EndPlay(EndPlayReason);
}

void UDestructibleWire::HandleOwnerDestroyed(AActor* /*DestroyedActor*/)
{
	OnOwningActorDies();
}

void UDestructibleWire::InitWireComp(UStaticMeshComponent* MyWireMesh, const TArray<float>& DroopPerCurve)
{
	M_WireMeshConnectFrom = MyWireMesh;
	M_CurveDepths = DroopPerCurve;
}

void UDestructibleWire::OnOwningActorDies()
{
	if (M_bTeardownDone)
	{
		return;
	}

	M_bTeardownDone = true;
	NotifyAndRemoveAllLinks();
}

void UDestructibleWire::SetupWireConnection(
	UDestructibleWire* ChildWireComp,
	int32 MaxAmountWires,
	UStaticMesh* WireMesh,
	const FVector& WireMeshScale
)
{
	if (!ChildWireComp || !ChildWireComp->EnsureWireMeshIsValid() || !EnsureWireMeshIsValid())
	{
		Wire_Error(TEXT("Failed to create wire mesh connection. Invalid setup or missing InitWireComp."));
		return;
	}

	// Early clamp by physical capacity on *this* end (per-link clamp still applied below).
	const int32 PhysicalSockets = GetAmountWiresSocketsLeft();
	if (PhysicalSockets == 0)
	{
		Wire_Error(TEXT("No sockets present for a new wire connection."));
		return;
	}
	if (MaxAmountWires > PhysicalSockets)
	{
		Wire_Debug(FString::Printf(TEXT("Requested %d wires but only %d sockets physically present; clamping."),
			MaxAmountWires, PhysicalSockets));
		MaxAmountWires = PhysicalSockets;
	}

	// Build wires for this *pair*, excluding sockets already used with this counterpart.
	TArray<FName> ThisSockets;
	TArray<FName> OtherSockets;
	TArray<USplineComponent*> NewSplines;
	TArray<USplineMeshComponent*> NewSplineMeshes;

	Wire_CreateSplineWires_Internal(
		M_WireMeshConnectFrom,
		ChildWireComp->M_WireMeshConnectFrom,
		MaxAmountWires,
		WireMesh,
		M_CurveDepths,
		WireMeshScale,
		/*OtherForPair*/ ChildWireComp,
		ThisSockets,
		OtherSockets,
		NewSplines,
		NewSplineMeshes
	);

	if (ThisSockets.Num() == 0)
	{
		Wire_Debug(TEXT("No pair-available sockets to place wires."));
		return;
	}

	// Record link locally (this side created the meshes).
	FWireLink Link;
	Link.Other = ChildWireComp;
	Link.ThisSockets = ThisSockets;
	Link.OtherSockets = OtherSockets;
	Link.Splines = NewSplines;
	Link.SplineMeshes = NewSplineMeshes;
	M_Links.Add(MoveTemp(Link));

	// Register backlink on the child (sockets only; it didn't spawn our meshes).
	ChildWireComp->RegisterBackLinkFromOther(this, /*MySockets*/ OtherSockets, /*OtherSockets*/ ThisSockets);
}

int32 UDestructibleWire::GetAmountWiresSocketsLeft() const
{
	// "Left" here is physical capacity — we allow other poles to reuse these sockets,
	// so this is not reduced by existing links with other counterparts.
	if (!IsValid(M_WireMeshConnectFrom))
	{
		return 0;
	}

	TArray<FName> AllSockets;
	Wire_GetEnSockets(M_WireMeshConnectFrom, AllSockets);
	return AllSockets.Num();
}

bool UDestructibleWire::EnsureWireMeshIsValid() const
{
	if (IsValid(M_WireMeshConnectFrom))
	{
		return true;
	}

	Wire_Error(TEXT("Wire component has no valid source mesh; call InitWireComp first."));
	return false;
}

void UDestructibleWire::CreateSplineWires(
	UStaticMeshComponent* CompA,
	UStaticMeshComponent* CompB,
	int32 AmountOfWires,
	UStaticMesh* WireMesh,
	const TArray<float>& CurveDepths,
	const FVector& WireMeshScale
)
{
	// Legacy wrapper (pair-agnostic). Uses full socket sets on both sides.
	TArray<FName> DummyA, DummyB;
	TArray<USplineComponent*> DummySplines;
	TArray<USplineMeshComponent*> DummyMeshes;

	Wire_CreateSplineWires_Internal(
		CompA, CompB, AmountOfWires, WireMesh, CurveDepths, WireMeshScale,
		/*OtherForPair*/ nullptr, DummyA, DummyB, DummySplines, DummyMeshes
	);
}

void UDestructibleWire::RemoveAllWires()
{
	OnOwningActorDies();
}

// === Internal helpers ===

void UDestructibleWire::RegisterBackLinkFromOther(
	UDestructibleWire* Other,
	const TArray<FName>& MySockets,
	const TArray<FName>& OtherSockets
)
{
	if (!Other)
	{
		return;
	}

	FWireLink Back;
	Back.Other = Other;
	Back.ThisSockets = MySockets;
	Back.OtherSockets = OtherSockets;
	// No meshes created by this side for this link.
	M_Links.Add(MoveTemp(Back));
}

void UDestructibleWire::RemoveConnectionTo(UDestructibleWire* Other)
{
	if (!Other)
	{
		return;
	}

	for (int32 i = M_Links.Num() - 1; i >= 0; --i)
	{
		FWireLink& Link = M_Links[i];
		if (Link.Other.Get() != Other)
		{
			continue;
		}

		// Destroy meshes/splines we created for this link (if any).
		for (USplineMeshComponent* SM : Link.SplineMeshes)
		{
			if (SM)
			{
				SM->DestroyComponent();
			}
			M_SplineMeshComponents.Remove(SM);
		}
		for (USplineComponent* SC : Link.Splines)
		{
			if (SC)
			{
				SC->DestroyComponent();
			}
			M_SplineComponents.Remove(SC);
		}

		M_Links.RemoveAtSwap(i, 1, EAllowShrinking::No);
	}
}

void UDestructibleWire::NotifyAndRemoveAllLinks()
{
	// Tell counterparts to remove any connection to us first (so their meshes go away too).
	for (const FWireLink& Link : M_Links)
	{
		if (UDestructibleWire* Other = Link.Other.Get())
		{
			Other->RemoveConnectionTo(this);
		}
	}

	// Now clear local meshes/splines and all bookkeeping.
	Wire_ClearAllWires();
	M_Links.Empty();
}

void UDestructibleWire::Wire_ClearAllWires()
{
	for (USplineMeshComponent* SM : M_SplineMeshComponents)
	{
		if (SM)
		{
			SM->DestroyComponent();
		}
	}
	M_SplineMeshComponents.Empty();

	for (USplineComponent* SC : M_SplineComponents)
	{
		if (SC)
		{
			SC->DestroyComponent();
		}
	}
	M_SplineComponents.Empty();
}

void UDestructibleWire::Wire_CollectSocketsUsedWithCounterpart(
	const UDestructibleWire* Other,
	TSet<FName>& OutThisUsed,
	TSet<FName>& OutOtherUsed
) const
{
	OutThisUsed.Reset();
	OutOtherUsed.Reset();

	if (!Other)
	{
		return;
	}

	for (const FWireLink& Link : M_Links)
	{
		if (Link.Other.Get() == Other)
		{
			for (const FName& S : Link.ThisSockets)
			{
				OutThisUsed.Add(S);
			}
			for (const FName& S : Link.OtherSockets)
			{
				OutOtherUsed.Add(S);
			}
		}
	}
}

void UDestructibleWire::Wire_GetAvailableSocketsForPair(
	UStaticMeshComponent* CompA,
	UStaticMeshComponent* CompB,
	const UDestructibleWire* Other,
	TArray<FName>& OutAvailA,
	TArray<FName>& OutAvailB
) const
{
	OutAvailA.Empty();
	OutAvailB.Empty();

	if (!CompA || !CompB)
	{
		return;
	}

	TArray<FName> AllA, AllB;
	Wire_GetEnSockets(CompA, AllA);
	Wire_GetEnSockets(CompB, AllB);

	// Exclude sockets already used *with this counterpart only*.
	TSet<FName> ThisUsed, OtherUsed;
	Wire_CollectSocketsUsedWithCounterpart(Other, ThisUsed, OtherUsed);

	for (const FName& S : AllA)
	{
		if (!ThisUsed.Contains(S))
		{
			OutAvailA.Add(S);
		}
	}
	for (const FName& S : AllB)
	{
		if (!OtherUsed.Contains(S))
		{
			OutAvailB.Add(S);
		}
	}
}

void UDestructibleWire::Wire_CreateSplineWires_Internal(
	UStaticMeshComponent* CompA,
	UStaticMeshComponent* CompB,
	int32 AmountOfWires,
	UStaticMesh* WireMesh,
	const TArray<float>& CurveDepths,
	const FVector& WireMeshScale,
	const UDestructibleWire* OtherForPair,
	TArray<FName>& OutSocketsA,
	TArray<FName>& OutSocketsB,
	TArray<USplineComponent*>& OutSplines,
	TArray<USplineMeshComponent*>& OutSplineMeshes
)
{
	OutSocketsA.Empty();
	OutSocketsB.Empty();
	OutSplines.Empty();
	OutSplineMeshes.Empty();

	AActor* OwnerWires = GetOwner();
	if (!OwnerWires || !CompA || !CompB || !WireMesh)
	{
		return;
	}

	// Ensure poles are visible.
	CompA->SetVisibility(true, true);
	CompB->SetVisibility(true, true);

	// Determine per-pair available sockets.
	TArray<FName> AvailA;
	TArray<FName> AvailB;
	Wire_GetAvailableSocketsForPair(CompA, CompB, OtherForPair, AvailA, AvailB);

	const int32 NumWires = Wire_DetermineWireCount(AmountOfWires, AvailA.Num(), AvailB.Num());
	if (NumWires == 0)
	{
		return;
	}

	// Build each wire with unique sockets for this pair.
	for (int32 idx = 0; idx < NumWires; ++idx)
	{
		const FName SocketA = AvailA[idx];
		const FName SocketB = AvailB[idx];

		OutSocketsA.Add(SocketA);
		OutSocketsB.Add(SocketB);

		// Compute curve.
		const FVector Start = CompA->GetSocketLocation(SocketA);
		const FVector End   = CompB->GetSocketLocation(SocketB);
		const float Depth   = CurveDepths.IsValidIndex(idx) ? CurveDepths[idx] : 0.f;

		TArray<FVector> CurvePoints;
		Wire_CreateCurvePoints(Start, End, Depth, CurvePoints);

		// Spawn spline component.
		const FName SplineName = MakeUniqueObjectName(this, USplineComponent::StaticClass(), *FString::Printf(TEXT("WireSpline_%d"), idx));
		USplineComponent* SplineComp = NewObject<USplineComponent>(this, SplineName);
		SplineComp->SetupAttachment(OwnerWires->GetRootComponent());
		SplineComp->RegisterComponent();
		OwnerWires->AddInstanceComponent(SplineComp);

		// Populate points.
		SplineComp->ClearSplinePoints(false);
		for (const FVector& P : CurvePoints)
		{
			SplineComp->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
		}
		SplineComp->UpdateSpline();

		M_SplineComponents.Add(SplineComp);
		OutSplines.Add(SplineComp);

		// Spawn spline-mesh segments.
		const int32 NumPts = SplineComp->GetNumberOfSplinePoints();
		for (int32 s = 0; s < NumPts - 1; ++s)
		{
			const FVector WS  = SplineComp->GetLocationAtSplinePoint(s, ESplineCoordinateSpace::World);
			const FVector WE  = SplineComp->GetLocationAtSplinePoint(s + 1, ESplineCoordinateSpace::World);
			const FVector TS  = SplineComp->GetTangentAtSplinePoint(s, ESplineCoordinateSpace::World);
			const FVector TE  = SplineComp->GetTangentAtSplinePoint(s + 1, ESplineCoordinateSpace::World);

			const FTransform& XForm = SplineComp->GetComponentTransform();
			const FVector LS  = XForm.InverseTransformPosition(WS);
			const FVector LE  = XForm.InverseTransformPosition(WE);
			const FVector LTS = XForm.InverseTransformVector(TS);
			const FVector LTE = XForm.InverseTransformVector(TE);

			const FName MeshName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), *FString::Printf(TEXT("WireMesh_%d_%d"), idx, s));
			USplineMeshComponent* SM = NewObject<USplineMeshComponent>(this, MeshName);
			SM->SetStaticMesh(WireMesh);
			SM->SetMobility(EComponentMobility::Movable);
			SM->SetupAttachment(SplineComp);

			const FVector2D CrossScale(WireMeshScale.Y, WireMeshScale.Z);
			SM->SetStartScale(CrossScale);
			SM->SetEndScale(CrossScale);

			SM->RegisterComponent();
			OwnerWires->AddInstanceComponent(SM);
			SM->SetStartAndEnd(LS, LTS, LE, LTE, true);

			M_SplineMeshComponents.Add(SM);
			OutSplineMeshes.Add(SM);
		}
	}
}

void UDestructibleWire::Wire_Debug(const FString& Message) const
{
	RTSFunctionLibrary::PrintString(Message);
}

void UDestructibleWire::Wire_Error(const FString& Message) const
{
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("null");
	RTSFunctionLibrary::ReportError(Message + TEXT("  (Owner: ") + OwnerName + TEXT(")"));
}

void UDestructibleWire::Wire_GetEnSockets(UStaticMeshComponent* Comp, TArray<FName>& OutSocketNames)
{
	OutSocketNames.Empty();
	if (!Comp)
	{
		return;
	}

	for (const FName& Sock : Comp->GetAllSocketNames())
	{
		const FString S = Sock.ToString();
		if (S.Len() == 2 && S[0] == 'E' && FChar::IsDigit(S[1]))
		{
			OutSocketNames.Add(Sock);
		}
	}
	OutSocketNames.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
}

int32 UDestructibleWire::Wire_DetermineWireCount(int32 Requested, int32 CountA, int32 CountB)
{
	return FMath::Min(Requested, FMath::Min(CountA, CountB));
}

void UDestructibleWire::Wire_CreateCurvePoints(
	const FVector& Start,
	const FVector& End,
	float Depth,
	TArray<FVector>& OutPoints
)
{
	OutPoints.Empty();

	const int32 Segments = 20;
	FVector Control = (Start + End) * .5f;
	Control.Z -= Depth;

	for (int32 i = 0; i <= Segments; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(Segments);
		const float u = 1.0f - t;
		OutPoints.Add(u * u * Start + 2.0f * u * t * Control + t * t * End);
	}
}
