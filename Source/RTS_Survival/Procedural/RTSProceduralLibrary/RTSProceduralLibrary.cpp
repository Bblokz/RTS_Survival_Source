// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSProceduralLibrary.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGVolumeData.h"
#include "Math/Box.h"
#include "Math/BoxSphereBounds.h"
#include "UObject/Package.h" // For DuplicateObject

FPCGDataCollection URTSProceduralLibrary::GetPointDataNotOnVolumes(TArray<UPCGPointData*> InPointData,
                                                                   TArray<UPCGSpatialData*> InSpatialData,
                                                                   const FName& OutputPinName)
{
    TArray<UPCGPointData*> OutPointData;

    // Process each provided PointData object.
    for (UPCGPointData* PointData : InPointData)
    {
        if (!PointData)
        {
            continue;
        }

        // Duplicate the input so that we do not modify the original data.
        // The duplicate is created using the same Outer as the original.
        UPCGPointData* FilteredPointData = DuplicateObject<UPCGPointData>(PointData, PointData->GetOuter());
        if (!FilteredPointData)
        {
            continue;
        }

        // Prepare a new array for the points that pass the border test.
        TArray<FPCGPoint> ValidPoints;
        const TArray<FPCGPoint>& Points = PointData->GetPoints();

        // Iterate through all points in this point data.
        for (const FPCGPoint& Point : Points)
        {
            bool bRemovePoint = false;
            bool bFoundAssociatedSpatial = false;

            // Get the point's density bounds (the volume in which it "lives")
            FBoxSphereBounds DensityBounds = Point.GetDensityBounds();
            // Build an AABB from the density bounds' origin and extent.
            FBox PointBox(DensityBounds.Origin - DensityBounds.BoxExtent,
                          DensityBounds.Origin + DensityBounds.BoxExtent);

            // Create 2D versions (XY only) of the point's center and its box corners.
            FVector2D PointCenterXY(Point.Transform.GetLocation().X, Point.Transform.GetLocation().Y);
            FVector2D PointBoxMinXY(PointBox.Min.X, PointBox.Min.Y);
            FVector2D PointBoxMaxXY(PointBox.Max.X, PointBox.Max.Y);

            // Loop over every provided spatial data.
            for (UPCGSpatialData* SpatialData : InSpatialData)
            {
                if (!SpatialData)
                {
                    continue;
                }

                // Retrieve the spatial data's bounds and extract the XY plane.
                FBox SpatialBounds = SpatialData->GetBounds();
                FVector2D SpatialMinXY(SpatialBounds.Min.X, SpatialBounds.Min.Y);
                FVector2D SpatialMaxXY(SpatialBounds.Max.X, SpatialBounds.Max.Y);
                FBox2D SpatialXY(SpatialMinXY, SpatialMaxXY);

                // Check whether the point's center (XY only) is inside this spatial data's bounds.
                if (SpatialXY.IsInside(PointCenterXY))
                {
                    bFoundAssociatedSpatial = true;

                    // Instead of relying on IsInside (which is inclusive),
                    // perform a strict check: if any side of the point's box touches or exceeds the spatial region's boundary,
                    // mark the point for removal.
                    if (PointBoxMinXY.X <= SpatialXY.Min.X ||
                        PointBoxMinXY.Y <= SpatialXY.Min.Y ||
                        PointBoxMaxXY.X >= SpatialXY.Max.X ||
                        PointBoxMaxXY.Y >= SpatialXY.Max.Y)
                    {
                        bRemovePoint = true;
                        break;
                    }
                }
            }

            // If the point belonged to any spatial data and failed the strict containment test, skip it.
            if (bFoundAssociatedSpatial && bRemovePoint)
            {
                continue;
            }
            if(not bFoundAssociatedSpatial)
            {
                // If the point did not belong to any spatial data, skip it.
                continue;
            }

            ValidPoints.Add(Point);
        }

        // Replace the points of our duplicate with only those that passed the check.
        FilteredPointData->SetPoints(ValidPoints);
        OutPointData.Add(FilteredPointData);
    }

    // Now add the filtered point data into an FPCGDataCollection.
    FPCGDataCollection Collection;
    for (UPCGPointData* PD : OutPointData)
    {
        if (!PD)
        {
            continue;
        }

        // Create a tagged data entry for each point data.
        FPCGTaggedData TagData;
        TagData.Data = PD;
        TagData.Pin = OutputPinName;  // Use the provided output pin name.
        // (Tags can be left empty or filled as needed.)
        Collection.TaggedData.Add(TagData);

        // Compute and add a CRC for the data. Here we use full CRC mode (true).
        // The CRC acts as a fingerprint for the current state of the data,
        // which downstream nodes can use for change detection and caching.
        FPCGCrc DataCrc = PD->GetOrComputeCrc(true);
        Collection.DataCrcs.Add(DataCrc);
    }

    return Collection;
}

/**
 * Helper function to compute the axis‐aligned bounding box (AABB)
 * for an oriented box defined by a center, a rotation, and an overall size.
 */
static FBox ComputeOrientedAABB(const FVector& Center, const FRotator& Rotation, const FVector& VolumeSize)
{
    const FVector HalfExtents = VolumeSize * 0.5f;
    // Unreal's convention: Forward = X, Right = Y, Up = Z.
    const FVector XAxis = Rotation.RotateVector(FVector::ForwardVector);
    const FVector YAxis = Rotation.RotateVector(FVector::RightVector);
    const FVector ZAxis = Rotation.RotateVector(FVector::UpVector);

    TArray<FVector> Corners;
    Corners.Reserve(8);
    for (int32 i = 0; i < 8; i++)
    {
        const float SignX = (i & 1) ? 1.f : -1.f;
        const float SignY = (i & 2) ? 1.f : -1.f;
        const float SignZ = (i & 4) ? 1.f : -1.f;
        FVector Corner = Center 
            + (XAxis * (SignX * HalfExtents.X))
            + (YAxis * (SignY * HalfExtents.Y))
            + (ZAxis * (SignZ * HalfExtents.Z));
        Corners.Add(Corner);
    }
    FBox AABB(ForceInit);
    for (const FVector& Corner : Corners)
    {
        AABB += Corner;
    }
    return AABB;
}

/**
 * Generates volumes at each point from the provided point data.
 * For every FPCGPoint in each UPCGPointData, computes an oriented AABB using the provided VolumeBounds
 * and creates a new UPCGVolumeData instance. Each volume is then tagged with the given OutputPinName.
 */
FPCGDataCollection URTSProceduralLibrary::GenerateVolumesAtPoints(TArray<UPCGPointData*> InPointData,
                                                                  const FVector& VolumeBounds,
                                                                  const FName& OutputPinName)
{
    FPCGDataCollection Collection;

    // Process each provided point data object.
    for (UPCGPointData* PointData : InPointData)
    {
        if (!PointData)
        {
            continue;
        }

        const TArray<FPCGPoint>& Points = PointData->GetPoints();

        // For each point in the point data, create a volume.
        for (const FPCGPoint& Point : Points)
        {
            const FVector Center = Point.Transform.GetLocation();
            const FRotator Rotation = Point.Transform.GetRotation().Rotator();

            // Compute the oriented AABB for the volume.
            FBox VolumeAABB = ComputeOrientedAABB(Center, Rotation, VolumeBounds);

            // Create a new volume data object.
            // Using the same Outer as the PointData to ensure correct lifetime.
            UPCGVolumeData* NewVolumeData = NewObject<UPCGVolumeData>(PointData->GetOuter());
            if (!NewVolumeData)
            {
                continue;
            }
            NewVolumeData->Initialize(VolumeAABB);

            // Create a tagged data entry for the volume.
            FPCGTaggedData TagData;
            TagData.Data = NewVolumeData;
            TagData.Pin = OutputPinName;
            Collection.TaggedData.Add(TagData);

            // Compute and add the data CRC.
            FPCGCrc DataCrc = NewVolumeData->GetOrComputeCrc(true);
            Collection.DataCrcs.Add(DataCrc);
        }
    }

    return Collection;
}
