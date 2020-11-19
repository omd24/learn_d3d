cbuffer ConstBufPerObj {
    float4x4 wvp : WORLDVIEWPROJECTION;
}

RasterizerState DisableCulling {
    CullMode = NONE;
};

struct VS_INPUT {
    float4 obj_pos : POSITION;
};

struct VS_OUTPUT {
    float4 pos : SV_Position;
};

VS_OUTPUT vertex_shader (VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = mul(input.obj_pos, wvp);
    return output;
}

float4 pixel_shader (VS_OUTPUT input) : SV_Target {
    return float4(1, 0, 0, 1);
}

technique10 main10 {
    pass p0 {
        SetVertexShader(CompileShader(vs_4_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, pixel_shader()));

        SetRasterizerState(DisableCulling);
    }
}
