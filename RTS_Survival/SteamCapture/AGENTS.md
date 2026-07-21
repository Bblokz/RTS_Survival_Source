# Steam Capture Rendering Invariants

The Steam capture must be resolution-independent while still looking like the player's rendered view. Preserve the architecture and constraints below whenever changing files in this directory.

## Required rendering path

- Render through `UGameViewportClient::Draw` into `FRTSSteamCaptureViewport`, an offscreen `FDummyViewport` sized to the requested output resolution.
- Do not use the visible PIE viewport dimensions as the capture dimensions. The PIE window border and editor layout make its drawable area different from common output resolutions such as 1920x1080.
- Let `ULocalPlayer::CalcSceneView` build the capture view. This is what gives the capture the current player camera transform, projection, camera fades and color scaling, map `PostProcessVolume` blends, `PlayerCameraManager` blends, camera post-processing, show flags, and renderer CVars.
- Copy the player's display color gamut, display output format, and scene-HDR state to the offscreen viewport with `SetupHDR`.
- Render the requested dimensions directly. Never stretch or CPU-resize pixels read from the visible viewport.

The camera position, rotation, and FOV source must match the player. A different output aspect ratio may legitimately show more or less horizontal or vertical content according to Unreal's aspect-ratio constraint, but it must never geometrically stretch the image.

## Why post-processing matches

The dedicated viewport uses the normal game-view construction rather than a `USceneCaptureComponent2D`. Unreal therefore evaluates the map's post-process volumes and the player camera's post-process chain once, in the same order used for the visible player view.

Do not manually copy any of the following onto a capture component or capture view:

- `PostProcessSettings`
- `PostProcessBlendWeight`
- blended `PostProcessVolume` settings
- bloom, exposure, tonemapper, or color-grading overrides

Manually copying player post-processing while the capture also evaluates the map volume applies parts of the look twice. The observed symptom was incorrect coloring and especially bloom that appeared doubled.

## View-state and exposure sharing

The visible viewport and offscreen viewport cannot safely render with the same complete scene view state because their resolutions differ. Sharing the complete state contaminates temporal history, occlusion data, and resolution-dependent resources used by the visible player view.

`FRTSSteamCaptureViewExtension` must preserve this split:

- `FSceneView` is first constructed with the player's view state. During construction, Unreal links `EyeAdaptationViewState` to that player state.
- In `SetupView`, replace only public `FSceneView::State` with the capture-owned `FSceneViewStateReference`.
- Do not replace or independently recreate `EyeAdaptationViewState`. Leaving it linked to the player makes the capture consume the player's exposure without updating it a second time.
- Keep `bIsMultipleViewFamily = true` and `bIsFirstViewInMultipleViewFamily = false` so the second render does not advance shared per-scene frame state.
- Keep `bIsMainViewFamily = false` for the offscreen family so it does not drive scene captures intended to run only from the main view family.

This behavior depends on Unreal Engine 5.5 renderer internals. When upgrading the engine, re-audit:

- `FSceneView` construction of `EyeAdaptationViewState`
- `FViewInfo::ShouldUpdateEyeAdaptationBuffer`
- `ULocalPlayer::CalcSceneView`
- `UGameViewportClient::Draw`

## Frame timing and render-target lifecycle

- Draw the capture from `FCoreDelegates::OnEndFrame`, after the visible player viewport submitted its render. This makes the current player exposure available and avoids a nested viewport draw.
- A manual or maximum-duration stop must remain pending until the end-frame capture has completed. Otherwise the final due frame can be lost and the viewport can be destroyed while it is still needed.
- Bracket every offscreen draw with `EnqueueBeginRenderFrame(false)` and `EnqueueEndRenderFrame(false, false)`.
- Draw with an `FCanvas` whose render-target rectangle exactly matches `M_OutputResolution`.
- Flush the scene canvas and dummy viewport debug canvas before ending the offscreen render frame.
- Complete readback before releasing the viewport or its capture view state.

These steps follow Unreal's own `FDummyViewport` high-resolution screenshot lifecycle without using the global high-resolution screenshot state.

## Color encoding

The offscreen viewport contains final display-encoded color after Unreal's tonemapper. PNG compression must receive those bytes without another transfer-function conversion.

- Read pixels with `FReadSurfaceDataFlags::SetLinearToGamma(false)`.
- Pass the resulting `FColor` pixels directly to PNG compression.
- Do not call `ToFColorSRGB`, apply `FLinearColor` gamma conversion, or otherwise encode sRGB again.

The previous double-sRGB encoding made the capture differ from the player even when the post-process settings were otherwise correct.

## Approaches that must not be reintroduced

- Reading the visible PIE viewport and resizing it to the requested resolution.
- Using a `USceneCaptureComponent2D` as the primary Steam capture path.
- Copying player or volume post-process settings into a second capture component.
- Rendering the dedicated viewport before the visible player viewport.
- Sharing the player's complete temporal view state with the different-resolution capture.
- Adding an extra gamma or sRGB encode during readback or PNG writing.

## Verification checklist

After changing this rendering path:

1. Build `RTS_SurvivalEditor Win64 Development`.
2. Run PIE in a window whose drawable size and aspect ratio differ from the capture override.
3. Capture at multiple resolutions and aspect ratios; verify the PNG dimensions exactly match the request and geometry is not stretched.
4. Compare against the visible player view on a map with a `PostProcessVolume`, strong bloom, and recognizable color grading.
5. Test an exposure transition and verify the capture follows the player without causing the visible viewport exposure to pulse or reset.
6. Check dark and mid-tone colors for signs of a second sRGB conversion.
7. Start and manually stop a duration-unlimited recording and verify the final due frame and metadata are written.
