struct PSInput {
    float4 position : SV_Position;
    float4 color : COLOR;
};

PSInput
VSMain (float4 p : POSITION, float4 c : COLOR) {
    PSInput result;
    result.position = p;
    result.color = c;
    return result;
}

float4
PSMain (PSInput input) : SV_Target {
    return input.color;
}
