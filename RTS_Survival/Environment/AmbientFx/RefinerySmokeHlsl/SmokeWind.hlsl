// Particle update code of the refinery smoke (NS_RadixiteSmoke1), written into its SmokeWind HLSL node by the console
// command RTS.AmbientFx.PatchRefinerySmoke. {WindX} / {WindY} are replaced with the ambient wind direction
// (URTSAmbientFxSettings::WindYawDegrees), so the smoke leans the same way the dust devils drift.
// Runs after SolveForcesAndVelocity: it steers Particles.Velocity, which the solver moves the particle by next frame.
// CPU VM: no uint maths, smoothstep or step.

const float WindSpeed = 230.0;          // cm/s the smoke drifts at once it has caught the wind.
const float WindResponse = 1.2;         // How fast a puff takes on the wind speed (1/s).
const float RiseDrag = 0.4;             // How fast the upward push dies away (1/s): the plume bends over.
const float GustStrength = 0.35;        // Share of the wind speed the shared gusts add or take away.
const float SwayShare = 0.3;            // Sideways swing of the whole plume, as a share of the wind speed.
const float WobbleShare = 0.3;         // Each puff's own sideways wobble, as a share of the wind speed.
const float SpinDegreesPerSecond = 14.0;

float DeltaSeconds = Engine.DeltaTime;
// Lifetimes are random per puff, which makes them a stable per-puff seed (0 to 1).
float Seed = frac(Particles.Lifetime * 97.13);
float2 WindDirection = float2({WindX}, {WindY});
float2 CrossDirection = float2(-WindDirection.y, WindDirection.x);

// Shared, slow rhythms swing the whole plume; the age term gives each puff its own curl.
float Gust = 1.0 + GustStrength * sin(Engine.Time * 0.53) * sin(Engine.Time * 0.21 + 1.3);
float Sway = SwayShare * sin(Engine.Time * 0.37 + 0.8)
	+ WobbleShare * sin(Particles.Age * 1.7 + Seed * 6.2832);
float2 TargetHorizontal = (WindDirection * Gust + CrossDirection * Sway) * WindSpeed * (0.8 + 0.4 * Seed);

float3 NewVelocity = Particles.Velocity;
NewVelocity.xy = lerp(NewVelocity.xy, TargetHorizontal, 1.0 - exp(-WindResponse * DeltaSeconds));
NewVelocity.z = NewVelocity.z * exp(-RiseDrag * DeltaSeconds);
Particles.Velocity = NewVelocity;
Particles.SpriteRotation = Particles.SpriteRotation + (Seed - 0.5) * 2.0 * SpinDegreesPerSecond * DeltaSeconds;
