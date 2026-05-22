import Composite;

VSOutput main(VSInput in)
{
    VSOutput out;

    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2(3.0, -1.0),
        float2(-1.0, 3.0)
    };

    out.position = float4(pos[in.vertexIndex], 0.0, 1.0);

    return out;
}