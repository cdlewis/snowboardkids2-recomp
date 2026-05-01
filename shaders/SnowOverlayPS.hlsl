struct Input {
    float4x4 transform;
    float2 translation;
    float2 snowSize;
    float snowTime;
    float snowOpacity;
};

[[vk::push_constant]]
ConstantBuffer<Input> gInput : register(b0, space0);

float hash21(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float2 hash22(float2 p)
{
    float n = hash21(p);
    return float2(n, hash21(p + n + 17.0));
}

float flakeLayer(float2 uv, float scale, float fallSpeed, float drift, float sizeMin, float sizeMax, float presenceThreshold)
{
    float2 p = uv;
    p.x *= gInput.snowSize.x / max(gInput.snowSize.y, 1.0);
    p *= scale;
    p.y += gInput.snowTime * fallSpeed;
    p.x += sin(gInput.snowTime * 0.35 + p.y * 0.15) * drift;

    float2 cell = floor(p);
    float2 local = frac(p) - 0.5;
    float acc = 0.0;

    [unroll]
    for (int j = -1; j <= 1; ++j)
    {
        [unroll]
        for (int i = -1; i <= 1; ++i)
        {
            float2 offset = float2(float(i), float(j));
            float2 neighbor = cell + offset;
            float2 rnd = hash22(neighbor);
            float2 center = rnd - 0.5;
            float radius = lerp(sizeMin, sizeMax, hash21(neighbor + 19.7));

            float2 wobble;
            wobble.x = sin(gInput.snowTime * (0.4 + rnd.x * 0.6) + rnd.y * 6.2831) * 0.08;
            wobble.y = cos(gInput.snowTime * (0.3 + rnd.y * 0.5) + rnd.x * 6.2831) * 0.04;

            float dist = length(local - offset - center - wobble);
            float flake = 1.0 - smoothstep(radius * 0.35, radius, dist);
            float presence = step(presenceThreshold, hash21(neighbor + 43.1));
            acc += flake * presence;
        }
    }

    return acc;
}

void PSMain(
    in float4 iColor : COLOR,
    in float2 iUV : TEXCOORD,
    out float4 oColor : SV_TARGET
)
{
    float2 uv = saturate(iUV);
    float diagonalMask = smoothstep(-0.015, 0.015, uv.y - uv.x);
    float heightMask = smoothstep(0.40, 0.45, uv.y);
    float triangleMask = diagonalMask * heightMask;

    float large = flakeLayer(uv, 8.0, 0.018, 0.020, 0.14, 0.24, 0.70);
    float medium = flakeLayer(uv, 18.0, 0.035, 0.016, 0.075, 0.135, 0.62);
    float small = flakeLayer(uv, 34.0, 0.060, 0.012, 0.035, 0.070, 0.56);

    float snow = large * 0.70 + medium * 0.52 + small * 0.32;
    snow *= lerp(0.80, 1.10, uv.y);

    float alpha = saturate(snow) * 0.80 * gInput.snowOpacity * iColor.a * triangleMask;

    oColor = float4(0.96, 0.99, 1.0, alpha);
}
