// Flash_Star: the side petals of gas burning at the muzzle, mesh SM_Muzzle_Part2 (a cross in the YZ plane, ~55 cm)
// with M_SmallArmsMuzzleFlash. Shorter and smaller than the cone; on about a third of the shots it stays dark.
// MuzzleShape: x size variation, y shows (above 0.33) or not, z roll, w brightness variation.

// @section Shape
// At least one and a half frames: on a slow frame the flash would otherwise die before it is ever drawn.
Particles.Lifetime = max(lerp(0.035, 0.06, R0), Engine.DeltaTime * 1.5);
Particles.MuzzleShape = float4(R0, R1, R2, R3);

// @section Place
// Smoothstep fade over the last 80 % of the life; dark on a third of the shots.
float FadeT = saturate((T - 0.2) / 0.8);
float Pop = (1.0 - FadeT * FadeT * (3.0 - 2.0 * FadeT)) * (Shape.y >= 0.33 ? 1.0 : 0.0);
float StarSize = S * lerp(0.28, 0.5, Shape.x) * lerp(0.8, 1.0, saturate(T * 5.0));
Particles.Position = OriginValue + Fwd * (3.0 * S);
Particles.Scale = float3(StarSize * 0.8, StarSize, StarSize);
float HalfRoll = Shape.z * 3.1415927;
float4 RollQuat = float4(sin(HalfRoll), 0.0, 0.0, cos(HalfRoll));
Particles.MeshOrientation = float4(
	ShotQuat.w * RollQuat.xyz + RollQuat.w * ShotQuat.xyz + cross(ShotQuat.xyz, RollQuat.xyz),
	ShotQuat.w * RollQuat.w - dot(ShotQuat.xyz, RollQuat.xyz));
Particles.Color = float4(float3(3.2, 1.0, 0.15) * lerp(0.7, 1.1, Shape.w), Pop);
