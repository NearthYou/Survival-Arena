#include "pch.h"
#include "Graphics.h"
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
#include "ReferenceEngineCounters.hpp"
#endif
#if defined(DXA_GAME_PROBE)
#include "ReferenceFrameCapture.hpp"
#endif

#include "D2DTextRenderer.h"

#define SHADOWMAP_SIZE 2048

Graphics::~Graphics()
{
	// 기존 코드 유지하고 추가
	for (int i = 0; i < GBUFFER_COUNT; ++i) {
		if (m_gBufferSRVs[i]) m_gBufferSRVs[i].Reset();
		if (m_gBufferRTVs[i]) m_gBufferRTVs[i].Reset();
		if (m_gBufferTextures[i]) m_gBufferTextures[i].Reset();
	}

	if (m_fullScreenQuadVB) m_fullScreenQuadVB.Reset();
	if (m_fullScreenQuadIB) m_fullScreenQuadIB.Reset();

	if (m_deviceContext != nullptr) {
		m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		m_deviceContext->Flush();
	}

	m_renderTargetView.Reset();
	m_depthStencilView.Reset();
	m_depthStencilTexture.Reset(); 
	m_shadowDSTexture.Reset();
	m_shadowDSV.Reset();

	if (m_shadowMap)
		m_shadowMap.reset();
	
	m_deviceContext.Reset();

	if (m_swapChain != nullptr)
		m_swapChain.Reset();
	if (m_device != nullptr)
		m_device.Reset();

	//D2DTextRenderer::DestroyInstance();
}

void Graphics::Init(HWND hwnd)
{
	m_hwnd = hwnd;

	CreateDeviceAndSwapChain();
	CreateRenderTargetView();
	CreateDepthStencilView();
	CreateRenderStates();

	CreateGBuffer();
	CreateFullScreenQuad();

	// D2D 초기화 (마지막에 추가)
	//D2DTextRenderer::GetInstance(); // 싱글톤 초기화


	SetViewport(GAME->GetGameDesc().width, GAME->GetGameDesc().height);
}

void Graphics::RenderBegin()
{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
    ReferenceDraws() = {};
#endif
	//마지막 단계에서 그려진 결과물을 가지고 뭘 할지. 
	//마지막 인자에 depthStencil을 쓰지 않았음. 이젠 최종적으로 깊이값을 채운다. 

	//깊이 값은 0 ~ 1
	//지금 그릴 거 깊이 값이 0.5인데, 이미 그려진 값이 0.35면 그 픽셀은 스킵. 
	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());
	m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), (float*)(&GAME->GetGameDesc().clearColor));

	//깊이 초기값을 왜 1로 세팅하는가? 깊이가 1이면 맨 뒤에 있는 것. 
	//가까운 것 0 ~ 1 먼 것. 깊이는 다음 물체가 그려질지 안 그려질지 결정하는 Z 깊이 값이구나.
	
	//스텐실이란. 원하는 뭔가에 따라서 그걸 구멍이 뚫려 그 부분만 바꾼다거나.
	//그런 고급 기법. 

	//스텐실 뷰는 카메라마다 초기화 하도록.  
	m_viewport.RSSetViewport();
}


void Graphics::RenderEnd()
{
#if defined(DXA_GAME_PROBE)
    ReferenceFlushFrame();
#endif

	#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
    HRESULT hr = m_swapChain->Present(0, 0);
#else
    HRESULT hr = m_swapChain->Present(1, 0);
#endif
    m_lastPresentResult = hr;
	CHECK(hr);
}

void Graphics::ClearDepthStencilView()
{
	m_deviceContext->ClearDepthStencilView(m_depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
}

void Graphics::ClearShadowDepthStencilView()
{
	m_deviceContext->ClearDepthStencilView(m_shadowDSV.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
}

void Graphics::ClearGBufferView()
{
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (int idx = 0; idx < GBUFFER_COUNT; ++idx) {
		m_deviceContext->ClearRenderTargetView(m_gBufferRTVs[idx].Get(), clearColor);
	}
}

void Graphics::SetShadowDepthStencilView()
{
	m_shadowVP.RSSetViewport();

	ID3D11RenderTargetView* renderTargets[1] = { 0 };
	m_deviceContext->OMSetRenderTargets(1, renderTargets, m_shadowDSV.Get());
}

void Graphics::SetRTVAndDSV()
{
	m_viewport.RSSetViewport();
	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(),
		m_depthStencilView.Get());
}

void Graphics::CreateDeviceAndSwapChain()
{
	DXGI_SWAP_CHAIN_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	{
		desc.BufferDesc.Width = GAME->GetGameDesc().width;
		desc.BufferDesc.Height = GAME->GetGameDesc().height;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//스캔 라인 그리기 모드
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		//크기 조정 모드
		desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 1;
		desc.OutputWindow = m_hwnd;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	}

	HRESULT hr = ::D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
#if defined(_DEBUG)
		D3D11_CREATE_DEVICE_DEBUG,
#else
        0,
#endif
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&desc,
		m_swapChain.GetAddressOf(),
		m_device.GetAddressOf(),
		nullptr,
		m_deviceContext.GetAddressOf()
	);

	CHECK(hr);
}
//RenderTargetView는 DX의 파이프라인에서 렌더링 타겟을 설정하기 위한 통일된 인터페이스. 
void Graphics::CreateRenderTargetView()
{
	HRESULT hr;

	ComPtr<ID3D11Texture2D> backBuffer = nullptr;
	hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());
	CHECK(hr);

	hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf());
	CHECK(hr);
}

void Graphics::CreateDepthStencilView()
{
	{
		D3D11_TEXTURE2D_DESC desc = { 0 };
		ZeroMemory(&desc, sizeof(desc));

		desc.Width = static_cast<uint32>(GAME->GetGameDesc().width);
		desc.Height = static_cast<uint32>(GAME->GetGameDesc().height);
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		//depth stencil 용도로 세팅. 
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		HRESULT hr = DEVICE->CreateTexture2D(&desc, nullptr, m_depthStencilTexture.GetAddressOf());
		CHECK(hr);


		desc.Width = SHADOWMAP_SIZE;
		desc.Height = SHADOWMAP_SIZE;
		desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		hr = DEVICE->CreateTexture2D(&desc, nullptr, m_shadowDSTexture.GetAddressOf());
		CHECK(hr);
	}

	{
		D3D11_DEPTH_STENCIL_VIEW_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Flags = 0;
		desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;

		HRESULT hr = DEVICE->CreateDepthStencilView(m_depthStencilTexture.Get(), &desc, m_depthStencilView.GetAddressOf());
		CHECK(hr);

		hr = DEVICE->CreateDepthStencilView(m_shadowDSTexture.Get(), &desc, m_shadowDSV.GetAddressOf());
		CHECK(hr);
	}

	{
		//데칼용 셰이더 리소스 뷰.
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		ComPtr<ID3D11ShaderResourceView> depthSRV;
		HRESULT hr = DEVICE->CreateShaderResourceView(m_depthStencilTexture.Get(), &srvDesc, depthSRV.GetAddressOf());
		CHECK(hr);

		// Texture 객체로 래핑해서 저장
		m_depthTexture = make_shared<Texture>();
		m_depthTexture->SetSRV(depthSRV);
	}

	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		ComPtr<ID3D11ShaderResourceView> srv;
		HRESULT hr = DEVICE->CreateShaderResourceView(m_shadowDSTexture.Get(), &srvDesc, srv.GetAddressOf());
		CHECK(hr);

		m_shadowMap = make_shared<Texture>();
		m_shadowMap->SetSRV(srv);

		
	}

}

void Graphics::CreateRenderStates()
{
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.StencilEnable = FALSE;


	DEVICE->CreateDepthStencilState(&depthDesc, m_decalDepthState.GetAddressOf());

	// 데칼용 블렌드 스테이트
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	DEVICE->CreateBlendState(&blendDesc, m_decalBlendState.GetAddressOf());

	// 기본 스테이트들도 생성 (복원용)
	D3D11_DEPTH_STENCIL_DESC defaultDepthDesc = {};
	defaultDepthDesc.DepthEnable = TRUE;
	defaultDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // 기본값
	defaultDepthDesc.DepthFunc = D3D11_COMPARISON_LESS;            // 기본값
	defaultDepthDesc.StencilEnable = FALSE;
	defaultDepthDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	defaultDepthDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	HRESULT hr = DEVICE->CreateDepthStencilState(&defaultDepthDesc, m_defaultDepthState.GetAddressOf());
	CHECK(hr);

	// 기본 블렌드 스테이트 (복원용) - 헬퍼 클래스 사용 안함
	D3D11_BLEND_DESC defaultBlendDesc = {};
	defaultBlendDesc.AlphaToCoverageEnable = FALSE;
	defaultBlendDesc.IndependentBlendEnable = FALSE;
	defaultBlendDesc.RenderTarget[0].BlendEnable = FALSE;          // 블렌딩 비활성화
	defaultBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	defaultBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	defaultBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	defaultBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	defaultBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	defaultBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	defaultBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = DEVICE->CreateBlendState(&defaultBlendDesc, m_defaultBlendState.GetAddressOf());
	CHECK(hr);
}



void Graphics::BeginGeometryPass()
{
	m_currentPass = RenderPass::GEOMETRY;
	m_deviceContext->OMSetRenderTargets(4, m_gBufferRTVs->GetAddressOf(), m_depthStencilView.Get());


	for (int idx = 0; idx < GBUFFER_COUNT; ++idx) {
		m_deviceContext->ClearRenderTargetView(m_gBufferRTVs[idx].Get(), (float*)(&GAME->GetGameDesc().clearColor));
	}
}

void Graphics::BeginLightingPass()
{
	m_currentPass = RenderPass::LIGHTING;
	//최종 렌더타겟으로 전환
	m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
	m_deviceContext->PSSetShaderResources(0, 4, m_gBufferSRVs->GetAddressOf());
}

void Graphics::CreateGBuffer()
{
	UINT width = GAME->GetGameDesc().width;
	UINT height = GAME->GetGameDesc().height;

	// Albedo Buffer (RGBA8)
	D3D11_TEXTURE2D_DESC albedoDesc = {};
	albedoDesc.Width = width;
	albedoDesc.Height = height;
	albedoDesc.MipLevels = 1;
	albedoDesc.ArraySize = 1;
	albedoDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	albedoDesc.SampleDesc.Count = 1;
	albedoDesc.SampleDesc.Quality = 0;
	albedoDesc.Usage = D3D11_USAGE_DEFAULT;
	albedoDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	albedoDesc.CPUAccessFlags = 0;
	albedoDesc.MiscFlags = 0;

	HRESULT hr = m_device->CreateTexture2D(&albedoDesc, nullptr, m_gBufferTextures[0].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateRenderTargetView(m_gBufferTextures[0].Get(), nullptr, m_gBufferRTVs[0].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateShaderResourceView(m_gBufferTextures[0].Get(), nullptr, m_gBufferSRVs[0].GetAddressOf());
	CHECK(hr);

	// Normal Buffer (RGBA16_FLOAT)
	D3D11_TEXTURE2D_DESC normalDesc = albedoDesc;
	normalDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	hr = m_device->CreateTexture2D(&normalDesc, nullptr, m_gBufferTextures[1].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateRenderTargetView(m_gBufferTextures[1].Get(), nullptr, m_gBufferRTVs[1].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateShaderResourceView(m_gBufferTextures[1].Get(), nullptr, m_gBufferSRVs[1].GetAddressOf());
	CHECK(hr);

	// Position Buffer (RGBA16_FLOAT)
	D3D11_TEXTURE2D_DESC positionDesc = albedoDesc;
	positionDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	// 더 높은 정밀도가 필요하다면 RGBA32_FLOAT 사용 가능
	// positionDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	hr = m_device->CreateTexture2D(&positionDesc, nullptr, m_gBufferTextures[2].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateRenderTargetView(m_gBufferTextures[2].Get(), nullptr, m_gBufferRTVs[2].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateShaderResourceView(m_gBufferTextures[2].Get(), nullptr, m_gBufferSRVs[2].GetAddressOf());
	CHECK(hr);

	// Material Buffer (RGBA8)
	D3D11_TEXTURE2D_DESC materialDesc = albedoDesc;
	materialDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	hr = m_device->CreateTexture2D(&materialDesc, nullptr, m_gBufferTextures[3].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateRenderTargetView(m_gBufferTextures[3].Get(), nullptr, m_gBufferRTVs[3].GetAddressOf());
	CHECK(hr);
	hr = m_device->CreateShaderResourceView(m_gBufferTextures[3].Get(), nullptr, m_gBufferSRVs[3].GetAddressOf());
	CHECK(hr);
}

void Graphics::CreateFullScreenQuad()
{
	QuadVertex vertices[] = {
		{ Vec3(-1.0f, -1.0f, 0.0f), Vec2(0.0f, 1.0f) }, // 좌하단
		{ Vec3(-1.0f,  1.0f, 0.0f), Vec2(0.0f, 0.0f) }, // 좌상단
		{ Vec3(1.0f,  1.0f, 0.0f), Vec2(1.0f, 0.0f) }, // 우상단
		{ Vec3(1.0f, -1.0f, 0.0f), Vec2(1.0f, 1.0f) }  // 우하단
	};

	UINT indices[] = { 0, 1, 2, 0, 2, 3 };

	// 버텍스 버퍼 생성
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(vertices);
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;

	HRESULT hr = m_device->CreateBuffer(&vbDesc, &vbData, m_fullScreenQuadVB.GetAddressOf());
	CHECK(hr);

	// 인덱스 버퍼 생성
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(indices);
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;

	hr = m_device->CreateBuffer(&ibDesc, &ibData, m_fullScreenQuadIB.GetAddressOf());
	CHECK(hr);
}



void Graphics::SetViewport(float _width, float _height, float _x, float _y, float _minDepth, float _maxDepth)
{
	m_viewport.Set(_width, _height, _x, _y, _minDepth, _maxDepth);
	m_shadowVP.Set(SHADOWMAP_SIZE, SHADOWMAP_SIZE, _x, _y, _minDepth, _maxDepth);
}

void Graphics::BindGBufferSRVs(UINT _startSlot) const
{
	m_deviceContext->PSSetShaderResources(_startSlot, GBUFFER_COUNT, m_gBufferSRVs->GetAddressOf());
}

ID3D11ShaderResourceView* Graphics::GetGBufferSRV(int _idx) const
{
	return (_idx < 0 || _idx >= GBUFFER_COUNT) ? nullptr : m_gBufferSRVs[_idx].Get();
}

void Graphics::BindFullScreenQuad() const
{
	UINT stride = sizeof(Vec3) + sizeof(Vec2);
	UINT offset = 0;
	ID3D11Buffer* vb = m_fullScreenQuadVB.Get();
	m_deviceContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	m_deviceContext->IASetIndexBuffer(m_fullScreenQuadIB.Get(), DXGI_FORMAT_R32_UINT, 0);
	m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

