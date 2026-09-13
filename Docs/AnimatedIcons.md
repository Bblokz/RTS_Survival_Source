# Animated vertical icons

## Content setup

The catalog is a **Data Asset > AnimatedIconDataAsset**. The starter asset is
`/Game/RTS_Survival/Blueprints/GameUI/PooledUI/AnimatedIcons/DA_AnimatedIcons`.
Its constructor adds `CommanderBoost`, `RangeBoost`, and `Healing` entries.
Assign a texture to each entry; `None` deliberately displays nothing and does not
need an entry. Display size, tint, and default animation belong to each entry.
Pool size and widget class also live in this asset.

Project Settings > Game > Animated Icons contains only **Icon Data Asset**.
`DefaultGame.ini` therefore needs just:

```ini
[/Script/RTS_Survival.AnimatedIconSettings]
IconDataAsset=/Game/RTS_Survival/Blueprints/GameUI/PooledUI/AnimatedIcons/DA_AnimatedIcons.DA_AnimatedIcons
```

`Tools/CreateAnimatedIconAssets.py` creates this catalog, assigns the config
reference, and creates `PAL_AnimatedIcons` with an Always Cook rule for the
catalog. It preserves existing catalog entries when rerun. The label makes the
catalog a cook root; its serialized texture and widget references are cook
dependencies. Keep the default PrimaryAssetLabel scan of `/Game` enabled, or
include this catalog through your project's equivalent Asset Manager cook rules.

Run the setup script through the Unreal Python console or the PythonScript
commandlet after building the Editor module. It requires PythonScriptPlugin and
EditorScriptingUtilities. Runtime playback has no Python or editor dependencies.

## PNG and alpha setup

Import RGBA PNGs with actual transparent backgrounds. Use `UserInterface2D
(RGBA)` compression, Texture Group `UI`, sRGB enabled for colored art, and
Compress Without Alpha disabled. Start with NoMipmaps for these small, stable
screen-space icons. Use clamp addressing, consistent transparent padding, and
inspect the alpha channel in the Texture Editor. Test edges against both light
and dark backgrounds; avoid artwork exported against a black or white matte.

The Image brush displays the texture directly. PNG alpha and image tint alpha
multiply the outer widget's render opacity. Leave tint alpha at one for ordinary
icons. No custom material, opacity mask material, or dynamic material instance
is needed. A Masked material's cutout behavior does not provide smooth fading.

## Pooled widget

The default `W_RTSVerticalAnimatedIcon` native class builds:

```text
M_IconSizeBox (SizeBox)
  IconScaleBox (ScaleBox, Scale To Fit)
    M_IconImage (Image)
```

An optional Widget Blueprint subclass can be assigned to the catalog's Widget
Class. A custom Designer hierarchy must contain a SizeBox named
`M_IconSizeBox` and an Image named `M_IconImage`. Put the Image in a centered
ScaleBox using Scale To Fit to preserve aspect ratio. An empty subclass uses
the native hierarchy. No Blueprint initialization calls are required.

Do not add a background, property bindings, Blueprint Tick, or a separate
fade/movement animation. The manager applies all changes. The widget is not
hit-testable while active and collapsed while dormant. Every activation replaces
the brush, tint, size, opacity, and render transform before showing its component.
Display size uses Slate units, not source texture pixels.

## Calling from Blueprint and C++

Blueprint: **Get World Subsystem (AnimatedIconWorldSubsystem)** ->
**Get Animated Icon Widget Pool Manager** -> one of:

- `ShowAnimatedIcon(IconType, WorldLocation)`
- `ShowAnimatedIconWithSettings(IconType, WorldLocation, AnimationSettings)`
- `ShowAnimatedIconAttachedToActor(IconType, AttachActor, LocalOffset)`
- `ShowAnimatedIconAttachedToActorWithSettings(IconType, AttachActor, LocalOffset, AnimationSettings)`

Calls return whether activation succeeded. `None`, a missing catalog/texture,
invalid attachment, or invalid animation returns false. Catalog defaults apply
unless a WithSettings function is used. Set a nonnegative visible/fade duration
and a positive combined lifetime; zero fade is an instant disappearance, and
zero visible time starts fading immediately. Negative DeltaZ moves downward.

C++: include `AnimatedIconWidgetPoolManager.h` and obtain the manager through
`FRTS_Statics::GetVerticalAnimatedIconWidgetPoolManager(WorldContextObject)`.
External classes should cache it as a `UPROPERTY() TWeakObjectPtr` with a member
validator, consistent with other project services.

The manager object is available after subsystem initialization. Pool prewarming
runs in `OnWorldBeginPlay`, before actors receive BeginPlay. Calls before that
return false. No configuration leaves the feature disabled. Dedicated servers
do not load the catalog or allocate the pool.

## Animation and ownership

One bounded pool serves all icon types. When full, the oldest active slot is
restarted; no widget or component is created on a Show call. Text and resource
text pools remain independent. Slate widget trees are also built during prewarming
and retained while dormant. Automatic component ticking sleeps hidden widgets.
Catalog and texture/class loading handles retain
asset residency throughout the pool lifetime; show calls perform no loading.
Catalog changes during PIE require restarting PIE to rebuild the cache.

Components render in **screen space** but move along **world Z**, exactly like
animated vertical text. Travel is linear over VisibleDuration + FadeOutDuration.
Opacity stays one during the visible phase, then fades linearly. The subsystem
only animates active slots, using world game time, so gameplay pause/time dilation
apply. Screen-space icons are not occluded by terrain/buildings. No distance
scaling, fog-of-war filtering, replication, or split-screen routing is added;
gameplay callers decide when a local player should receive an icon.

Attached offsets are local to the target root. Attachment inherits target
movement, rotation, and scale, with incremental world-Z animation matching text.
Destroyed actors or invalid roots release their icons. Component ownership
always remains on the pool actor. Reset detaches and hides the component; the
next activation establishes a fresh attachment/transform. Shutdown destroys
the pool owner and releases asset-loading handles.

## Verification

Run automation tests under `RTS.UI.AnimatedIcons` in the Session Frontend.
They cover lifetime boundaries, visible/fade movement, zero-duration cases,
negative travel, native widget initialization, fixed-capacity reuse, failed
attachment allocation, actor destruction, and shutdown. Also visually check
camera zoom, aspect ratios, alpha edges, fade midpoint, and rapid alternation
between differently sized/tinted icons. Validate catalog inclusion in a packaged
build after assigning the final artwork.
