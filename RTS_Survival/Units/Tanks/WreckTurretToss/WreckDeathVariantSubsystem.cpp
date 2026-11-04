#include "WreckDeathVariantSubsystem.h"

#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogWreckDeathVariant, Log, All);

namespace WreckDeathVariantConstants
{
	// Burst slots in use at once; a Debris burst holds one, a HeavyDebris burst more. A full budget (a big battle)
	// falls back to a lighter variant.
	constexpr int32 MaxDebrisBurstSlots = 4;
	const FName HullBaseColorParam = TEXT("HullBaseColor");
	// NS_WreckDebrisBurst scales every piece by this user parameter; shrinking it to zero sinks them away.
	const FName DebrisScaleParam = TEXT("DebrisScale");
	// The sinking pieces shrink in steps this often; fine enough at RTS camera distance.
	constexpr float SinkTickSeconds = 0.1f;
	// Texture names that mark a colour map, in order of preference.
	const TCHAR* const ColourTextureHints[] = {
		TEXT("Diffuse"), TEXT("BaseColor"), TEXT("Albedo"), TEXT("Color"), TEXT("Colour"), TEXT("Camo"), TEXT("_D"),
		TEXT("_BC")
	};
	// Colour-space sRGB textures that are still not colour maps.
	const TCHAR* const NonColourTextureHints[] = {
		TEXT("Normal"), TEXT("_N"), TEXT("Mask"), TEXT("Rough"), TEXT("RAOM"), TEXT("ORM"), TEXT("Noise"), TEXT("Grunge"),
		TEXT("Dirt")
	};

	bool GetHasHint(const FString& TextureName, const TCHAR* const* Hints, const int32 HintCount)
	{
		for (int32 HintIndex = 0; HintIndex < HintCount; ++HintIndex)
		{
			if (TextureName.Contains(Hints[HintIndex]))
			{
				return true;
			}
		}
		return false;
	}

	/** @return The hull's colour map: an sRGB texture, preferring one whose name says so; null when none. */
	UTexture* FindColourTexture(const UMaterialInterface& HullMaterial)
	{
		TArray<UTexture*> UsedTextures;
		HullMaterial.GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
		UTexture* FirstColourSpaceTexture = nullptr;
		for (UTexture* Texture : UsedTextures)
		{
			if (not IsValid(Texture) || not Texture->SRGB || Texture->CompressionSettings == TC_Normalmap)
			{
				continue;
			}
			const FString TextureName = Texture->GetName();
			if (GetHasHint(TextureName, NonColourTextureHints, UE_ARRAY_COUNT(NonColourTextureHints)))
			{
				continue;
			}
			if (GetHasHint(TextureName, ColourTextureHints, UE_ARRAY_COUNT(ColourTextureHints)))
			{
				return Texture;
			}
			if (FirstColourSpaceTexture == nullptr)
			{
				FirstColourSpaceTexture = Texture;
			}
		}
		return FirstColourSpaceTexture;
	}
}

ERegularDeathVariant UWreckDeathVariantSubsystem::DrawVariant()
{
	if (M_VariantBag.IsEmpty())
	{
		RefillVariantBag();
	}
	M_LastVariant = M_VariantBag.Pop(EAllowShrinking::No);
	bM_HasDrawnVariant = true;
	return M_LastVariant;
}

void UWreckDeathVariantSubsystem::RefillVariantBag()
{
	M_VariantBag = {
		ERegularDeathVariant::Standard, ERegularDeathVariant::ChainReaction, ERegularDeathVariant::Debris,
		ERegularDeathVariant::HeavyDebris, ERegularDeathVariant::WheelThrow
	};
	for (int32 Index = M_VariantBag.Num() - 1; Index > 0; --Index)
	{
		M_VariantBag.Swap(Index, FMath::RandRange(0, Index));
	}
	// Draws pop from the back; never start a new bag with the variant that ended the last one.
	if (bM_HasDrawnVariant && M_VariantBag.Last() == M_LastVariant)
	{
		M_VariantBag.Swap(0, M_VariantBag.Num() - 1);
	}
}

bool UWreckDeathVariantSubsystem::GetCanStartDebrisBurst(const int32 BurstSlots)
{
	PruneFinishedDebrisBursts();
	return GetUsedDebrisBurstSlots() + BurstSlots <= WreckDeathVariantConstants::MaxDebrisBurstSlots;
}

int32 UWreckDeathVariantSubsystem::GetUsedDebrisBurstSlots() const
{
	int32 UsedSlots = 0;
	for (const FWreckDebrisBurstSinkState& Burst : M_ActiveDebrisBursts)
	{
		UsedSlots += Burst.BurstSlots;
	}
	return UsedSlots;
}

void UWreckDeathVariantSubsystem::RegisterDebrisBurst(UNiagaraComponent* DebrisComponent, const float LieSeconds,
                                                      const float SinkSeconds, const int32 BurstSlots)
{
	UWorld* World = GetWorld();
	if (not IsValid(DebrisComponent) || not IsValid(World))
	{
		return;
	}
	FWreckDebrisBurstSinkState& Burst = M_ActiveDebrisBursts.AddDefaulted_GetRef();
	Burst.Component = DebrisComponent;
	Burst.SinkStartSeconds = World->GetTimeSeconds() + LieSeconds;
	Burst.SinkSeconds = FMath::Max(SinkSeconds, WreckDeathVariantConstants::SinkTickSeconds);
	Burst.BurstSlots = FMath::Max(1, BurstSlots);
	FTimerManager& TimerManager = World->GetTimerManager();
	if (not TimerManager.IsTimerActive(M_DebrisSinkTimerHandle))
	{
		TimerManager.SetTimer(M_DebrisSinkTimerHandle, FTimerDelegate::CreateUObject(
			                      this, &UWreckDeathVariantSubsystem::TickDebrisSinking),
		                      WreckDeathVariantConstants::SinkTickSeconds, true);
	}
}

void UWreckDeathVariantSubsystem::TickDebrisSinking()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const float NowSeconds = World->GetTimeSeconds();
	for (FWreckDebrisBurstSinkState& Burst : M_ActiveDebrisBursts)
	{
		UNiagaraComponent* Component = Burst.Component.Get();
		if (not IsValid(Component) || NowSeconds < Burst.SinkStartSeconds)
		{
			continue;
		}
		const float RemainingScale = 1.f - (NowSeconds - Burst.SinkStartSeconds) / Burst.SinkSeconds;
		if (RemainingScale <= 0.f)
		{
			// Gone into the ground: end the burst so its slot frees.
			UE_LOG(LogWreckDeathVariant, Log, TEXT("RTS_WRD debris_sunk t=%.1f"), NowSeconds);
			Component->DeactivateImmediate();
			continue;
		}
		Component->SetVariableVec3(WreckDeathVariantConstants::DebrisScaleParam, FVector(RemainingScale));
	}
	PruneFinishedDebrisBursts();
	if (M_ActiveDebrisBursts.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(M_DebrisSinkTimerHandle);
	}
}

void UWreckDeathVariantSubsystem::PruneFinishedDebrisBursts()
{
	M_ActiveDebrisBursts.RemoveAll([](const FWreckDebrisBurstSinkState& Burst)
	{
		return not Burst.Component.IsValid() || not Burst.Component->IsActive();
	});
}

UMaterialInterface* UWreckDeathVariantSubsystem::GetDebrisMaterialForHull(UMaterialInterface* HullMaterial,
                                                                          UMaterialInterface* ChunkMaterial)
{
	if (not IsValid(ChunkMaterial) || not IsValid(HullMaterial))
	{
		return ChunkMaterial;
	}
	// Keyed on the texture, not the hull material: a wreck with its own dynamic material must not add an entry each death.
	UTexture* ColourTexture = WreckDeathVariantConstants::FindColourTexture(*HullMaterial);
	if (not IsValid(ColourTexture))
	{
		return ChunkMaterial;
	}
	if (const TObjectPtr<UMaterialInstanceDynamic>* CachedMaterial = M_DebrisMaterialByTexture.Find(ColourTexture))
	{
		if (IsValid(*CachedMaterial))
		{
			return CachedMaterial->Get();
		}
	}
	UE_LOG(LogWreckDeathVariant, Log, TEXT("RTS_WRD debris_material hull=%s colour_texture=%s"), *HullMaterial->GetName(),
	       *ColourTexture->GetName());
	UMaterialInstanceDynamic* TintedChunk = UMaterialInstanceDynamic::Create(ChunkMaterial, this);
	TintedChunk->SetTextureParameterValue(WreckDeathVariantConstants::HullBaseColorParam, ColourTexture);
	M_DebrisMaterialByTexture.Add(ColourTexture, TintedChunk);
	return TintedChunk;
}
