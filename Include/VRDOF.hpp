/*
rF2 VRDOF
*/

#ifndef _INTERNALS_EXAMPLE_H
#define _INTERNALS_EXAMPLE_H

#include "InternalsPlugin.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>

#define PLUGIN_NAME             "rF2 VRDOF - 2019.03.07"
#define DELTA_BEST_VERSION      "v1"
#define LINUX 
//#undef LINUX

//Is this not working? Disable MSAA and use FXAA

#if _WIN64
  #define LOG_FILE              "Bin64\\Plugins\\VRDOF.log"
#else
  #define LOG_FILE              "Bin32\\Plugins\\VRDOF.log"

#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

typedef HRESULT (WINAPI *pD3DCompile)
    (LPCVOID                         pSrcData,
     SIZE_T                          SrcDataSize,
     LPCSTR                          pFileName,
     CONST D3D_SHADER_MACRO*         pDefines,
     ID3DInclude*                    pInclude,
     LPCSTR                          pEntrypoint,
     LPCSTR                          pTarget,
     UINT                            Flags1,
     UINT                            Flags2,
     ID3DBlob**                      ppCode,
     ID3DBlob**                      ppErrorMsgs);




__declspec(align(16))
struct cbViewport{
    float width;
    float height;
};

class VRDOFPlugin : public InternalsPluginV06
{

public:

    // Constructor/destructor
    VRDOFPlugin() {}
    ~VRDOFPlugin();


    void EnterRealtime();
    void ExitRealtime();

    void Load();                   // when a new track/car is loaded

    bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);

private:
    void WriteLog(const char * const msg);
   

    IDXGISwapChain* getDX11SwapChain();
    void CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd);
    void CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext);
	void CreateSearchTexture(ID3D11Device* pDevice, ID3D11Texture2D** ppTexture);
    void CreateInvisibleWindow(HWND* hwnd);



    void InitPipeline();
};
#endif // _INTERNALS_EXAMPLE_H
