
struct InputVS
{
	float3	pos			: POSITION;
	float2  uv			: TEXCOORD;
	float4	color		: COLOR;
};
struct OutputVS
{
	float4	pos     	: SV_POSITION;
    float2  uv			: TEXCOORD0;
	float4	color		: COLOR0;
};
 
OutputVS VS(InputVS input)
{
	OutputVS result;
	result.pos = float4(input.pos,1);
	result.uv = input.uv;
	result.color = input.color;
 
	return result;
}


float4 GetColor(int x, int y)
{

    static const int kirbyDot[16][16] =
    {
        {0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0},
        {0,0,0,0,1,1,2,3,3,3,2,1,1,0,0,0},
        {0,0,0,1,2,3,3,3,3,3,3,3,2,1,0,0},
        {0,0,1,2,3,3,3,3,3,3,3,3,3,1,0,0},
        {0,0,1,3,3,3,3,3,3,3,3,3,3,2,1,0},
        {0,1,3,3,3,3,3,3,3,1,3,1,3,2,1,0},
        {1,2,3,3,3,3,3,3,3,1,3,1,3,3,3,1},
        {1,3,3,3,3,3,3,3,3,1,3,1,3,3,3,1},
        {1,3,3,3,3,3,2,2,3,3,3,3,2,2,3,1},
        {1,2,3,3,2,3,3,3,3,3,3,3,3,2,3,1},
        {0,1,2,3,1,3,3,3,3,3,1,3,3,1,2,1},
        {0,0,1,1,2,3,3,3,3,3,3,3,2,1,1,0},
        {0,0,0,1,1,2,2,3,3,3,3,2,1,1,0,0},
        {0,0,1,2,2,1,1,1,1,1,1,1,2,2,1,0},
        {0,1,2,2,2,2,2,1,1,1,1,2,2,2,2,1},
        {0,0,1,1,1,1,1,0,0,0,0,1,1,1,1,0},
    };

    int colorNum = kirbyDot[y][x];

    switch (colorNum)
    {
    case 0:return float4(1, 1, 1, 0);
    case 1:return float4(0, 0, 0, 1);
    case 2:return float4(0.9137,0.3411,0.5843,1);
    case 3:return float4(0.949,0.6352,0.7411, 1);
    }

    return float4(0, 0, 0, 0);

}
 
float4 PS(OutputVS input) : SV_TARGET
{
    float4 outPut = input.color;
    const float2 uv = input.uv;

    int x = uv.x * 16;
    int y = uv.y * 16;

    outPut = GetColor(x, y);


    if (outPut.a == 0)
        discard;

    return outPut;
}