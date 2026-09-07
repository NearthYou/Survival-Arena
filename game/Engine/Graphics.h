#pragma once
#include "Viewport.h"

class Texture;

class Graphics
{
	DECLARE_SINGLE(Graphics);

public:
	~Graphics();

public:
	void Init(HWND hwnd);


	void RenderBegin();
	void RenderEnd();

	HRESULT GetLastPresentResult() const { return m_lastPresentResult; }
	ComPtr<ID3D11Device> GetDevice() { return m_device; }
	ComPtr<ID3D11DeviceContext> GetDeviceContext() { return m_deviceContext; }

	void ClearDepthStencilView();
	void ClearShadowDepthStencilView();
	void ClearGBufferView();
	void SetShadowDepthStencilView();
	void SetRTVAndDSV();

	void Cleanup();

private:
    HRESULT m_lastPresentResult = S_OK;
	void CreateDeviceAndSwapChain();
	void CreateRenderTargetView();
	void CreateDepthStencilView();
	void CreateRenderStates();
public:
	//디퍼드 렌더링 용.
	void BeginGeometryPass();
	void BeginLightingPass();
	void CreateGBuffer();
	void CreateFullScreenQuad();

	void BindGBufferSRVs(UINT _startSlot = 0) const;
	ID3D11ShaderResourceView* GetGBufferSRV(int _idx) const;
	void BindFullScreenQuad() const;
public:
	void SetViewport(float _width, float _height, float _x = 0, float _y = 0, float _minDepth = 0, float _maxDepth = 1);
	Viewport& GetViewport() { return m_viewport; }
	Viewport& GetShadowViewport() { return m_shadowVP; }
	int GetGBUFFER_COUNT() { return GBUFFER_COUNT; }
	shared_ptr<Texture> GetShadowMap() { return m_shadowMap; }
	HWND GetHwnd() { return m_hwnd; }

	ComPtr<ID3D11RenderTargetView>* GetGBuffersRTVs() { return m_gBufferRTVs; }
	ComPtr<ID3D11DepthStencilView> GetDepthStencilView() { return m_depthStencilView; }
	ComPtr<ID3D11RenderTargetView> GetRenderTargetView() { return m_renderTargetView; }
	ComPtr<IDXGISwapChain> GetSwapChain() { return m_swapChain; }
	shared_ptr<Texture> GetDepthTexture() const { return m_depthTexture; }

	// 풀스크린 쿼드 접근 함수들 추가
	ComPtr<ID3D11Buffer> GetFullScreenQuadVB() { return m_fullScreenQuadVB; }
	ComPtr<ID3D11Buffer> GetFullScreenQuadIB() { return m_fullScreenQuadIB; }

	ComPtr<ID3D11DepthStencilState> GetDecalDepthStencilState() { return m_decalDepthState; }
	ComPtr<ID3D11BlendState> GetDecalBlendState() { return m_decalBlendState; }

private:
	HWND m_hwnd = {};

	// Device & SwapChain
	ComPtr<ID3D11Device> m_device = nullptr;
	ComPtr<ID3D11DeviceContext> m_deviceContext = nullptr;
	ComPtr<IDXGISwapChain> m_swapChain = nullptr;

	// RTV
	//셰이더에서 계산을 해서 그려주는 뷰. 
	ComPtr<ID3D11RenderTargetView> m_renderTargetView;

	//DSV(Depth Stencil View)
	//그림자는 카메라를 조명 위치에서 한 번 더 연산 한 다음에
	//깊이 값을 대상으로 그림. 
	ComPtr<ID3D11Texture2D> m_depthStencilTexture;
	ComPtr<ID3D11DepthStencilView> m_depthStencilView;
	ComPtr<ID3D11Texture2D> m_shadowDSTexture;
	ComPtr<ID3D11DepthStencilView> m_shadowDSV;
	shared_ptr<Texture> m_shadowMap;
	shared_ptr<Texture> m_depthTexture;


	//State들. 
	ComPtr<ID3D11DepthStencilState> m_decalDepthState;
	ComPtr<ID3D11BlendState> m_decalBlendState;
	ComPtr<ID3D11DepthStencilState> m_defaultDepthState;
	ComPtr<ID3D11BlendState> m_defaultBlendState;

	// Misc
	Viewport m_viewport;
	Viewport m_shadowVP;

public:
	static const int GBUFFER_COUNT = 4;
	enum class RenderPass { NONE, GEOMETRY, LIGHTING, FORWARD};

public:
	RenderPass GetCurrentPass() const { return m_currentPass; }
	bool IsCurrentPassGeometry() const { return m_currentPass == RenderPass::GEOMETRY; }

public:
	ComPtr<ID3D11Texture2D> m_gBufferTextures[GBUFFER_COUNT];
	ComPtr<ID3D11RenderTargetView> m_gBufferRTVs[GBUFFER_COUNT];
	ComPtr<ID3D11ShaderResourceView> m_gBufferSRVs[GBUFFER_COUNT];

	// 풀스크린 쿼드용
	ComPtr<ID3D11Buffer> m_fullScreenQuadVB;
	ComPtr<ID3D11Buffer> m_fullScreenQuadIB;
	RenderPass m_currentPass = RenderPass::NONE;

};

