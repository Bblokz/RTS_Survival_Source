#pragma once

#include "CoreMinimal.h"
#include "DummyViewport.h"
#include "SceneTypes.h"
#include "SceneViewExtension.h"

class FSceneView;
class FSceneViewFamily;
class FViewportClient;

/**
 * @brief Offscreen game viewport whose render target is independent of the PIE window dimensions.
 * The Steam capture subsystem renders the normal game viewport client into this target.
 */
class FRTSSteamCaptureViewport final : public FDummyViewport
{
public:
	explicit FRTSSteamCaptureViewport(FViewportClient* ViewportClient);
	virtual ~FRTSSteamCaptureViewport() override;

	/**
	 * @brief Creates an offscreen surface that uses the player's display output configuration.
	 * @param Resolution Exact dimensions of the recording surface.
	 * @param PlayerViewport Viewport supplying HDR and display color configuration.
	 * @return True when the offscreen render target was created successfully.
	 */
	bool Initialize(const FIntPoint& Resolution, const FViewport& PlayerViewport);
	void Release();
	bool GetIsInitialized() const { return bM_IsInitialized; }

private:
	bool bM_IsInitialized = false;
};

/**
 * @brief Gives the offscreen viewport independent temporal history while retaining player eye adaptation.
 * It prevents target-resolution rendering from modifying the temporal state used by the visible PIE viewport.
 */
class FRTSSteamCaptureViewExtension final : public FSceneViewExtensionBase
{
public:
	FRTSSteamCaptureViewExtension(
		const FAutoRegister& AutoRegister,
		const FViewport* CaptureViewport,
		ERHIFeatureLevel::Type FeatureLevel);
	virtual ~FRTSSteamCaptureViewExtension() override;

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(
		FRDGBuilder& GraphBuilder,
		FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	const FViewport* M_CaptureViewport = nullptr;

	// The capture keeps its own target-sized temporal history; eye adaptation remains linked to the player state.
	FSceneViewStateReference M_CaptureViewState;
};
