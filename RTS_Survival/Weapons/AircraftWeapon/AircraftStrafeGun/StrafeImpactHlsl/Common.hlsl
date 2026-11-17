// Custom HLSL of the aircraft strafe impact emitters (NS_AircraftStrafeImpacts), split into sections that
// RTS.StrafeImpactVfx.BuildNiagaraSystem joins per emitter; the code is written into the emitters' HLSL nodes.
// Spawn node:  SpawnBegin + <Layer> Shape + SpawnEnd + ReadImpact + <Layer> Place
// Update node: UpdateBegin + ReadImpact + <Layer> Place
// Inputs: StrafePositionAndCalibre (xyz location, w calibre in mm) / StrafeNormalAndSurface (xyz ground normal,
// w surface: 0 dirt, 1 sand, 2 stone, 3 metal), float4 arrays with the impacts of this frame of both groups behind
// each other; TypeOffset / TypeCount (where this emitter's group starts and how many impacts it has); ExecCount
// (particles spawned this frame); StrafeOriginIn / StrafeAxisIn / StrafeShapeIn (the particle attributes as read
// before the node; the update must read them this way).
// The CPU VM has no smoothstep or step: layers write them out as saturate(...) and comparisons.
// S is the calibre scale: 1 for a 7.92 mm MG, about 1.7 for a 20 mm cannon.

// @section SpawnBegin
float R0 = rand(1.0);
float R1 = rand(1.0);
float R2 = rand(1.0);
float R3 = rand(1.0);
// Every impact of this emitter's group gets the same share of the particles spawned this frame.
int ImpactIndex = TypeOffset + (ExecIndex() * TypeCount) / max(ExecCount, 1);
// Left without a start value on purpose: the VM compiler refuses a constant as an out argument.
float4 ImpactPC;
float4 ImpactNS;
StrafePositionAndCalibre.Get(ImpactIndex, ImpactPC);
StrafeNormalAndSurface.Get(ImpactIndex, ImpactNS);
Particles.StrafeOrigin = ImpactPC.xyz;
float CalibreScale = pow(max(ImpactPC.w, 4.0) / 7.92, 0.6);
Particles.StrafeAxis = float4(normalize(ImpactNS.xyz + float3(0.0, 0.0, 0.0001)), CalibreScale);
float SurfaceKind = ImpactNS.w;

// @section SpawnEnd
// No impact for this group (should not happen, the spawn count follows the impacts): the particle dies at once.
Particles.Lifetime = TypeCount > 0 ? Particles.Lifetime : 0.0;
float3 OriginValue = Particles.StrafeOrigin;
float4 AxisValue = Particles.StrafeAxis;
float4 ShapeValue = Particles.StrafeShape;
float AgeSeconds = 0.0;
float T = 0.0;

// @section UpdateBegin
float3 OriginValue = StrafeOriginIn;
float4 AxisValue = StrafeAxisIn;
float4 ShapeValue = StrafeShapeIn;
float AgeSeconds = Particles.Age;
float T = saturate(Particles.NormalizedAge);

// @section ReadImpact
float3 GroundUp = AxisValue.xyz;
float S = AxisValue.w;
float4 Shape = ShapeValue;
// Surface tint from the shape's w (written at spawn): dirt, sand, stone; metal reads as stone dust.
float Kind = Shape.w;
float3 DirtColour = Kind < 0.5 ? float3(0.33, 0.26, 0.19) : (Kind < 1.5 ? float3(0.62, 0.53, 0.38) : float3(0.46, 0.45, 0.43));
