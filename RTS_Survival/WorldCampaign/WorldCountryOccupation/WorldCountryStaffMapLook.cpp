// Copyright (C) Bas Blokzijl - All rights reserved.

// The staff-map look of UWorldCountryOccupationRegulator: hatched occupied ground, a liberation front that spreads
// from every cleared target, a border that tells the country's state and a highlight for the country under the cursor.
// Kept apart from WorldCountryOccupationRegulator.cpp, which maps anchors to countries and derives liberation.

#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryEdgeDistance.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace WorldCountryStaffMapLook
{
	constexpr int32 FillMaterialIndex = 0;
	// The fill material takes this many front points; further cleared targets of one country are not drawn.
	constexpr int32 MaxFrontPoints = 8;
	// A cleared target frees ground up to this share of the distance to the nearest enemy target still standing.
	constexpr float FrontReachShareOfEnemyDistance = 0.8f;
	constexpr float MinFrontRadius = 140.f;
	constexpr float MaxFrontRadius = 700.f;
	constexpr float FrontGrowSeconds = 2.5f;
	constexpr float FrontGrowTickSeconds = 0.03f;
	constexpr float BorderBrightness = 1.6f;
	constexpr float HoveredBorderBrightness = 3.2f;
	const TCHAR* const OutlineMeshNameMarker = TEXT("Outline");

	const FName ParameterOccupiedColor(TEXT("OccupiedColor"));
	const FName ParameterHatchColor(TEXT("HatchColor"));
	const FName ParameterFreeColor(TEXT("FreeColor"));
	const FName ParameterFrontEdgeColor(TEXT("FrontEdgeColor"));
	const FName ParameterFullyFree(TEXT("FullyFree"));
	const FName ParameterHover(TEXT("Hover"));
	const FName ParameterBorderColor(TEXT("BorderColor"));
	const FName ParameterSecondColor(TEXT("SecondColor"));
	const FName ParameterDashed(TEXT("Dashed"));
	const FName ParameterBrightness(TEXT("Brightness"));
	const FName ParameterBorderBrightness(TEXT("BorderBrightness"));
	const FName ParameterBorderWidth(TEXT("BorderWidth"));
	const FName ParameterEdgeDistance(TEXT("EdgeDistance"));
	const FName ParameterEdgeBoundsMin(TEXT("EdgeBoundsMin"));
	const FName ParameterEdgeBoundsSize(TEXT("EdgeBoundsSize"));
	// Longer side of a country's edge distance picture; Russia is about 8 map units per pixel at this size.
	constexpr int32 EdgeDistanceMaxPixelsPerSide = 1024;
	// How far beyond the outermost anchors a country is still measured.
	constexpr float CampaignMapRectMargin = 900.f;

	// Staff-map palette: dark red ground with red hatching for the enemy, steel blue for free ground.
	const FLinearColor OccupiedGround = FLinearColor(FColor::FromHex(TEXT("2B1412")));
	const FLinearColor OccupiedHatch = FLinearColor(FColor::FromHex(TEXT("7E2218")));
	const FLinearColor FreeGround = FLinearColor(FColor::FromHex(TEXT("2C5268")));
	const FLinearColor NeutralGround = FLinearColor(FColor::FromHex(TEXT("3B4038")));
	const FLinearColor FrontEdge = FLinearColor(FColor::FromHex(TEXT("E6DFC8")));
	const FLinearColor EnemyBorder = FLinearColor(FColor::FromHex(TEXT("D0281C")));
	const FLinearColor FreeBorder = FLinearColor(FColor::FromHex(TEXT("6FB6D6")));
	const FLinearColor ContestedBorder = FLinearColor(FColor::FromHex(TEXT("E6DFC8")));
	const FLinearColor NeutralBorder = FLinearColor(FColor::FromHex(TEXT("5A6058")));

	FName GetFrontPointParameterName(const int32 PointIndex)
	{
		return FName(*FString::Printf(TEXT("FrontPoint%d"), PointIndex));
	}

	/** "SM_Germany_Outline" and "SM_MiddleCountriesOutline" both give the name of the fill mesh they frame. */
	FString GetFramedMeshName(const FString& OutlineMeshName)
	{
		FString FramedName = OutlineMeshName;
		FramedName.ReplaceInline(OutlineMeshNameMarker, TEXT(""));
		while (FramedName.EndsWith(TEXT("_")))
		{
			FramedName.LeftChopInline(1);
		}
		return FramedName;
	}
}

void UWorldCountryOccupationRegulator::LoadStaffMapLookMaterials()
{
	if (not bM_UseStaffMapLook)
	{
		return;
	}

	M_LoadedStaffMapFillMaterial = M_StaffMapFillMaterial.LoadSynchronous();
	M_LoadedBorderStateMaterial = M_BorderStateMaterial.LoadSynchronous();
}

FName UWorldCountryOccupationRegulator::GetCountryIdAtMapLocation(const FVector2D MapLocation) const
{
	const int32 LiberationDataIndex = FindLiberationDataIndexAtMapLocation(MapLocation);
	return M_CountryLiberationData.IsValidIndex(LiberationDataIndex)
		       ? M_CountryLiberationData[LiberationDataIndex].CountryId
		       : NAME_None;
}

void UWorldCountryOccupationRegulator::SetHoveredCountryAtMapLocation(const FVector2D MapLocation,
                                                                     const bool bIsCursorOverMap)
{
	const int32 NewHoveredIndex = bIsCursorOverMap ? FindLiberationDataIndexAtMapLocation(MapLocation) : INDEX_NONE;
	if (NewHoveredIndex == M_HoveredLiberationDataIndex)
	{
		return;
	}

	ApplyHoverToCountry(M_HoveredLiberationDataIndex, false);
	M_HoveredLiberationDataIndex = NewHoveredIndex;
	ApplyHoverToCountry(M_HoveredLiberationDataIndex, true);
}

int32 UWorldCountryOccupationRegulator::FindLiberationDataIndexAtMapLocation(const FVector2D& MapLocation) const
{
	for (int32 LiberationDataIndex = 0; LiberationDataIndex < M_CountryLiberationData.Num(); LiberationDataIndex++)
	{
		const int32 CountryIndex = M_CountryLiberationData[LiberationDataIndex].CountryIndex;
		if (not M_CountryTrianglesByIndex.IsValidIndex(CountryIndex))
		{
			continue;
		}

		if (WorldCountryLiberationRules::GetIsPointInCountryTriangles(MapLocation, M_CountryTrianglesByIndex[CountryIndex]))
		{
			return LiberationDataIndex;
		}
	}
	return INDEX_NONE;
}

void UWorldCountryOccupationRegulator::ApplyHoverToCountry(const int32 LiberationDataIndex, const bool bIsHovered)
{
	if (not M_CountryLiberationData.IsValidIndex(LiberationDataIndex))
	{
		return;
	}

	using namespace WorldCountryStaffMapLook;
	const FWorldCountryLiberationData& LiberationData = M_CountryLiberationData[LiberationDataIndex];
	if (IsValid(LiberationData.StaffMapMaterial))
	{
		LiberationData.StaffMapMaterial->SetScalarParameterValue(ParameterHover, bIsHovered ? 1.f : 0.f);
		LiberationData.StaffMapMaterial->SetScalarParameterValue(
			ParameterBorderBrightness, bIsHovered ? HoveredBorderBrightness : BorderBrightness);
	}

	if (IsValid(LiberationData.BorderMaterial))
	{
		LiberationData.BorderMaterial->SetScalarParameterValue(
			ParameterBrightness, bIsHovered ? HoveredBorderBrightness : BorderBrightness);
	}
}

bool UWorldCountryOccupationRegulator::TryApplyStaffMapLook(FWorldCountryLiberationData& LiberationData)
{
	using namespace WorldCountryStaffMapLook;
	UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(LiberationData.CountryIndex);
	if (not bM_UseStaffMapLook || not IsValid(M_LoadedStaffMapFillMaterial) || not IsValid(CountryMesh))
	{
		return false;
	}

	if (not IsValid(LiberationData.StaffMapMaterial))
	{
		LiberationData.StaffMapMaterial = UMaterialInstanceDynamic::Create(M_LoadedStaffMapFillMaterial, this);
	}

	UMaterialInstanceDynamic* FillMaterial = LiberationData.StaffMapMaterial;
	if (not IsValid(FillMaterial))
	{
		return false;
	}

	if (CountryMesh->GetMaterial(FillMaterialIndex) != FillMaterial)
	{
		CountryMesh->SetMaterial(FillMaterialIndex, FillMaterial);
	}

	const bool bHasNoTargets = LiberationData.TargetTally.GetTotalTargets() == 0;
	const bool bIsFullyFree = bHasNoTargets || LiberationData.Liberation >= 1.f;
	FillMaterial->SetVectorParameterValue(ParameterOccupiedColor, OccupiedGround);
	FillMaterial->SetVectorParameterValue(ParameterHatchColor, OccupiedHatch);
	FillMaterial->SetVectorParameterValue(ParameterFreeColor, bHasNoTargets ? NeutralGround : FreeGround);
	FillMaterial->SetVectorParameterValue(ParameterFrontEdgeColor, FrontEdge);
	FillMaterial->SetScalarParameterValue(ParameterFullyFree, bIsFullyFree ? 1.f : 0.f);
	ApplyCountryFrontPoints(LiberationData);
	ApplyCountryEdgeDistance(LiberationData);
	ApplyCountryBorderState(LiberationData);
	return true;
}

FBox2D UWorldCountryOccupationRegulator::GetCampaignMapRect() const
{
	FBox2D MapRect(ForceInit);
	for (TActorIterator<AAnchorPoint> AnchorIterator(GetWorld()); AnchorIterator; ++AnchorIterator)
	{
		MapRect += FVector2D(AnchorIterator->GetActorLocation());
	}
	return MapRect.bIsValid ? MapRect.ExpandBy(WorldCountryStaffMapLook::CampaignMapRectMargin) : MapRect;
}

void UWorldCountryOccupationRegulator::ApplyCountryEdgeDistance(FWorldCountryLiberationData& LiberationData)
{
	using namespace WorldCountryStaffMapLook;
	UMaterialInstanceDynamic* FillMaterial = LiberationData.StaffMapMaterial;
	if (not IsValid(FillMaterial) || LiberationData.bHasBuiltEdgeDistanceTexture)
	{
		return;
	}

	LiberationData.bHasBuiltEdgeDistanceTexture = true;
	FillMaterial->SetScalarParameterValue(ParameterBorderWidth, 0.f);
	if (not M_CountryTrianglesByIndex.IsValidIndex(LiberationData.CountryIndex) || M_BorderBandWidth <= 0.f)
	{
		return;
	}

	const FWorldCountryEdgeDistanceField Field = WorldCountryEdgeDistance::BuildEdgeDistanceField(
		M_CountryTrianglesByIndex[LiberationData.CountryIndex], GetCampaignMapRect(), EdgeDistanceMaxPixelsPerSide);
	if (not Field.GetIsValid())
	{
		return;
	}

	UTexture2D* EdgeDistanceTexture = UTexture2D::CreateTransient(Field.Width, Field.Height, PF_G8);
	if (not IsValid(EdgeDistanceTexture) || EdgeDistanceTexture->GetPlatformData() == nullptr)
	{
		return;
	}

	EdgeDistanceTexture->SRGB = false;
	EdgeDistanceTexture->Filter = TF_Bilinear;
	EdgeDistanceTexture->AddressX = TA_Clamp;
	EdgeDistanceTexture->AddressY = TA_Clamp;
	FTexture2DMipMap& Mip = EdgeDistanceTexture->GetPlatformData()->Mips[0];
	void* MipData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(MipData, Field.Pixels.GetData(), Field.Pixels.Num());
	Mip.BulkData.Unlock();
	EdgeDistanceTexture->UpdateResource();

	LiberationData.EdgeDistanceTexture = EdgeDistanceTexture;
	const FVector2D BoundsSize = Field.Bounds.GetSize();
	FillMaterial->SetTextureParameterValue(ParameterEdgeDistance, EdgeDistanceTexture);
	FillMaterial->SetVectorParameterValue(ParameterEdgeBoundsMin,
	                                      FLinearColor(Field.Bounds.Min.X, Field.Bounds.Min.Y, 0.f, 0.f));
	FillMaterial->SetVectorParameterValue(ParameterEdgeBoundsSize, FLinearColor(BoundsSize.X, BoundsSize.Y, 0.f, 0.f));
	FillMaterial->SetScalarParameterValue(ParameterBorderWidth, M_BorderBandWidth);
}

void UWorldCountryOccupationRegulator::ApplyCountryFrontPoints(FWorldCountryLiberationData& LiberationData)
{
	using namespace WorldCountryStaffMapLook;
	TArray<FWorldCountryFrontPoint> FrontPoints;
	BuildCountryFrontPoints(LiberationData.CountryIndex, FrontPoints);

	// The first look of a country shows its saved state at once; only targets cleared after that grow outward.
	const bool bIsFirstLook = not LiberationData.bHasShownFrontOnce;
	LiberationData.bHasShownFrontOnce = true;
	const UWorld* World = GetWorld();
	const double NowSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	for (int32 PointIndex = 0; PointIndex < MaxFrontPoints; PointIndex++)
	{
		FLinearColor PointValue(0.f, 0.f, 0.f, 0.f);
		if (FrontPoints.IsValidIndex(PointIndex))
		{
			const FWorldCountryFrontPoint& FrontPoint = FrontPoints[PointIndex];
			if (not LiberationData.FrontGrowStartSecondsByAnchorKey.Contains(FrontPoint.AnchorKey))
			{
				// A start far in the past means "fully grown".
				LiberationData.FrontGrowStartSecondsByAnchorKey.Add(
					FrontPoint.AnchorKey, bIsFirstLook ? NowSeconds - FrontGrowSeconds : NowSeconds);
			}

			const double GrowStartSeconds = LiberationData.FrontGrowStartSecondsByAnchorKey[FrontPoint.AnchorKey];
			const float GrowAlpha = FMath::Clamp(static_cast<float>(NowSeconds - GrowStartSeconds) / FrontGrowSeconds,
			                                     0.f, 1.f);
			const float GrownRadius = FrontPoint.Radius * FMath::InterpEaseOut(0.f, 1.f, GrowAlpha, 2.f);
			PointValue = FLinearColor(FrontPoint.MapLocation.X, FrontPoint.MapLocation.Y, GrownRadius, 0.f);
			LiberationData.bIsFrontGrowing |= GrowAlpha < 1.f;
		}
		LiberationData.StaffMapMaterial->SetVectorParameterValue(GetFrontPointParameterName(PointIndex), PointValue);
	}

	if (LiberationData.bIsFrontGrowing)
	{
		StartFrontGrowthTimer();
	}
}

void UWorldCountryOccupationRegulator::BuildCountryFrontPoints(const int32 CountryIndex,
                                                               TArray<FWorldCountryFrontPoint>& OutFrontPoints) const
{
	using namespace WorldCountryStaffMapLook;
	OutFrontPoints.Reset();
	const TArray<TWeakObjectPtr<AAnchorPoint>>* CountryAnchors = FindCountryAnchorsByIndex(CountryIndex);
	if (CountryAnchors == nullptr || not bM_IsCampaignProgressBound || not M_WorldCampaignProgressManager.IsValid())
	{
		return;
	}

	TArray<FVector2D> StandingEnemyLocations;
	for (const TWeakObjectPtr<AAnchorPoint>& AnchorPoint : *CountryAnchors)
	{
		const AAnchorPoint* Anchor = AnchorPoint.Get();
		if (not IsValid(Anchor))
		{
			continue;
		}

		const FWorldCountryAnchorTargetInput AnchorInput = BuildAnchorTargetInput(
			Anchor, M_WorldCampaignProgressManager.Get());
		if (not AnchorInput.bIsTarget)
		{
			continue;
		}

		const FVector2D MapLocation(Anchor->GetActorLocation());
		if (not AnchorInput.bIsCleared)
		{
			StandingEnemyLocations.Add(MapLocation);
			continue;
		}

		FWorldCountryFrontPoint FrontPoint;
		FrontPoint.AnchorKey = Anchor->GetAnchorKey();
		FrontPoint.MapLocation = MapLocation;
		OutFrontPoints.Add(FrontPoint);
	}

	for (FWorldCountryFrontPoint& FrontPoint : OutFrontPoints)
	{
		float NearestEnemyDistance = MaxFrontRadius / FrontReachShareOfEnemyDistance;
		for (const FVector2D& EnemyLocation : StandingEnemyLocations)
		{
			NearestEnemyDistance = FMath::Min(NearestEnemyDistance,
			                                  static_cast<float>(FVector2D::Distance(EnemyLocation, FrontPoint.MapLocation)));
		}
		FrontPoint.Radius = FMath::Clamp(NearestEnemyDistance * FrontReachShareOfEnemyDistance, MinFrontRadius,
		                                 MaxFrontRadius);
	}

	// A stable order, so the same cleared targets always land in the same material slots.
	OutFrontPoints.Sort([](const FWorldCountryFrontPoint& Left, const FWorldCountryFrontPoint& Right)
	{
		return Left.AnchorKey.ToString() < Right.AnchorKey.ToString();
	});
	if (OutFrontPoints.Num() > MaxFrontPoints)
	{
		OutFrontPoints.SetNum(MaxFrontPoints);
	}
}

void UWorldCountryOccupationRegulator::StartFrontGrowthTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_FrontGrowthTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(M_FrontGrowthTimerHandle, this,
	                                  &UWorldCountryOccupationRegulator::TickFrontGrowth,
	                                  WorldCountryStaffMapLook::FrontGrowTickSeconds, true);
}

void UWorldCountryOccupationRegulator::TickFrontGrowth()
{
	bool bIsAnyFrontGrowing = false;
	for (FWorldCountryLiberationData& LiberationData : M_CountryLiberationData)
	{
		if (not LiberationData.bIsFrontGrowing || not IsValid(LiberationData.StaffMapMaterial))
		{
			continue;
		}

		LiberationData.bIsFrontGrowing = false;
		ApplyCountryFrontPoints(LiberationData);
		bIsAnyFrontGrowing |= LiberationData.bIsFrontGrowing;
	}

	UWorld* World = GetWorld();
	if (not bIsAnyFrontGrowing && IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_FrontGrowthTimerHandle);
	}
}

void UWorldCountryOccupationRegulator::ApplyCountryBorderState(FWorldCountryLiberationData& LiberationData)
{
	using namespace WorldCountryStaffMapLook;
	const bool bHasNoTargets = LiberationData.TargetTally.GetTotalTargets() == 0;
	const bool bIsContested = not bHasNoTargets && LiberationData.Liberation > 0.f && LiberationData.Liberation < 1.f;
	FLinearColor BorderColor = EnemyBorder;
	if (bHasNoTargets)
	{
		BorderColor = NeutralBorder;
	}
	else if (LiberationData.Liberation >= 1.f)
	{
		BorderColor = FreeBorder;
	}
	else if (bIsContested)
	{
		BorderColor = ContestedBorder;
	}

	const bool bIsHovered = M_CountryLiberationData.IsValidIndex(M_HoveredLiberationDataIndex)
		&& M_CountryLiberationData[M_HoveredLiberationDataIndex].CountryId == LiberationData.CountryId;
	const float Brightness = bIsHovered ? HoveredBorderBrightness : BorderBrightness;
	if (IsValid(LiberationData.StaffMapMaterial))
	{
		// The band inside the country carries the state; the thin outline mesh below repeats it.
		LiberationData.StaffMapMaterial->SetVectorParameterValue(ParameterBorderColor, BorderColor);
		LiberationData.StaffMapMaterial->SetVectorParameterValue(ParameterSecondColor, EnemyBorder);
		LiberationData.StaffMapMaterial->SetScalarParameterValue(ParameterDashed, bIsContested ? 1.f : 0.f);
		LiberationData.StaffMapMaterial->SetScalarParameterValue(ParameterBorderBrightness, Brightness);
	}

	if (not IsValid(M_LoadedBorderStateMaterial))
	{
		return;
	}

	if (not LiberationData.bHasSearchedBorderMeshActor)
	{
		LiberationData.bHasSearchedBorderMeshActor = true;
		LiberationData.BorderMeshActor = FindBorderMeshActorForCountry(LiberationData.CountryIndex);
	}

	AStaticMeshActor* BorderMeshActor = LiberationData.BorderMeshActor.Get();
	UStaticMeshComponent* BorderMesh = IsValid(BorderMeshActor) ? BorderMeshActor->GetStaticMeshComponent() : nullptr;
	if (not IsValid(BorderMesh))
	{
		return;
	}

	if (not IsValid(LiberationData.BorderMaterial))
	{
		LiberationData.BorderMaterial = UMaterialInstanceDynamic::Create(M_LoadedBorderStateMaterial, this);
		for (int32 MaterialIndex = 0; MaterialIndex < BorderMesh->GetNumMaterials(); MaterialIndex++)
		{
			BorderMesh->SetMaterial(MaterialIndex, LiberationData.BorderMaterial);
		}
		BorderMeshActor->SetActorHiddenInGame(false);
	}

	UMaterialInstanceDynamic* BorderMaterial = LiberationData.BorderMaterial;
	BorderMaterial->SetVectorParameterValue(ParameterBorderColor, BorderColor);
	BorderMaterial->SetVectorParameterValue(ParameterSecondColor, EnemyBorder);
	BorderMaterial->SetScalarParameterValue(ParameterDashed, bIsContested ? 1.f : 0.f);
	BorderMaterial->SetScalarParameterValue(ParameterBrightness, Brightness);
}

AStaticMeshActor* UWorldCountryOccupationRegulator::FindBorderMeshActorForCountry(const int32 CountryIndex) const
{
	using namespace WorldCountryStaffMapLook;
	const UStaticMeshComponent* CountryMesh = GetCountryMeshForIndex(CountryIndex);
	const UStaticMesh* CountryStaticMesh = IsValid(CountryMesh) ? CountryMesh->GetStaticMesh() : nullptr;
	if (not IsValid(CountryStaticMesh))
	{
		return nullptr;
	}

	// The level names outlines after the fill mesh they frame ("SM_Germany" -> "SM_Germany_Outline"); merged fills
	// such as "SM_Russia_united" are framed by the outline of their base name, so the longest matching name wins.
	const FString CountryMeshName = CountryStaticMesh->GetName();
	AStaticMeshActor* BestBorderActor = nullptr;
	int32 BestMatchLength = 0;
	for (TActorIterator<AStaticMeshActor> ActorIterator(GetWorld()); ActorIterator; ++ActorIterator)
	{
		const UStaticMeshComponent* CandidateMesh = ActorIterator->GetStaticMeshComponent();
		const UStaticMesh* CandidateStaticMesh = IsValid(CandidateMesh) ? CandidateMesh->GetStaticMesh() : nullptr;
		if (not IsValid(CandidateStaticMesh) || not CandidateStaticMesh->GetName().Contains(OutlineMeshNameMarker))
		{
			continue;
		}

		const FString FramedMeshName = GetFramedMeshName(CandidateStaticMesh->GetName());
		if (CountryMeshName.StartsWith(FramedMeshName) && FramedMeshName.Len() > BestMatchLength)
		{
			BestBorderActor = *ActorIterator;
			BestMatchLength = FramedMeshName.Len();
		}
	}
	return BestBorderActor;
}
