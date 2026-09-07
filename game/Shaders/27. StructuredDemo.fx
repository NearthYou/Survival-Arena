
struct InputDesc
{
    matrix input;
};

struct OutputDesc
{
    matrix result;
};
StructuredBuffer<InputDesc> Input;
RWStructuredBuffer<OutputDesc> Output;


//이게 뭘 말하는가? 
//일종의 군대라고 생각하기. Cell단위로. 오와 열을 이어서. 
//3차원의 배열 10 * 8 * 3 = 240개 thread

//(7,5,0)그룹 내에서는 유일, 여러 그룹에서는 고유하지 않음. 


//DispatchThreadID는 모든 스레드에서 고유하다. 
//왜 3차원으로 했을까? 셰이더를 통해 작업할 때는 텍스쳐는 2차원 데이터고
//그래서 그런 차원 지원을 하지 않았을까. 생각한다. 
//무한으로 막 늘릴 순 없음. 고정적으로 1024개 여야함.
//그 이상으로 가려면 그룹을 나누어야 함. 대부분의 경우는 적절한 분배로 하나의 스레드가 어느 부분을 담당한다. 


[numthreads(500, 1, 1)]
void CS(uint id : SV_GroupIndex)
{
    matrix result = Input[id].input * 2;

    Output[id].result = result;
}

technique11 T0
{
	//여러 개의 패스를 둔다. 
   
    pass P0
    {
        SetVertexShader(NULL);
        SetPixelShader(NULL);
        SetComputeShader(CompileShader(cs_5_0, CS()));

    }

};
