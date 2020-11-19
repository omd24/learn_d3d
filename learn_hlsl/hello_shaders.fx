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

float4 vertex_shader (float3 obj_pos : POSITION) : SV_Position {
	return mul(float4(obj_pos, 1), wvp);
}

float4 pixel_shader() : SV_Target {
	return float4(0, 0, 1, 1);
}

technique10 main10 {
	pass p0 {
		SetVertexShader(CompileShader(vs_4_0, vertex_shader()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, pixel_shader()));
		
		SetRasterizerState(DisableCulling);
	}
}