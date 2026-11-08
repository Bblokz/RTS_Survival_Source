// Spawn node of NS_AddOnArmorPanelDebris (written into the emitter by RTS.AddOnArmor.BuildDebrisSystem).
// One particle per shot-off panel: it starts exactly where the panel hung, with its push and spin.
// Inputs: PieceStart (xyz position, w uniform scale), PieceVelocity (xyz), PieceSpin (xyz axis, w rad/s),
// PieceOrientation (quaternion xyzw), PieceCount, DebrisLifetime.
// Branch free: the CPU Vector VM rejects variables that only some branches write.
int PieceIndex = ExecIndex();
float4 StartValue;
PieceStart.Get(PieceIndex, StartValue);
float4 VelocityValue;
PieceVelocity.Get(PieceIndex, VelocityValue);
float4 SpinValue;
PieceSpin.Get(PieceIndex, SpinValue);
float4 OrientationValue;
PieceOrientation.Get(PieceIndex, OrientationValue);
// 1 for a burst slot that carries a panel of this hit, 0 for a spare slot, which dies at once.
float CarriesPiece = step((float)PieceIndex + 0.5, (float)PieceCount);
Particles.Position = StartValue.xyz;
Particles.PieceScale = StartValue.w;
Particles.Velocity = VelocityValue.xyz;
Particles.SpinAxis = SpinValue.xyz;
Particles.SpinSpeed = SpinValue.w;
Particles.MeshOrientation = OrientationValue;
Particles.Scale = float3(StartValue.w, StartValue.w, StartValue.w);
Particles.Resting = 0.0;
Particles.Lifetime = DebrisLifetime * CarriesPiece;
