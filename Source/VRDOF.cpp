/*
rF2 VR DOF
*/


#include "VRDOF.hpp"
#include "hooking.h"
#include "shaders.h"
#include <d3d11.h>
//#include <d3dcompiler.h>
#include <dxgi.h>
#include <cstdio>
#include <string>
#include <cmath>

#include <openvr.h>
#pragma comment(lib, "openvr_api.lib")

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
//#pragma comment(lib, "d3dcompiler_43.lib")



// plugin information

extern "C" __declspec(dllexport)
    const char * __cdecl GetPluginName()                   { return PLUGIN_NAME; }

extern "C" __declspec(dllexport)
    PluginObjectType __cdecl GetPluginType()               { return PO_INTERNALS; }

extern "C" __declspec(dllexport)
    int __cdecl GetPluginVersion()                         { return 6; }

extern "C" __declspec(dllexport)
    PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new VRDOFPlugin); }

extern "C" __declspec(dllexport)
    void __cdecl DestroyPluginObject(PluginObject *obj)    { delete((VRDOFPlugin *)obj); }


FILE* out_file = NULL;

bool g_realtime = false;
bool g_messageDisplayed = false;

ID3D11Device* g_tempd3dDevice = NULL;
ID3D11Device* g_d3dDevice = NULL;
IDXGISwapChain* g_swapchain = NULL;
ID3D11DeviceContext* g_context = NULL;
vr::IVRCompositor* g_compositor = NULL;
    
ID3D11Texture2D* g_depthTexture = NULL;

typedef vr::EVRCompositorError(__fastcall *SubmitHook) (void* pThis, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags);

SubmitHook g_oldSubmit = NULL;

pD3DCompile pShaderCompiler = NULL;
ID3D11VertexShader *g_pVS = NULL;
ID3D11PixelShader *g_pPS;   
ID3D11InputLayout *g_pLayout;
ID3D11Buffer *g_pVBuffer;
ID3D11RasterizerState* g_rs;
ID3D11SamplerState* g_d3dSamplerState;
ID3D11ShaderResourceView* g_DepthShaderResourceView;
ID3D11ShaderResourceView* g_ColorShaderResourceView;
ID3D11DepthStencilState* g_DepthStencilState; //to disable depth writes
ID3D11Texture2D* g_renderTargetTextureMap;
ID3D11RenderTargetView* g_renderTargetViewMap;


void draw(){
	g_context->OMSetRenderTargets( 1, &g_renderTargetViewMap, NULL);
	float bgColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    g_context->ClearRenderTargetView(g_renderTargetViewMap, bgColor);
	g_context->RSSetState(g_rs);
    g_context->VSSetShader(g_pVS, 0, 0);
    g_context->PSSetShader(g_pPS, 0, 0);
    g_context->IASetInputLayout(g_pLayout);
	g_context->PSSetSamplers(0, 1, &g_d3dSamplerState);
	g_context->PSSetShaderResources(0, 1, &g_DepthShaderResourceView);
	g_context->PSSetShaderResources(1, 1, &g_ColorShaderResourceView);
	g_context->OMSetDepthStencilState(g_DepthStencilState, 0);
    UINT stride = sizeof(float)*3;
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);
    g_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
    g_context->Draw(6, 0);
	
}


bool testCompositor(DWORD* current){
	__try{
		vr::IVRCompositor* c = (vr::IVRCompositor*)current;
		c->CanRenderScene();
    }__except(1){
		return false;
	}
	return true;
}


vr::EVRCompositorError __fastcall hookedSubmit(void* pThis, vr::EVREye eEye, const vr::Texture_t *pTexture, const vr::VRTextureBounds_t* pBounds, vr::EVRSubmitFlags nSubmitFlags){
	if(g_realtime && pShaderCompiler && g_pVS){
		ID3D11PixelShader* oldPS;
		ID3D11VertexShader* oldVS;
		ID3D11ClassInstance* PSclassInstances[256]; // 256 is max according to PSSetShader documentation
		ID3D11ClassInstance* VSclassInstances[256];
		UINT psCICount = 256;
		UINT vsCICount = 256;
		ID3D11Buffer* oldVertexBuffers;
		UINT oldStrides;
		UINT oldOffsets;
		ID3D11InputLayout* oldLayout;
		D3D11_PRIMITIVE_TOPOLOGY oldTopo;
		ID3D11Buffer* oldPSConstantBuffer0;
		ID3D11Buffer* oldVSConstantBuffer0;
		ID3D11Buffer* oldPSConstantBuffer1;
		ID3D11Buffer* oldVSConstantBuffer1;
		ID3D11RasterizerState* rs;
		ID3D11SamplerState* ss;
		ID3D11ShaderResourceView* srv[2];
		ID3D11DepthStencilState* dss;
		UINT stencilRef;
		ID3D11RenderTargetView* rtv;
		ID3D11DepthStencilView* dsv;

		//What the hell is all this crap, you may ask?
		//Well rf2 seems to use gets to retrieve resources that were used when drawing the last frame
		//And then attemps to use them, if we dont revert all state back to how we found it, then there will be graphical glitches
		//Evidence of this? I found a pointer to g_PS in a trace of d3d11 calls outside of our hooked Present
		g_context->OMGetRenderTargets(1, &rtv, &dsv);
		g_context->RSGetState(&rs);
		g_context->PSGetSamplers(0, 1, &ss);
		g_context->PSGetShaderResources(0, 1, &srv[0]);
		g_context->PSGetShaderResources(1, 1, &srv[1]);
		g_context->OMGetDepthStencilState(&dss, &stencilRef);
		g_context->PSGetShader(&oldPS, PSclassInstances, &psCICount);
		g_context->VSGetShader(&oldVS, VSclassInstances, &vsCICount);
		g_context->IAGetInputLayout(&oldLayout);
		g_context->PSGetConstantBuffers(0, 1, &oldPSConstantBuffer0);
		g_context->VSGetConstantBuffers(0, 1, &oldVSConstantBuffer0);
		g_context->PSGetConstantBuffers(1, 1, &oldPSConstantBuffer1);
		g_context->VSGetConstantBuffers(1, 1, &oldVSConstantBuffer1);
		g_context->IAGetVertexBuffers(0, 1, &oldVertexBuffers, &oldStrides, &oldOffsets);
		g_context->IAGetPrimitiveTopology(&oldTopo);


		// Is there a better way of doing this? I just want to update the texture...
		SAFE_RELEASE(g_ColorShaderResourceView);
		g_d3dDevice->CreateShaderResourceView((ID3D11Texture2D*)(pTexture->handle), NULL, &g_ColorShaderResourceView);
		
		draw();//Actually do our stuff...

		g_context->OMSetRenderTargets(1, &rtv, dsv);
		g_context->RSSetState(rs);
		g_context->PSSetSamplers(0, 1, &ss);
		g_context->PSSetShaderResources(0, 1, &srv[0]);
		g_context->PSSetShaderResources(1, 1, &srv[1]);
		g_context->OMSetDepthStencilState(dss, stencilRef);
		g_context->PSSetShader(oldPS, PSclassInstances, psCICount);
		g_context->VSSetShader(oldVS, VSclassInstances, vsCICount);
		g_context->IASetInputLayout(oldLayout);
		g_context->PSSetConstantBuffers(0, 1, &oldPSConstantBuffer0);
		g_context->VSSetConstantBuffers(0, 1, &oldVSConstantBuffer0);
		g_context->PSSetConstantBuffers(1, 1, &oldPSConstantBuffer1);
		g_context->VSSetConstantBuffers(1, 1, &oldVSConstantBuffer1);
		g_context->IASetVertexBuffers(0, 1, &oldVertexBuffers, &oldStrides, &oldOffsets);
		g_context->IASetPrimitiveTopology(oldTopo);
		
		vr::Texture_t vrTexture = { ( void * ) g_renderTargetTextureMap, vr::TextureType_DirectX, vr::ColorSpace_Gamma };
		return g_oldSubmit(pThis, eEye, &vrTexture, pBounds, nSubmitFlags);
	}

	return g_oldSubmit(pThis, eEye, pTexture, pBounds, nSubmitFlags);

}

VRDOFPlugin::~VRDOFPlugin(){
    SAFE_RELEASE(g_pLayout)
    SAFE_RELEASE(g_pVS)
    SAFE_RELEASE(g_pPS)
    SAFE_RELEASE(g_pVBuffer)
}

void VRDOFPlugin::WriteLog(const char * const msg)
{
    if (out_file == NULL)
        out_file = fopen(LOG_FILE, "w");

    if (out_file != NULL){
        fprintf(out_file, "%s\n", msg);
        fflush(out_file);
    }
}


void VRDOFPlugin::Load()
{
    WriteLog("--LOAD--");
    //D3DCompiler is a hot mess, basically there can be different versions of the dll depending on the windows version
    //rFactor2 is currently linked against 43, probably to support Windows 7, but that can change, so we try to find which one the
    //game has already loaded and use that one
#define DLL_COUNT 3
    if(!pShaderCompiler){
        static const char *d3dCompilerNames[] = {"d3dcompiler_43.dll", "d3dcompiler_46.dll", "d3dcompiler_47.dll"};
        HMODULE D3DCompilerModule = NULL;
        size_t i = 0;
        for (; i < DLL_COUNT; ++i){
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, d3dCompilerNames[i], &D3DCompilerModule)){
                break;
            }
        }
        if(D3DCompilerModule){
            WriteLog("Using:");
            WriteLog(d3dCompilerNames[i]);
            pShaderCompiler = (pD3DCompile)GetProcAddress(D3DCompilerModule, "D3DCompile");
        }else{
            WriteLog("Failed to load d3dcompiler");
        }
    }


    //Get a pointer to the SwapChain and Init the pipeline to draw the lights
    if(!g_swapchain){
        g_swapchain = getDX11SwapChain();
        if(g_swapchain){
            WriteLog("Found SwapChain");
            g_swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_d3dDevice);
            if(g_d3dDevice){
                WriteLog("Found DX11 device");
                g_d3dDevice->GetImmediateContext(&g_context);
                if(g_context){
                    WriteLog("Found Context");
                    InitPipeline();
                }
            }
            
        }
    }
}

bool testTexture(DWORD* current){
	static int count = 1; //1 is probably it, 2 is unknown, 3 is too small, 4 is global shadow projections, 5 is unknown (but intriguing, also wrong size), 6 is too small,
	__try{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		ID3D11Texture2D* t = (ID3D11Texture2D*)current;
		D3D11_TEXTURE2D_DESC desc;
		t->GetDesc(&desc);
		format = desc.Format;
		fprintf(out_file, "Found Texture: %d x %d, %d %s MSAA %d\n", desc.Width, desc.Height, desc.Format, (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)?"true":"false", desc.SampleDesc.Count);
		fflush(out_file);
		if(format == DXGI_FORMAT_R24G8_TYPELESS){
			return !(--count);
		}
	}__except(1){
		return false;
	}
	return false;
}



void VRDOFPlugin::EnterRealtime()
{
    g_realtime = true;
    WriteLog("---ENTERREALTIME---");

    if(!g_oldSubmit){
		vr::IVRCompositor* searchCompositor = vr::VRCompositor(); //Very important: Read comment on openvr.h:3160 about matching versions
		DWORD pVtable = *(DWORD*)searchCompositor;
		g_compositor = (vr::IVRCompositor*) findInstance(searchCompositor, pVtable, testCompositor);
		if(g_compositor)
			WriteLog("Found Compositor");
		else
			WriteLog("Unable to find Compositor");
        //Only hook submit when entering realtime to prevent things like the steam overlay from overriding our hook
		DWORD_PTR* pCompositorVtable = (DWORD_PTR*)g_compositor;
        pCompositorVtable = (DWORD_PTR*)pCompositorVtable[0];
		//call qword [r10 + 0x28]
		// 0x28 = 40
		// 40/8 = 5 (bytes to 64bit words)
		g_oldSubmit = (SubmitHook)placeDetour((BYTE*)pCompositorVtable[5], (BYTE*)hookedSubmit, 0);
		if(g_oldSubmit){
			WriteLog("Submit Hook successfull");
		}else{
			WriteLog("Unable to hook Submit");
		}
		if(g_oldSubmit){
			ID3D11Texture2D *pSearchTexture;
			CreateSearchTexture(g_d3dDevice, &pSearchTexture);
			DWORD pVtable = *(DWORD*)pSearchTexture;
			g_depthTexture = (ID3D11Texture2D*) findInstance(pSearchTexture, pVtable, testTexture);
			if(g_depthTexture){
				WriteLog("Found depth buffer texture");
				g_d3dDevice->CreateShaderResourceView(g_depthTexture, NULL, &g_DepthShaderResourceView);
			}else
				WriteLog("No depth buffer texture found!");
			SAFE_RELEASE(pSearchTexture);
		}
		if(g_DepthShaderResourceView){
			//Create a new render target
			D3D11_TEXTURE2D_DESC depthDesc;
			g_depthTexture->GetDesc(&depthDesc);
		
		
		
			D3D11_TEXTURE2D_DESC textureDesc;
			D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;

			ZeroMemory(&textureDesc, sizeof(textureDesc));
			textureDesc.Width = depthDesc.Width;
			textureDesc.Height = depthDesc.Height;
			textureDesc.MipLevels = 1;
			textureDesc.ArraySize = 1;
			textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.Usage = D3D11_USAGE_DEFAULT;
			textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			textureDesc.CPUAccessFlags = 0;
			textureDesc.MiscFlags = 0;

			g_d3dDevice->CreateTexture2D(&textureDesc, NULL, &g_renderTargetTextureMap);
		
			renderTargetViewDesc.Format = textureDesc.Format;
			renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			renderTargetViewDesc.Texture2D.MipSlice = 0;

			// Create the render target view.
			g_d3dDevice->CreateRenderTargetView(g_renderTargetTextureMap, &renderTargetViewDesc, &g_renderTargetViewMap);

			/*D3D11_MAPPED_SUBRESOURCE ms;
			g_context->Map(g_pViewportCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
			cbViewport* viewportDataPtr = (cbViewport*)ms.pData;
			viewportDataPtr->width = (float)(textureDesc.Width);
			viewportDataPtr->height = (float)(textureDesc.Height);
			g_context->Unmap(g_pViewportCBuffer, NULL);*/
		}
	}
    if(!g_d3dDevice || !g_swapchain || !g_context)
        WriteLog("Failed to find dx11 resources");
}

void VRDOFPlugin::ExitRealtime(){
    g_realtime = false;
    WriteLog("---EXITREALTIME---");
}

bool VRDOFPlugin::WantsToDisplayMessage( MessageInfoV01 &msgInfo ){
	if(g_realtime && !g_messageDisplayed){
        
        if(g_d3dDevice && g_swapchain && g_oldSubmit && g_pPS)
            sprintf(msgInfo.mText, "VRDOF OK");
        else
            sprintf(msgInfo.mText, "VRDOF FAILURE");

        g_messageDisplayed = true;
        return true;
    }
    return false;
}



bool testSwapChain(DWORD* current){
	__try{
		IDXGISwapChain* sc = (IDXGISwapChain*)current;
        ID3D11Device* dev = NULL;
        sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev);
    }__except(1){
		return false;
	}
	return true;
}


void VRDOFPlugin::CreateSearchTexture(ID3D11Device* pDevice, ID3D11Texture2D** pTexture){
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = 1920;
	depthStencilDesc.Height = 1080;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	pDevice->CreateTexture2D(&depthStencilDesc, 0, pTexture);
}


void VRDOFPlugin::CreateInvisibleWindow(HWND* hwnd){
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"FY";
    RegisterClassExW(&wc);
    *hwnd = CreateWindowExW(0,
                             L"FY",
                             L"null",
                             WS_OVERLAPPEDWINDOW,
                             300, 300,
                             640, 480,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr);
}

void VRDOFPlugin::CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext){
    HRESULT hr;

    ID3D11DeviceContext* pd3dDeviceContext = NULL;
    hr = D3D11CreateDevice(NULL,                               
                            D3D_DRIVER_TYPE_HARDWARE,
                            ( HMODULE )0,
                            0,
                            NULL,
                            0,
                            D3D11_SDK_VERSION,
                            pDevice,
                            NULL,
                            pContext);

    if(*pDevice)
        WriteLog("Created device OK");
    else{
        WriteLog("Failed to create device");
    }
    
}

void VRDOFPlugin::CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd){
    HRESULT hr;

    IDXGIFactory1 * pIDXGIFactory1 = NULL;
    CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
    if(pIDXGIFactory1)
        WriteLog("Create Factory OK");
    else{
        WriteLog("Failed to create factory1");
        return;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE; //(GetWindowLong(hWnd, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    hr = pIDXGIFactory1->CreateSwapChain(device, &swapChainDesc, tempSwapChain);
        

    if(*tempSwapChain){
        WriteLog("Created SwapChain OK");
    }else{
        WriteLog("Failed to create SwapChain");
    }
    
    SAFE_RELEASE(pIDXGIFactory1)
}
 
IDXGISwapChain* VRDOFPlugin::getDX11SwapChain(){
    IDXGISwapChain* pSearchSwapChain = NULL;    
    ID3D11Device* pDevice = NULL;
    ID3D11DeviceContext* pContext = NULL;
    
    WriteLog("Creating D3D11 Device");
    CreateSearchDevice(&pDevice, &pContext);
    if(!pDevice)
        return NULL;

    WriteLog("Getting window handle");
    HWND hWnd;
    CreateInvisibleWindow(&hWnd);
    if (hWnd == NULL){
        WriteLog("Failed to get HWND");
        return NULL;
    }

    WriteLog("Creating fake swapChain");
    CreateSearchSwapChain(pDevice, &pSearchSwapChain, hWnd);
    if(pSearchSwapChain)
        WriteLog("Successfully created swap chain");
    else{
        WriteLog("FAILED TO CREATE SWAP CHAIN");
        return NULL;
    }
    IDXGISwapChain* pSwapChain = NULL;
    DWORD pVtable = *(DWORD*)pSearchSwapChain;
    pSwapChain = (IDXGISwapChain*) findInstance(pSearchSwapChain, pVtable, testSwapChain);
    if(pSwapChain)
        WriteLog("Found Swapchain");
    else
        WriteLog("Unable to find Swapchain");

    SAFE_RELEASE(pSearchSwapChain)    
    SAFE_RELEASE(pContext)
    SAFE_RELEASE(pDevice)




    return pSwapChain;
 }


void VRDOFPlugin::InitPipeline(){
    ID3D10Blob *VS, *PS;
    ID3D10Blob *pErrors;
    std::string shader = std::string(semaphore_shader);
    
    if(!pShaderCompiler)
        return;

    WriteLog("Compiling shader");
    (pShaderCompiler)((LPCVOID) shader.c_str(), shader.length(), NULL, NULL, NULL, "VShader", "vs_4_0", 0, 0, &VS, &pErrors);
    if(pErrors)
        WriteLog(static_cast<char*>(pErrors->GetBufferPointer()));

    (pShaderCompiler)((LPCVOID) shader.c_str(), shader.length(), NULL, NULL, NULL, "PShader", "ps_4_0", 0, 0, &PS, &pErrors);
    if(pErrors)
        WriteLog(static_cast<char*>(pErrors->GetBufferPointer()));

    if(pErrors)
        return;
    
    g_d3dDevice->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &g_pVS);
    g_d3dDevice->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &g_pPS);



    WriteLog("Creating buffers");
    D3D11_BUFFER_DESC vbd;//, viewport_cbd;//, color_cbd;
    ZeroMemory(&vbd, sizeof(vbd));
    vbd.Usage = D3D11_USAGE_DYNAMIC;                // write access access by CPU and GPU
    vbd.ByteWidth = sizeof(float) * 3 * 4;          // size is the VERTEX struct * 3
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer
    g_d3dDevice->CreateBuffer(&vbd, NULL, &g_pVBuffer);       // create the buffer

	/*ZeroMemory(&viewport_cbd, sizeof(viewport_cbd));
	viewport_cbd.Usage = D3D11_USAGE_DYNAMIC;                
	viewport_cbd.ByteWidth = sizeof(cbViewport);
    viewport_cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;      
    viewport_cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_d3dDevice->CreateBuffer(&viewport_cbd, NULL, &g_pViewportCBuffer);*/

	/*ZeroMemory(&color_cbd, sizeof(color_cbd));
	color_cbd.Usage = D3D11_USAGE_DYNAMIC;                
    color_cbd.ByteWidth = sizeof(cbLights);
    color_cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;      
    color_cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_d3dDevice->CreateBuffer(&color_cbd, NULL, &g_pLightColorCBuffer);*/


    float quad_vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };


    WriteLog("Filling buffers");
    D3D11_MAPPED_SUBRESOURCE ms;
    g_context->Map(g_pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);   // map the buffer
    memcpy(ms.pData, quad_vertices, sizeof(quad_vertices));                // copy the data
    g_context->Unmap(g_pVBuffer, NULL);

    D3D11_INPUT_ELEMENT_DESC ied[] ={
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    WriteLog("Setting layout of shader");
    g_d3dDevice->CreateInputLayout(ied, 1, VS->GetBufferPointer(), VS->GetBufferSize(), &g_pLayout);


	/*g_context->Map(g_pLightColorCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
	cbLights* lightsDataPtr = (cbLights*)ms.pData;
	lightsDataPtr->color = 0.0f;
	lightsDataPtr->count = 4.0f;	
    g_context->Unmap(g_pLightColorCBuffer, NULL);*/

	WriteLog("Creating rasterizer state");
	D3D11_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FillMode = D3D11_FILL_SOLID;

	g_d3dDevice->CreateRasterizerState(&rsDesc, &g_rs);


	WriteLog("Setting up Sampler");
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory( &samplerDesc, sizeof(D3D11_SAMPLER_DESC) );
 
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.BorderColor[0] = 1.0f;
	samplerDesc.BorderColor[1] = 1.0f;
	samplerDesc.BorderColor[2] = 1.0f;
	samplerDesc.BorderColor[3] = 1.0f;
	samplerDesc.MinLOD = -FLT_MAX;
	samplerDesc.MaxLOD = FLT_MAX;
 
	HRESULT hr = g_d3dDevice->CreateSamplerState( &samplerDesc, &g_d3dSamplerState );
	if ( FAILED( hr ) ){
		WriteLog("Failed to create sampler state");
	}

	D3D11_DEPTH_STENCIL_DESC depthDesc;
	ZeroMemory(&depthDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depthDesc.DepthEnable = FALSE;
	g_d3dDevice->CreateDepthStencilState(&depthDesc, &g_DepthStencilState);
}