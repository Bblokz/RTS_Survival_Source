// PROTOTYPE 2 deploy offset (Custom node body in MF_HQDeploy_VAT). Returns a LOCAL-space offset.
// Inputs: LP (pre-skinned local position), U4 = (piece u, pivot.x), U5 = (pivot.y, pivot.z), T (game time),
// CpdMode/CpdValue/CpdDuration/CpdDirection (custom primitive data 0..3, same meaning as prototype 1),
// Frames (rows in the textures), OffsetTex (RGB pivot offset cm, A uniform scale), RotTex (quaternion xyzw).
float u = U4.x;
if (u < 0.0)
{
    return float3(0, 0, 0); // truck: never moves
}
float D = CpdDuration > 0.01 ? CpdDuration : 12.0;
float P;
if (CpdMode < 0.5)
{
    float period = 2.7 * D;
    float c = fmod(T, period);
    P = c < 1.35 * D ? saturate(c / D) : 1.0 - saturate((c - 1.35 * D) / D);
}
else if (CpdMode < 1.5)
{
    P = saturate(CpdValue);
}
else
{
    float e = saturate((T - CpdValue) / D);
    P = CpdDirection < 0 ? 1.0 - e : e;
}
if (P >= 0.9999)
{
    return float3(0, 0, 0); // exact rest pose, no half-float rounding
}
float3 pivot = float3(U4.y, U5.x, U5.y);
float v = (P * (Frames - 1.0) + 0.5) / Frames;
float4 A = OffsetTex.SampleLevel(OffsetTexSampler, float2(u, v), 0);
float4 Q = RotTex.SampleLevel(RotTexSampler, float2(u, v), 0);
float qLen = length(Q);
Q = qLen > 1e-4 ? Q / qLen : float4(0, 0, 0, 1);
float3 rel = (LP - pivot) * A.w;
float3 rotated = rel + 2.0 * cross(Q.xyz, cross(Q.xyz, rel) + Q.w * rel);
return pivot + A.xyz + rotated - LP;
