/***** Resources *****/
#define SHOULD_FLIP_TEXTU_Y 1

cbuffer cbuf_per_obj {
	float4x4 wvp : WORLDVIEWPROJECTION <
		string UIWidget = "None";
	>;
}

RasterizerState discull {
	CullMode = NONE;
};

Texture2D color_texture <
string res_name = "default_color.dds";
string ui_name = "color texture";
string res_type = "2D";
>;

SamplerState color_sampler {
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = WRAP;
	AddressV = WRAP;
};
/***** Structs *****/
struct vs_inp {
	float4 obj_pos : POSITION;
	float2 textu_coord : TEXCOORD;
};
struct vs_out {
	float4 pos : SV_Position;
	float2 texu_coord : TEXCOORD;
};
/***** Utils *****/
float2
get_corrected_textu_coord (float2 textu_coord) {
	#if SHOULD_FLIP_TEXTU_Y
		return float2(textu_coord.x, 1.0 - textu_coord.y);
	#else
		return textu_coord;
	#endif
}
/***** Vertex Shader *****/
vs_out
vertex_shader (vs_inp input) {
    vs_out output = (vs_out)0;
    output.pos = mul(input.obj_pos, wvp);
    output.texu_coord = get_corrected_textu_coord(input.textu_coord);
    return output;
}
/***** Pixel Shader *****/
float4 pixel_shader (vs_out input):SV_Target {
    return color_texture.Sample(color_sampler, input.texu_coord);
}
/***** Techniques *****/
technique10 main10 {
    pass p0 {
        SetVertexShader(CompileShader(vs_4_0, vertex_shader()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, pixel_shader()));
        SetRasterizerState(discull);
    }
}




