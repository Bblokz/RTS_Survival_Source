// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring of hull meshes for UAddOnArmorPanelsComponent.
// RTS.AddOnArmor.BuildPanelHull Source=<mesh> Target=<new mesh> [MinAbsY=114.9] [MinPanelArea=5000] [PanelsPerSide=0]
//   [MinZ=] [MaxZ=] [MaxIslandLength=] [MaterialOverrides=<slot>:<material>|<slot>:<material>] [-RebuildPanelMaster]
//   [-DryRun]
// Copies the mesh and moves its add-on panels into hideable material slots. Panel geometry = mesh islands lying
// entirely at |Y| >= MinAbsY, within MinZ..MaxZ and no longer than MaxIslandLength along X (longer ones are rails the
// panels hang on and stay hull). With PanelsPerSide 0, islands of at least MinPanelArea cm2 are panels (two that
// overlap along X by more than half are the faces of one double-sided plate and merge) and smaller ones (bolts,
// clamps, stiffeners) join the panel of their side whose X range holds them; with
// PanelsPerSide N (wire screens, cage armour), each side's panel geometry is split into N equal sections along X.
// Each panel's index goes into UV channel PanelIndexUvChannel and the panel layout is stored on the mesh. LODs with
// their own mesh description are tagged too, matched to the LOD0 panels by side and X.
// Panel geometry keeps one slot per source slot ("AddOnArmorPanels", "AddOnArmorPanels_1", ...). Its material is a copy
// of the source slot's material (or of MaterialOverrides, for Blueprints that override the hull's materials) named
// <material>_AddOnPanels next to the target mesh: an instance is reparented to <master>_AddOnArmorPanels, the master
// duplicated as a masked material whose opacity mask hides the panels set in custom primitive data
// HiddenPanelsPrimitiveDataIndex (made once per master); a plain material becomes such a masked copy itself.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorPanelAuthoringShared.h"
#include "AddOnArmorPanelLayout.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorPanelAuthoring, Log, All);

namespace AddOnArmorPanelAuthoring
{
	using namespace AddOnArmorPanelMask;

	const TCHAR* const PanelMasterSuffix = TEXT("_AddOnArmorPanels");
	const TCHAR* const PanelMaterialSuffix = TEXT("_AddOnPanels");
	constexpr float DefaultMinAbsY = 114.9f;
	constexpr float DefaultMinPanelArea = 5000.f;
	constexpr float UnboundedZ = 1.0e6f;
	// Visible panels write 1 and hidden ones 0 into the opacity mask.
	constexpr float OpacityMaskClipValue = 0.5f;

	const TCHAR* const PanelVisibilityHlsl = TEXT(
		"float PanelIndex = floor(PanelUv.x + 0.5);\n"
		"float Hidden = fmod(floor(HiddenPanels / exp2(PanelIndex)), 2.0);\n"
		"return 1.0 - Hidden;");

	struct FPanelHullSettings
	{
		FString SourcePath;
		FString TargetPath;
		// Debris panel to store on the layout; empty keeps the one of the hull being rebuilt.
		FString DebrisMeshPath;
		// Source material slot index -> material used instead of the mesh's own (Blueprint material overrides).
		TMap<int32, FString> MaterialOverrides;
		float MinAbsY = DefaultMinAbsY;
		float MinPanelArea = DefaultMinPanelArea;
		int32 PanelsPerSide = 0;
		float MinZ = -UnboundedZ;
		float MaxZ = UnboundedZ;
		float MaxIslandLength = UnboundedZ;
		bool bRebuildPanelMaster = false;
		bool bDryRun = false;
	};

	struct FSkirtPanel
	{
		TArray<int32> IslandIndices;
		FBox Bounds = FBox(ForceInit);
		EAddOnArmorPanelSide Side = EAddOnArmorPanelSide::Left;
	};

	// The panel slot made for one source slot: its name and material.
	struct FPanelSlot
	{
		FName SlotName;
		int32 SourceMaterialIndex = INDEX_NONE;
		TObjectPtr<UMaterialInterface> Material = nullptr;
	};

	void ParseMaterialOverrides(const FString& Text, TMap<int32, FString>& OutOverrides)
	{
		TArray<FString> Entries;
		Text.ParseIntoArray(Entries, TEXT("|"));
		for (const FString& Entry : Entries)
		{
			FString SlotText;
			FString MaterialPath;
			if (Entry.Split(TEXT(":"), &SlotText, &MaterialPath))
			{
				OutOverrides.Add(FCString::Atoi(*SlotText), MaterialPath);
			}
		}
	}

	bool ParseSettings(const TArray<FString>& Args, FPanelHullSettings& OutSettings)
	{
		const FString Joined = FString::Join(Args, TEXT(" "));
		FString OverridesText;
		FParse::Value(*Joined, TEXT("Source="), OutSettings.SourcePath);
		FParse::Value(*Joined, TEXT("Target="), OutSettings.TargetPath);
		FParse::Value(*Joined, TEXT("MinAbsY="), OutSettings.MinAbsY);
		FParse::Value(*Joined, TEXT("MinPanelArea="), OutSettings.MinPanelArea);
		FParse::Value(*Joined, TEXT("PanelsPerSide="), OutSettings.PanelsPerSide);
		FParse::Value(*Joined, TEXT("MinZ="), OutSettings.MinZ);
		FParse::Value(*Joined, TEXT("MaxZ="), OutSettings.MaxZ);
		FParse::Value(*Joined, TEXT("MaxIslandLength="), OutSettings.MaxIslandLength);
		FParse::Value(*Joined, TEXT("MaterialOverrides="), OverridesText);
		FParse::Value(*Joined, TEXT("DebrisMesh="), OutSettings.DebrisMeshPath);
		ParseMaterialOverrides(OverridesText, OutSettings.MaterialOverrides);
		OutSettings.bRebuildPanelMaster = FParse::Param(*Joined, TEXT("RebuildPanelMaster"));
		OutSettings.bDryRun = FParse::Param(*Joined, TEXT("DryRun"));
		return not OutSettings.SourcePath.IsEmpty() && not OutSettings.TargetPath.IsEmpty();
	}

	bool GetIsPanelGeometry(const FMeshIsland& Island, const FPanelHullSettings& Settings)
	{
		const bool bOutside = Island.Bounds.Min.Y >= Settings.MinAbsY || Island.Bounds.Max.Y <= -Settings.MinAbsY;
		return bOutside && Island.Bounds.Min.Z >= Settings.MinZ && Island.Bounds.Max.Z <= Settings.MaxZ
			&& Island.Bounds.GetSize().X <= Settings.MaxIslandLength;
	}

	EAddOnArmorPanelSide GetSide(const FBox& Bounds)
	{
		return Bounds.GetCenter().Y < 0.0 ? EAddOnArmorPanelSide::Left : EAddOnArmorPanelSide::Right;
	}

	/** @return The panel of the island's side whose X range holds the island's centre, else the nearest by X. */
	int32 FindPanelForIsland(const TArray<FSkirtPanel>& Panels, const FBox& IslandBounds)
	{
		const FVector Centre = IslandBounds.GetCenter();
		int32 BestPanel = INDEX_NONE;
		double BestDistance = TNumericLimits<double>::Max();
		for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
		{
			const FSkirtPanel& Panel = Panels[PanelIndex];
			if (Panel.Side != GetSide(IslandBounds))
			{
				continue;
			}
			const bool bInsideRange = Centre.X >= Panel.Bounds.Min.X && Centre.X <= Panel.Bounds.Max.X;
			const double Distance = bInsideRange ? 0.0 : FMath::Abs(Centre.X - Panel.Bounds.GetCenter().X);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestPanel = PanelIndex;
			}
		}
		return BestPanel;
	}

	void SortPanels(TArray<FSkirtPanel>& Panels)
	{
		Panels.Sort([](const FSkirtPanel& A, const FSkirtPanel& B)
		{
			return A.Side != B.Side ? A.Side < B.Side : A.Bounds.GetCenter().X < B.Bounds.GetCenter().X;
		});
	}

	/** @brief Two panels of one side overlapping along X by more than half are the two faces of one plate. */
	void MergeDoubleFacedPanels(TArray<FSkirtPanel>& SortedPanels)
	{
		constexpr double FaceOverlapRatio = 0.5;
		TArray<FSkirtPanel> Merged;
		for (FSkirtPanel& Panel : SortedPanels)
		{
			FSkirtPanel* const Previous = Merged.IsEmpty() ? nullptr : &Merged.Last();
			const double Overlap = Previous != nullptr
				? FMath::Min(Previous->Bounds.Max.X, Panel.Bounds.Max.X) - FMath::Max(Previous->Bounds.Min.X, Panel.Bounds.Min.X)
				: 0.0;
			const double ShorterLength = Previous != nullptr
				? FMath::Min(Previous->Bounds.GetSize().X, Panel.Bounds.GetSize().X)
				: 0.0;
			if (Previous != nullptr && Previous->Side == Panel.Side && Overlap > FaceOverlapRatio * ShorterLength)
			{
				Previous->IslandIndices.Append(Panel.IslandIndices);
				Previous->Bounds += Panel.Bounds;
				continue;
			}
			Merged.Add(MoveTemp(Panel));
		}
		SortedPanels = MoveTemp(Merged);
	}

	/** @brief Large islands are panels; smaller panel geometry joins the panel it sits on. */
	TArray<FSkirtPanel> ClassifyPanelsByArea(const TArray<FMeshIsland>& Islands, const FPanelHullSettings& Settings)
	{
		TArray<FSkirtPanel> Panels;
		for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
		{
			const FMeshIsland& Island = Islands[IslandIndex];
			if (GetIsPanelGeometry(Island, Settings) && Island.Area >= Settings.MinPanelArea)
			{
				FSkirtPanel& Panel = Panels.AddDefaulted_GetRef();
				Panel.IslandIndices.Add(IslandIndex);
				Panel.Bounds = Island.Bounds;
				Panel.Side = GetSide(Island.Bounds);
			}
		}
		SortPanels(Panels);
		MergeDoubleFacedPanels(Panels);
		for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
		{
			const FMeshIsland& Island = Islands[IslandIndex];
			if (not GetIsPanelGeometry(Island, Settings) || Island.Area >= Settings.MinPanelArea)
			{
				continue;
			}
			const int32 PanelIndex = FindPanelForIsland(Panels, Island.Bounds);
			if (PanelIndex != INDEX_NONE)
			{
				Panels[PanelIndex].IslandIndices.Add(IslandIndex);
			}
		}
		return Panels;
	}

	/** @brief Splits each side's panel geometry into PanelsPerSide equal sections along X (wire screens, cages). */
	TArray<FSkirtPanel> ClassifyPanelsBySections(const TArray<FMeshIsland>& Islands, const FPanelHullSettings& Settings)
	{
		TArray<FSkirtPanel> Panels;
		for (const EAddOnArmorPanelSide Side : {EAddOnArmorPanelSide::Left, EAddOnArmorPanelSide::Right})
		{
			TArray<int32> SideIslands;
			FBox SideBounds(ForceInit);
			for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
			{
				if (GetIsPanelGeometry(Islands[IslandIndex], Settings) && GetSide(Islands[IslandIndex].Bounds) == Side)
				{
					SideIslands.Add(IslandIndex);
					SideBounds += Islands[IslandIndex].Bounds;
				}
			}
			if (SideIslands.IsEmpty())
			{
				continue;
			}
			const double SectionLength = FMath::Max(SideBounds.GetSize().X / Settings.PanelsPerSide, UE_KINDA_SMALL_NUMBER);
			TArray<FSkirtPanel> SidePanels;
			SidePanels.SetNum(Settings.PanelsPerSide);
			for (const int32 IslandIndex : SideIslands)
			{
				const double Offset = Islands[IslandIndex].Bounds.GetCenter().X - SideBounds.Min.X;
				const int32 Section = FMath::Clamp(FMath::FloorToInt32(Offset / SectionLength), 0, Settings.PanelsPerSide - 1);
				SidePanels[Section].IslandIndices.Add(IslandIndex);
				SidePanels[Section].Bounds += Islands[IslandIndex].Bounds;
				SidePanels[Section].Side = Side;
			}
			for (FSkirtPanel& Panel : SidePanels)
			{
				if (not Panel.IslandIndices.IsEmpty())
				{
					Panels.Add(MoveTemp(Panel));
				}
			}
		}
		SortPanels(Panels);
		return Panels;
	}

	/** @return The static material index of a polygon group, by its slot name (the group index when unnamed). */
	int32 GetSourceMaterialIndex(const UStaticMesh& Mesh, const FMeshDescription& Description, const FPolygonGroupID Group)
	{
		const FStaticMeshConstAttributes Attributes(Description);
		const FName SlotName = Attributes.GetPolygonGroupMaterialSlotNames()[Group];
		const int32 MaterialIndex = Mesh.GetStaticMaterials().IndexOfByPredicate([SlotName](const FStaticMaterial& Material)
		{
			return Material.ImportedMaterialSlotName == SlotName;
		});
		return MaterialIndex != INDEX_NONE ? MaterialIndex : Group.GetValue();
	}

	FName MakePanelSlotName(const int32 PanelSlotIndex)
	{
		return PanelSlotIndex == 0
			? PanelMaterialSlotName
			: FName(*FString::Printf(TEXT("%s_%d"), *PanelMaterialSlotName.ToString(), PanelSlotIndex));
	}

	/** @return The panel slot for a source material slot, created on first use. */
	const FPanelSlot& FindOrAddPanelSlot(TArray<FPanelSlot>& PanelSlots, const int32 SourceMaterialIndex)
	{
		for (const FPanelSlot& PanelSlot : PanelSlots)
		{
			if (PanelSlot.SourceMaterialIndex == SourceMaterialIndex)
			{
				return PanelSlot;
			}
		}
		FPanelSlot& PanelSlot = PanelSlots.AddDefaulted_GetRef();
		PanelSlot.SlotName = MakePanelSlotName(PanelSlots.Num() - 1);
		PanelSlot.SourceMaterialIndex = SourceMaterialIndex;
		return PanelSlot;
	}

	/**
	 * @brief Moves panel polygons into their panel slots and writes the panel indices into the panel UV channel.
	 * @param Mesh Source mesh, for the slot names' material indices.
	 * @param IslandPanels Panel index per island of Description, INDEX_NONE for hull geometry.
	 * @param Islands Islands of Description.
	 * @param PanelSlots Panel slots shared by all LODs; new source slots add entries.
	 * @param Description LOD description to tag.
	 */
	void RetagPanels(const UStaticMesh& Mesh, const TArray<int32>& IslandPanels, const TArray<FMeshIsland>& Islands,
	                 TArray<FPanelSlot>& PanelSlots, FMeshDescription& Description)
	{
		FStaticMeshAttributes Attributes(Description);
		TVertexInstanceAttributesRef<FVector2f> Uvs = Attributes.GetVertexInstanceUVs();
		if (Uvs.GetNumChannels() <= PanelIndexUvChannel)
		{
			Uvs.SetNumChannels(PanelIndexUvChannel + 1);
		}
		TMap<FPolygonGroupID, FPolygonGroupID> PanelGroupBySourceGroup;
		for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
		{
			if (IslandPanels[IslandIndex] == INDEX_NONE)
			{
				continue;
			}
			for (const FPolygonID PolygonID : Islands[IslandIndex].Polygons)
			{
				const FPolygonGroupID SourceGroup = Description.GetPolygonPolygonGroup(PolygonID);
				FPolygonGroupID* PanelGroup = PanelGroupBySourceGroup.Find(SourceGroup);
				if (PanelGroup == nullptr)
				{
					const FPanelSlot& PanelSlot = FindOrAddPanelSlot(
						PanelSlots, GetSourceMaterialIndex(Mesh, Description, SourceGroup));
					const FPolygonGroupID NewGroup = Description.CreatePolygonGroup();
					Attributes.GetPolygonGroupMaterialSlotNames()[NewGroup] = PanelSlot.SlotName;
					PanelGroup = &PanelGroupBySourceGroup.Add(SourceGroup, NewGroup);
				}
				Description.SetPolygonPolygonGroup(PolygonID, *PanelGroup);
				for (const FVertexInstanceID VertexInstance : Description.GetPolygonVertexInstances(PolygonID))
				{
					Uvs.Set(VertexInstance, PanelIndexUvChannel, FVector2f(static_cast<float>(IslandPanels[IslandIndex]), 0.f));
				}
			}
		}
	}

	TArray<int32> MakeIslandPanelsForLod0(const TArray<FMeshIsland>& Islands, const TArray<FSkirtPanel>& Panels)
	{
		TArray<int32> IslandPanels;
		IslandPanels.Init(INDEX_NONE, Islands.Num());
		for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
		{
			for (const int32 IslandIndex : Panels[PanelIndex].IslandIndices)
			{
				IslandPanels[IslandIndex] = PanelIndex;
			}
		}
		return IslandPanels;
	}

	/** @return For a lower LOD: the LOD0 panel each panel-geometry island belongs to, by side and X. */
	TArray<int32> MakeIslandPanelsForLod(const TArray<FMeshIsland>& Islands, const TArray<FSkirtPanel>& Panels,
	                                     const FPanelHullSettings& Settings)
	{
		TArray<int32> IslandPanels;
		IslandPanels.Init(INDEX_NONE, Islands.Num());
		for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
		{
			if (GetIsPanelGeometry(Islands[IslandIndex], Settings))
			{
				IslandPanels[IslandIndex] = FindPanelForIsland(Panels, Islands[IslandIndex].Bounds);
			}
		}
		return IslandPanels;
	}

	template <typename TExpression>
	TExpression* AddExpression(UMaterial& Material, const int32 PositionX, const int32 PositionY)
	{
		// The editing library also sets the expression's material and GUID, which the material compiler relies on.
		return Cast<TExpression>(UMaterialEditingLibrary::CreateMaterialExpression(
			&Material, TExpression::StaticClass(), PositionX, PositionY));
	}

	/** @return False when the mask could not be wired; the material is left unsaved. */
	bool AddPanelMask(UMaterial& Material)
	{
		constexpr int32 NodeX = -900;
		constexpr int32 NodeY = 1400;
		constexpr int32 InputNodeOffsetX = 300;
		constexpr int32 InputNodeSpacingY = 120;
		UMaterialExpressionTextureCoordinate* const PanelUv = AddExpression<UMaterialExpressionTextureCoordinate>(
			Material, NodeX - InputNodeOffsetX, NodeY);
		PanelUv->CoordinateIndex = PanelIndexUvChannel;
		UMaterialExpressionScalarParameter* const HiddenPanels = AddExpression<UMaterialExpressionScalarParameter>(
			Material, NodeX - InputNodeOffsetX, NodeY + InputNodeSpacingY);
		HiddenPanels->ParameterName = HiddenPanelsParameterName;
		HiddenPanels->bUseCustomPrimitiveData = true;
		HiddenPanels->PrimitiveDataIndex = HiddenPanelsPrimitiveDataIndex;
		UMaterialExpressionCustom* const Visible = AddExpression<UMaterialExpressionCustom>(Material, NodeX, NodeY);
		Visible->Code = PanelVisibilityHlsl;
		Visible->OutputType = CMOT_Float1;
		Visible->Description = TEXT("AddOnArmorPanelVisible");
		Visible->Inputs.Reset();
		Visible->Inputs.AddDefaulted_GetRef().InputName = TEXT("PanelUv");
		Visible->Inputs.AddDefaulted_GetRef().InputName = TEXT("HiddenPanels");
		const bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(PanelUv, FString(), Visible, TEXT("PanelUv"))
			&& UMaterialEditingLibrary::ConnectMaterialExpressions(HiddenPanels, FString(), Visible, TEXT("HiddenPanels"))
			&& UMaterialEditingLibrary::ConnectMaterialProperty(Visible, FString(), MP_OpacityMask);
		if (not bConnected)
		{
			return false;
		}
		// Opaque masters keep a constant on the opacity mask pin; a set constant wins over the connected node.
		Material.GetEditorOnlyData()->OpacityMask.UseConstant = false;
		Material.BlendMode = BLEND_Masked;
		Material.OpacityMaskClipValue = OpacityMaskClipValue;
		// PostEditChange re-derives bCanMaskedBeAssumedOpaque: the copy keeps the opaque master's true, which makes the
		// renderer drop the opacity mask; a recompile alone does not update it.
		Material.PreEditChange(nullptr);
		Material.PostEditChange();
		return not Material.bCanMaskedBeAssumedOpaque;
	}

	/**
	 * @return The masked copy of a material at PackageName, made on first use (or again with bRebuild).
	 * @param Source Material to copy; a master shared by instances, or a hull's own plain material.
	 * @param PackageName Package of the masked copy.
	 * @param bRebuild Replace an existing copy (after changes to the source or to this panel graph).
	 */
	UMaterial* FindOrBuildMaskedCopy(UMaterial& Source, const FString& PackageName, const bool bRebuild)
	{
		if (FPackageName::DoesPackageExist(PackageName) && not bRebuild)
		{
			return LoadObject<UMaterial>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)));
		}
		UMaterial* const Material = DuplicateIntoFreshPackage(Source, PackageName);
		if (not IsValid(Material) || not AddPanelMask(*Material))
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Error, TEXT("RTS_ADDON_PANEL_AUTHORING masked copy %s FAIL"), *PackageName);
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Material);
		const bool bSaved = SaveAsset(*Material);
		UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display, TEXT("RTS_ADDON_PANEL_AUTHORING masked copy %s saved=%d"),
		       *PackageName, bSaved ? 1 : 0);
		return Material;
	}

	/**
	 * @brief Sets every global parameter the hull instance or one of its parent instances overrides on the panel
	 * instance; master defaults (and so custom-primitive-data parameters such as the team colour) are left alone.
	 */
	void CopyOverriddenParameters(const UMaterialInstanceConstant& HullMaterial, UMaterialInstanceConstant& PanelMaterial)
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		HullMaterial.GetAllScalarParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			float Value = 0.f;
			if (Info.Association == GlobalParameter && HullMaterial.GetScalarParameterValue(Info, Value, true))
			{
				UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(&PanelMaterial, Info.Name, Value);
			}
		}
		HullMaterial.GetAllVectorParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			FLinearColor Value;
			if (Info.Association == GlobalParameter && HullMaterial.GetVectorParameterValue(Info, Value, true))
			{
				UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(&PanelMaterial, Info.Name, Value);
			}
		}
		HullMaterial.GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UTexture* Value = nullptr;
			if (Info.Association == GlobalParameter && HullMaterial.GetTextureParameterValue(Info, Value, true))
			{
				UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(&PanelMaterial, Info.Name, Value);
			}
		}
		HullMaterial.GetAllStaticSwitchParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			bool bValue = false;
			FGuid ExpressionId;
			if (Info.Association == GlobalParameter && HullMaterial.GetStaticSwitchParameterValue(Info, bValue, ExpressionId, true))
			{
				UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(&PanelMaterial, Info.Name, bValue);
			}
		}
		UMaterialEditingLibrary::UpdateMaterialInstance(&PanelMaterial);
	}

	UMaterialInstanceConstant* BuildPanelMaterialInstance(UMaterialInstanceConstant& HullMaterial,
	                                                      const FString& PanelMaterialPath, const bool bRebuildMaster)
	{
		UMaterial* const HullMaster = HullMaterial.GetMaterial();
		UMaterial* const PanelMaster = IsValid(HullMaster)
			? FindOrBuildMaskedCopy(*HullMaster, HullMaster->GetOutermost()->GetName() + PanelMasterSuffix, bRebuildMaster)
			: nullptr;
		UMaterialInstanceConstant* const PanelMaterial = IsValid(PanelMaster)
			? DuplicateIntoFreshPackage(HullMaterial, PanelMaterialPath)
			: nullptr;
		if (not IsValid(PanelMaterial))
		{
			return nullptr;
		}
		UMaterialEditingLibrary::SetMaterialInstanceParent(PanelMaterial, PanelMaster);
		// Reparenting onto the master drops what intermediate parent instances overrode; copy the chain's overrides.
		CopyOverriddenParameters(HullMaterial, *PanelMaterial);
		FAssetRegistryModule::AssetCreated(PanelMaterial);
		return SaveAsset(*PanelMaterial) ? PanelMaterial : nullptr;
	}

	/** @return The masked panel material for one source slot material, next to the target mesh. */
	UMaterialInterface* BuildPanelMaterial(UMaterialInterface& SourceMaterial, const FPanelHullSettings& Settings)
	{
		const FString PanelMaterialPath = FPackageName::GetLongPackagePath(Settings.TargetPath) / SourceMaterial.GetName()
			+ PanelMaterialSuffix;
		if (UMaterialInstanceConstant* const Instance = Cast<UMaterialInstanceConstant>(&SourceMaterial))
		{
			return BuildPanelMaterialInstance(*Instance, PanelMaterialPath, Settings.bRebuildPanelMaster);
		}
		if (UMaterial* const Material = Cast<UMaterial>(&SourceMaterial))
		{
			return FindOrBuildMaskedCopy(*Material, PanelMaterialPath, true);
		}
		UE_LOG(LogRTSAddOnArmorPanelAuthoring, Error, TEXT("RTS_ADDON_PANEL_AUTHORING unsupported material %s"),
		       *SourceMaterial.GetPathName());
		return nullptr;
	}

	bool BuildPanelMaterials(const UStaticMesh& Source, const FPanelHullSettings& Settings, TArray<FPanelSlot>& PanelSlots)
	{
		for (FPanelSlot& PanelSlot : PanelSlots)
		{
			const FString* const OverridePath = Settings.MaterialOverrides.Find(PanelSlot.SourceMaterialIndex);
			UMaterialInterface* const SourceMaterial = OverridePath != nullptr
				? LoadObject<UMaterialInterface>(nullptr, **OverridePath)
				: Source.GetMaterial(PanelSlot.SourceMaterialIndex);
			PanelSlot.Material = IsValid(SourceMaterial) ? BuildPanelMaterial(*SourceMaterial, Settings) : nullptr;
			if (not IsValid(PanelSlot.Material))
			{
				return false;
			}
		}
		return true;
	}

	UAddOnArmorPanelLayout* MakeLayout(UStaticMesh& Mesh, const TArray<FSkirtPanel>& Panels)
	{
		TArray<FAddOnArmorPanel> LayoutPanels;
		for (const FSkirtPanel& Panel : Panels)
		{
			FAddOnArmorPanel& LayoutPanel = LayoutPanels.AddDefaulted_GetRef();
			LayoutPanel.MeshSpaceCentre = Panel.Bounds.GetCenter();
			LayoutPanel.Side = Panel.Side;
		}
		UAddOnArmorPanelLayout* const Layout = NewObject<UAddOnArmorPanelLayout>(&Mesh, NAME_None, RF_Transactional);
		Layout->SetPanels(LayoutPanels);
		return Layout;
	}

	/** @brief Sections follow polygon group order; each takes its slot's material and its source section's flags. */
	void SetSectionInfos(UStaticMesh& Target, const int32 LodIndex, const FMeshDescription& Description,
	                     const TArray<FPanelSlot>& PanelSlots)
	{
		const FStaticMeshConstAttributes Attributes(Description);
		int32 SectionIndex = 0;
		for (const FPolygonGroupID Group : Description.PolygonGroups().GetElementIDs())
		{
			const FName SlotName = Attributes.GetPolygonGroupMaterialSlotNames()[Group];
			const FPanelSlot* const PanelSlot = PanelSlots.FindByPredicate([SlotName](const FPanelSlot& Slot)
			{
				return Slot.SlotName == SlotName;
			});
			const int32 MaterialIndex = Target.GetMaterialIndexFromImportedMaterialSlotName(SlotName);
			const int32 FlagsMaterialIndex = PanelSlot != nullptr ? PanelSlot->SourceMaterialIndex : MaterialIndex;
			FMeshSectionInfo Info(MaterialIndex != INDEX_NONE ? MaterialIndex : Group.GetValue());
			for (int32 ExistingSection = 0; ExistingSection < Target.GetSectionInfoMap().GetSectionNumber(LodIndex); ++ExistingSection)
			{
				const FMeshSectionInfo Existing = Target.GetSectionInfoMap().Get(LodIndex, ExistingSection);
				if (Existing.MaterialIndex == FlagsMaterialIndex)
				{
					Info.bCastShadow = Existing.bCastShadow;
					Info.bEnableCollision = Existing.bEnableCollision;
					break;
				}
			}
			Target.GetSectionInfoMap().Set(LodIndex, SectionIndex++, Info);
		}
	}

	void LogPanels(const TArray<FMeshIsland>& Islands, const TArray<FSkirtPanel>& Panels)
	{
		for (int32 PanelIndex = 0; PanelIndex < Panels.Num(); ++PanelIndex)
		{
			const FSkirtPanel& Panel = Panels[PanelIndex];
			int32 PolygonCount = 0;
			for (const int32 IslandIndex : Panel.IslandIndices)
			{
				PolygonCount += Islands[IslandIndex].Polygons.Num();
			}
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display,
			       TEXT("RTS_ADDON_PANEL_AUTHORING panel=%d side=%s islands=%d polygons=%d min=%s max=%s"),
			       PanelIndex, Panel.Side == EAddOnArmorPanelSide::Left ? TEXT("Left") : TEXT("Right"),
			       Panel.IslandIndices.Num(), PolygonCount, *Panel.Bounds.Min.ToCompactString(),
			       *Panel.Bounds.Max.ToCompactString());
		}
	}

	/** @return The tagged descriptions per LOD; LODs without a description (reduction) stay unset. */
	TArray<TOptional<FMeshDescription>> TagLods(const UStaticMesh& Source, const TArray<FSkirtPanel>& Panels,
	                                            const TArray<FMeshIsland>& Lod0Islands, const FPanelHullSettings& Settings,
	                                            TArray<FPanelSlot>& PanelSlots)
	{
		TArray<TOptional<FMeshDescription>> Descriptions;
		Descriptions.SetNum(Source.GetNumSourceModels());
		for (int32 LodIndex = 0; LodIndex < Source.GetNumSourceModels(); ++LodIndex)
		{
			const FMeshDescription* const SourceDescription = Source.GetMeshDescription(LodIndex);
			if (SourceDescription == nullptr)
			{
				continue;
			}
			FMeshDescription Description = *SourceDescription;
			const TArray<FMeshIsland> Islands = LodIndex == 0 ? Lod0Islands : FindIslands(Description);
			const TArray<int32> IslandPanels = LodIndex == 0
				? MakeIslandPanelsForLod0(Islands, Panels)
				: MakeIslandPanelsForLod(Islands, Panels, Settings);
			RetagPanels(Source, IslandPanels, Islands, PanelSlots, Description);
			Descriptions[LodIndex] = MoveTemp(Description);
		}
		return Descriptions;
	}

	UStaticMesh* LoadMeshIfPresent(const FString& PackagePath)
	{
		if (PackagePath.IsEmpty() || not FPackageName::DoesPackageExist(PackagePath))
		{
			return nullptr;
		}
		return LoadObject<UStaticMesh>(nullptr, *(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath)));
	}

	/** @return The debris panel the rebuilt hull keeps: the given one, else the one the previous build had. */
	UStaticMesh* FindDebrisMesh(const FPanelHullSettings& Settings)
	{
		if (not Settings.DebrisMeshPath.IsEmpty())
		{
			return LoadMeshIfPresent(Settings.DebrisMeshPath);
		}
		UStaticMesh* const PreviousHull = LoadMeshIfPresent(Settings.TargetPath);
		const UAddOnArmorPanelLayout* const PreviousLayout = IsValid(PreviousHull)
			? PreviousHull->GetAssetUserData<UAddOnArmorPanelLayout>()
			: nullptr;
		return IsValid(PreviousLayout) ? PreviousLayout->GetDebrisMesh() : nullptr;
	}

	UStaticMesh* WritePanelHull(UStaticMesh& Source, const FPanelHullSettings& Settings, const TArray<FSkirtPanel>& Panels,
	                            TArray<TOptional<FMeshDescription>>& Descriptions, const TArray<FPanelSlot>& PanelSlots)
	{
		// Read before the fresh package moves the previous hull out.
		UStaticMesh* const DebrisMesh = FindDebrisMesh(Settings);
		UStaticMesh* const Target = DuplicateIntoFreshPackage(Source, Settings.TargetPath);
		if (not IsValid(Target))
		{
			return nullptr;
		}
		TArray<FStaticMaterial> Materials = Source.GetStaticMaterials();
		for (const FPanelSlot& PanelSlot : PanelSlots)
		{
			Materials.Add(FStaticMaterial(PanelSlot.Material, PanelSlot.SlotName, PanelSlot.SlotName));
		}
		Target->SetStaticMaterials(Materials);
		for (int32 LodIndex = 0; LodIndex < Descriptions.Num(); ++LodIndex)
		{
			if (not Descriptions[LodIndex].IsSet())
			{
				continue;
			}
			SetSectionInfos(*Target, LodIndex, Descriptions[LodIndex].GetValue(), PanelSlots);
			Target->CreateMeshDescription(LodIndex, MoveTemp(Descriptions[LodIndex].GetValue()));
			Target->CommitMeshDescription(LodIndex);
		}
		Target->RemoveUserDataOfClass(UAddOnArmorPanelLayout::StaticClass());
		UAddOnArmorPanelLayout* const Layout = MakeLayout(*Target, Panels);
		Layout->SetDebrisMesh(DebrisMesh);
		Target->AddAssetUserData(Layout);
		if (not IsValid(DebrisMesh))
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Warning, TEXT("RTS_ADDON_PANEL_AUTHORING WARNING no debris panel mesh on %s:")
			       TEXT(" shot-off panels will vanish without debris. Model one panel as its own static mesh and pass")
			       TEXT(" DebrisMesh=, or run RTS.AddOnArmor.SetPanelDebrisMesh / ExtractPanelDebris (AGENTS.md)."),
			       *Settings.TargetPath);
		}
		Target->Build(false);
		Target->PostEditChange();
		FAssetRegistryModule::AssetCreated(Target);
		return SaveAsset(*Target) ? Target : nullptr;
	}

	void BuildPanelHull(const TArray<FString>& Args)
	{
		FPanelHullSettings Settings;
		UStaticMesh* const Source = ParseSettings(Args, Settings) ? LoadObject<UStaticMesh>(nullptr, *Settings.SourcePath) : nullptr;
		const FMeshDescription* const SourceDescription = IsValid(Source) ? Source->GetMeshDescription(0) : nullptr;
		if (SourceDescription == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Error, TEXT("RTS_ADDON_PANEL_AUTHORING RESULT FAIL inputs (%s)"),
			       *FString::Join(Args, TEXT(" ")));
			return;
		}
		const TArray<FMeshIsland> Islands = FindIslands(*SourceDescription);
		const TArray<FSkirtPanel> Panels = Settings.PanelsPerSide > 0
			? ClassifyPanelsBySections(Islands, Settings)
			: ClassifyPanelsByArea(Islands, Settings);
		LogPanels(Islands, Panels);
		if (Panels.IsEmpty() || Panels.Num() > MaxPanelsPerMesh || Settings.bDryRun)
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display, TEXT("RTS_ADDON_PANEL_AUTHORING RESULT %s panels=%d (1 to %d) %s"),
			       Settings.bDryRun ? TEXT("DRYRUN") : TEXT("FAIL"), Panels.Num(), MaxPanelsPerMesh, *Settings.SourcePath);
			return;
		}
		TArray<FPanelSlot> PanelSlots;
		TArray<TOptional<FMeshDescription>> Descriptions = TagLods(*Source, Panels, Islands, Settings, PanelSlots);
		UStaticMesh* const Target = BuildPanelMaterials(*Source, Settings, PanelSlots)
			? WritePanelHull(*Source, Settings, Panels, Descriptions, PanelSlots)
			: nullptr;
		if (not IsValid(Target))
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Error, TEXT("RTS_ADDON_PANEL_AUTHORING RESULT FAIL material or mesh copy %s"),
			       *Settings.SourcePath);
			return;
		}
		for (int32 LodIndex = 0; LodIndex < Target->GetNumLODs(); ++LodIndex)
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display, TEXT("RTS_ADDON_PANEL_AUTHORING lod=%d tris=%d sections=%d"),
			       LodIndex, Target->GetNumTriangles(LodIndex), Target->GetNumSections(LodIndex));
		}
		for (const FPanelSlot& PanelSlot : PanelSlots)
		{
			UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display, TEXT("RTS_ADDON_PANEL_AUTHORING slot %s from %d material %s"),
			       *PanelSlot.SlotName.ToString(), PanelSlot.SourceMaterialIndex, *GetPathNameSafe(PanelSlot.Material));
		}
		UE_LOG(LogRTSAddOnArmorPanelAuthoring, Display,
		       TEXT("RTS_ADDON_PANEL_AUTHORING RESULT DONE panels=%d islands=%d slots=%d target=%s"),
		       Panels.Num(), Islands.Num(), Target->GetStaticMaterials().Num(), *Settings.TargetPath);
	}

	FAutoConsoleCommand BuildPanelHullCommand(
		TEXT("RTS.AddOnArmor.BuildPanelHull"),
		TEXT("Copies a hull mesh with its add-on armour panels in hideable material slots. Args: Source= Target= "
			"[MinAbsY=] [MinPanelArea=] [PanelsPerSide=] [MinZ=] [MaxZ=] [MaxIslandLength=] [MaterialOverrides=slot:material|...] "
			"[DebrisMesh=<panel mesh; default keeps the previous build's>] "
			"[-RebuildPanelMaster] [-DryRun]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildPanelHull));
}

#endif // WITH_EDITOR
