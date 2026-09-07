


RWByteAddressBuffer Output; // UAV

struct ComputeInput
{
    uint3 groupID : SV_GroupID;
    uint3 groupThreadID : SV_GroupThreadID;
    uint3 dispatchThreadID : SV_DispatchThreadID;
    uint groupIndex : SV_GroupIndex;
};


//이게 뭘 말하는가? 
//일종의 군대라고 생각하기. Cell단위로. 오와 열을 이어서. 
//3차원의 배열 10 * 8 * 3 = 240개 thread

//(7,5,0)그룹 내에서는 유일, 여러 그룹에서는 고유하지 않음. 


//DispatchThreadID는 모든 스레드에서 고유하다. 
[numthreads(10,8,3)]
void CS(ComputeInput input)
{
    uint index = input.groupIndex;
    uint outAddress = index * 10 * 4;
    
    Output.Store3(outAddress + 0, input.groupID);
    Output.Store3(outAddress + 12, input.groupThreadID);
    Output.Store3(outAddress + 24, input.dispatchThreadID);
    Output.Store(outAddress + 36, input.groupIndex);
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
