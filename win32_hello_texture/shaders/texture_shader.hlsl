struct PixelShaderInput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D global_texture : register(t0);
SamplerState global_sampler : register(s0);

PixelShaderInput
VertexShader_Main (float4 p : POSITION, float4 uv : TEXCOORD) {
    PixelShaderInput result;
    result.position = p;
    result.uv = uv;
    return result;
}

float4
PixelShader_Main (PixelShaderInput input) : SV_Target {
    return global_texture.Sample(global_sampler, input.uv);
}

