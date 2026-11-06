// Update node of NS_AddOnArmorPanelDebris (written into the emitter by RTS.AddOnArmor.BuildDebrisSystem).
// Flight: gravity, air drag and spin. Ground: a plane at GroundHeight; pieces bounce, slide, then rest and turn flat.
// The last ShrinkSeconds of their life they shrink away from their spawn scale (PieceScaleIn).
// Branch free: both the flying and the resting result are computed and the particle's state picks one.
float DeltaTime = Engine.DeltaTime;
float3 Position = Particles.Position;
float3 Velocity = Particles.Velocity;
float4 Orientation = Particles.MeshOrientation;
float SpinSpeed = Particles.SpinSpeed;
float Resting = Particles.Resting;
float Flying = 1.0 - Resting;

// Flight.
float3 FlightVelocity = (Velocity + float3(0.0, 0.0, -980.0 * DeltaTime)) * saturate(1.0 - 0.35 * DeltaTime);
float3 FlightPosition = Position + FlightVelocity * DeltaTime;
float HalfAngle = 0.5 * SpinSpeed * DeltaTime;
float3 SpinVector = SpinAxisIn * sin(HalfAngle);
float SpinScalar = cos(HalfAngle);
float4 Spun = float4(SpinScalar * Orientation.xyz + Orientation.w * SpinVector + cross(SpinVector, Orientation.xyz),
	SpinScalar * Orientation.w - dot(SpinVector, Orientation.xyz));

// Resting: turn the plate's face normal (local Y) towards up or down, whichever is nearer, so it lies flat.
float3 LocalNormal = float3(0.0, 1.0, 0.0);
float3 Twice = 2.0 * cross(Orientation.xyz, LocalNormal);
float3 PlateNormal = LocalNormal + Orientation.w * Twice + cross(Orientation.xyz, Twice);
float UpSign = step(0.0, PlateNormal.z) * 2.0 - 1.0;
float3 TurnAxis = cross(PlateNormal, float3(0.0, 0.0, UpSign));
float TurnSine = length(TurnAxis);
float TurnStep = min(asin(min(TurnSine, 1.0)), 6.0 * DeltaTime);
float3 StepVector = TurnAxis / max(TurnSine, 0.0001) * sin(0.5 * TurnStep);
float StepScalar = cos(0.5 * TurnStep);
float4 Turned = float4(StepScalar * Orientation.xyz + Orientation.w * StepVector + cross(StepVector, Orientation.xyz),
	StepScalar * Orientation.w - dot(StepVector, Orientation.xyz));

// Ground contact of a flying piece: slow enough it lands and rests, else it bounces with friction.
float Floor = GroundHeight + 1.0;
float BelowFloor = step(FlightPosition.z, Floor);
float SlowEnough = step(abs(FlightVelocity.z), 120.0);
float Lands = Flying * BelowFloor * SlowEnough;
float Bounces = Flying * BelowFloor * (1.0 - SlowEnough);
float StillFlying = Flying * (1.0 - BelowFloor);
float3 BouncedVelocity = float3(FlightVelocity.x * 0.55, FlightVelocity.y * 0.55, -FlightVelocity.z * 0.3);
float3 FlownPosition = float3(FlightPosition.x, FlightPosition.y, max(FlightPosition.z, Floor));

float Remaining = LifetimeIn - AgeIn;
float ShrinkScale = saturate(Remaining / max(ShrinkSeconds, 0.001)) * PieceScaleIn;
Particles.Position = lerp(Position, FlownPosition, Flying);
Particles.Velocity = FlightVelocity * StillFlying + BouncedVelocity * Bounces;
Particles.MeshOrientation = normalize(lerp(Turned, Spun, Flying));
Particles.SpinSpeed = SpinSpeed * StillFlying + SpinSpeed * 0.5 * Bounces;
Particles.Resting = saturate(Resting + Lands);
Particles.Scale = float3(ShrinkScale, ShrinkScale, ShrinkScale);
