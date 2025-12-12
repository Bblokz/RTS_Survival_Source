#pragma once


#include "CoreMinimal.h"

#include "GridOverlaySettings.generated.h"

USTRUCT(Blueprintable, BlueprintType)
struct FGridOverlaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	float CellSize = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	int32 MaxSize = 32;

	/** Extra spacing between visual tile instances (world units). 0 = no gap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid", meta=(ClampMin="0.0", UIMin="0.0"))
	float InstanceGap = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	UStaticMesh* CellMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	FLinearColor VacantColor = FLinearColor::Gray;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	FLinearColor ValidColor = FLinearColor::Green;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Grid")
	FLinearColor InvalidColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Material")
	UMaterialInterface* GridOverlayMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Material")
	FName MatParam_VacantColor = TEXT("VacantColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Material")
	FName MatParam_ValidColor = TEXT("ValidColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Material")
	FName MatParam_InvalidColor = TEXT("InvalidColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Material")
	FName MatParam_Opacity = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"), Category= "Material")
	float MaterialOpacity = 0.35f;

	// ---------------- Falloff controls ----------------

	/** Toggle falloff (1 = on, 0 = off). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|Falloff")
	bool bUseFalloff = true;

	/** How many tile "rings" to fade across before reaching the floor (e.g. 2–6). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", UIMin="0.1"), Category="Material|Falloff")
	float FalloffMaxSteps = 3.0f;

	/** Shape of the curve (1=linear, >1 = faster fade, <1 = slower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", UIMin="0.1"), Category="Material|Falloff")
	float FalloffExponent = 1.5f;

	/** Minimum opacity for non-footprint tiles so they never fully vanish. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"), Category="Material|Falloff")
	float FalloffMinOpacity = 0.0f;

	/** Material parameter names for the falloff controls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|Falloff")
	FName MatParam_UseFalloff = TEXT("UseFalloff"); // scalar (0/1)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|Falloff")
	FName MatParam_FalloffMaxSteps = TEXT("FalloffMaxSteps");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|Falloff")
	FName MatParam_FalloffExponent = TEXT("FalloffExponent");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|Falloff")
	FName MatParam_FalloffMinOpacity = TEXT("FalloffMinOpacity");

	// Defines how much of the square is considered inner square; that part gets multiplied with the inner square
	// opacity multiplier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"), Category="Material|InnerSquareOpacity")
	float InnerSquareRatio= 0.8f;

	// Opacity multiplier for the inner square area (0 = fully transparent, 1 = no change).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", ClampMax="1.0"), Category="Material|InnerSquareOpacity")
	float InnerSquareOpacityMlt = 0.5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|InnerSquareOpacity")
	FName MatParam_InnerSquareRatio = TEXT("InnerSquareRatio");
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Material|InnerSquareOpacity")
	FName MatParam_InnerSquareOpacityMlt = TEXT("InnerSquareOpacityMlt");
	
};
