#include "PrecompiledHeader.h"
#include "Pipeline.h"

///////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Pipeline::Pipeline(ID3D11Device* a_pDevice, ID3DX11Effect* a_pEffect)
{
    m_apOldRTVs[0] = 0;
    m_pOldDS = 0;
    m_pDepthTex = 0;
    m_pTexRTV = 0;
    m_pTexDSV = 0;
    m_pTex3DRTV = 0;

    m_clearColor[0] = 0;
    m_clearColor[1] = 0;
    m_clearColor[2] = 0;
    m_clearColor[3] = 0;

    m_pDevice = a_pDevice;
    m_pEffect = a_pEffect;
    //m_pEffect->AddRef(); // new owner should increment reference counter
    m_pContext = GetCtx(a_pDevice);

    m_pFullScreenQuad = new FullScreenQuad(m_pDevice, m_pEffect->GetTechniqueByName("DrawTexturedQuad"));

    D3D11_QUERY_DESC qryDesc;
	qryDesc.Query = D3D11_QUERY_EVENT;
	qryDesc.MiscFlags = 0;
	m_pDevice->CreateQuery(&qryDesc, &m_pQuery);	
}


///////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Pipeline::Pipeline(ID3D11Device* a_pDevice, ID3DX11Effect* a_pEffect, FullScreenQuad* a_quad)
{
    m_apOldRTVs[0] = 0;
    m_pOldDS = 0;
    m_pDepthTex = 0;
    m_pTexRTV = 0;
    m_pTexDSV = 0;
    m_pTex3DRTV = 0;

    m_clearColor[0] = 0;
    m_clearColor[1] = 0;
    m_clearColor[2] = 0;
    m_clearColor[3] = 0;

    m_pDevice = a_pDevice;
    m_pEffect = a_pEffect;
    //m_pEffect->AddRef(); // new owner should increment reference counter
    m_pContext = GetCtx(a_pDevice);

    m_pFullScreenQuad = a_quad;

    D3D11_QUERY_DESC qryDesc;
	qryDesc.Query = D3D11_QUERY_EVENT;
	qryDesc.MiscFlags = 0;
	m_pDevice->CreateQuery(&qryDesc, &m_pQuery);	
}

///////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Pipeline::Pipeline(ID3D11Device* a_pDevice, const wchar_t* a_fxFileName)
{
    m_apOldRTVs[0] = 0;
    m_pOldDS = 0;
    m_pDepthTex = 0;
    m_pTexRTV = 0;
    m_pTexDSV = 0;
    m_pTex3DRTV = 0;
    m_pFullScreenQuad = 0;
    m_pQuery = 0;
    RUN_TIME_ERROR("not implemented");
}

///////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Pipeline::~Pipeline()
{
    SAFE_RELEASE(m_pQuery);
    SAFE_RELEASE(m_pContext);
    SAFE_RELEASE(m_pDepthTex);
    SAFE_RELEASE(m_pTexRTV);
    SAFE_RELEASE(m_pTexDSV);
    SAFE_RELEASE(m_pTex3DRTV);

    delete m_pFullScreenQuad;

    FreeCaches();
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindSRV_F(std::string a_str, ID3D11Texture2D* a_pTex, const char* file, int line)
{
    if(a_pTex==0)
        RUN_TIME_ERROR("Bind(texture2D): zero pointer to texture");

    D3D11_TEXTURE2D_DESC desc;
    a_pTex->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
    {
        std::string err_msg = std::string("Bind(texture2D): no texture with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }

    if(!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
        RUN_TIME_ERROR_AT("Bind(texture2D): given resource can not be bind as Shader Resource cause it is not a shader resource", file, line);

    ID3DX11EffectShaderResourceVariable* pResource = pVar->AsShaderResource();
    if(!pResource->IsValid())
    {
        std::string err_msg = std::string("Bind(texture2D): invalid resource with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = desc.MipLevels;

    // this is a very nice technique - if you already create SRV, just save it in cache.
    //
    if(m_SRVCache.find(a_pTex) == m_SRVCache.end())
    {
        ID3D11ShaderResourceView* pSRV = 0;
        DX_SAFE_CALL(m_pDevice->CreateShaderResourceView(a_pTex, &SRVDesc, &pSRV));
        DX_SAFE_CALL(pResource->SetResource(pSRV));
        m_SRVCache[a_pTex] = pSRV;
    }
    else
        DX_SAFE_CALL(pResource->SetResource(m_SRVCache[a_pTex]));
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindSRV_F(std::string a_str, ID3D11Texture3D* a_pTex, const char* file, int line)
{
    if(a_pTex==0)
        RUN_TIME_ERROR("BindSRV(texture2D): zero pointer to texture");

    D3D11_TEXTURE3D_DESC desc;
    a_pTex->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
    {
        std::string err_msg = std::string("BindSRV(texture3D): no texture with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }

    if(!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
        RUN_TIME_ERROR_AT("BindSRV(texture3D): given resource can not be bind as Shader Resource cause it have no appropriate flags", file, line);

    ID3DX11EffectShaderResourceVariable* pResource = pVar->AsShaderResource();
    if(!pResource->IsValid())
    {
        std::string err_msg = std::string("Bind(texture2D): invalid resource with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    SRVDesc.Texture3D.MipLevels = desc.MipLevels;
    SRVDesc.Texture3D.MostDetailedMip = 0;

    // this is a very nice technique - if you already create SRV, just save it in cache.
    //
    if(m_SRVCache.find(a_pTex) == m_SRVCache.end())
    {
        ID3D11ShaderResourceView* pSRV = 0;
        DX_SAFE_CALL(m_pDevice->CreateShaderResourceView(a_pTex, &SRVDesc, &pSRV));
        DX_SAFE_CALL(pResource->SetResource(pSRV));
        m_SRVCache[a_pTex] = pSRV;
    }
    else
        DX_SAFE_CALL(pResource->SetResource(m_SRVCache[a_pTex]));
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindSRV_F(std::string a_str, ID3D11Buffer* a_pBuff, const char* file, int line)
{
    if(a_pBuff==0)
        RUN_TIME_ERROR("BindSRV(Buffer): zero pointer to buffer");

    D3D11_BUFFER_DESC desc;
    a_pBuff->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindSRV(Buffer): no buffer with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
        RUN_TIME_ERROR_AT("BindSRV(Buffer): given resource can not be bind as Shader Resource cause it is not a shader resource", file, line);

    ID3DX11EffectShaderResourceVariable* pResource = pVar->AsShaderResource();
    if(!pResource->IsValid())
    {
        std::string err_msg = std::string("BindSRV(Buffer): invalid resource with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }


    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

    if(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.NumElements = desc.ByteWidth/desc.StructureByteStride;
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
    }
    else
    {
        RUN_TIME_ERROR("USE STRUCTURED_BUFFER or IBuffer to bind");
        //SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        //SRVDesc.Buffer.NumElements = desc.ByteWidth/sizeof(Elem);
        //SRVDesc.Format = DXGIFormat<Elem>();
    }

    if(m_SRVCache.find(a_pBuff) == m_SRVCache.end())
    {
        ID3D11ShaderResourceView* pSRV = 0;
        DX_SAFE_CALL(m_pDevice->CreateShaderResourceView(a_pBuff, &SRVDesc, &pSRV));
        DX_SAFE_CALL(pResource->SetResource(pSRV));
        m_SRVCache[a_pBuff] = pSRV;
    }
    else
        DX_SAFE_CALL(pResource->SetResource(m_SRVCache[a_pBuff]));
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindSRV_F(std::string a_str, IBuffer* a_buff, const char* file, int line)
{
    ID3D11Buffer* a_pBuff = a_buff->GetD3DBuffer();

    if(a_pBuff==0)
        RUN_TIME_ERROR("BindSRV(Buffer): zero pointer to texture");

    D3D11_BUFFER_DESC desc;
    a_pBuff->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindSRV(Buffer): no texture with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
        RUN_TIME_ERROR_AT("BindSRV(Buffer): given resource can not be bind as Shader Resource cause it is not a shader resource", file, line);

    ID3DX11EffectShaderResourceVariable* pResource = pVar->AsShaderResource();
    if(!pResource->IsValid())
    {
        std::string err_msg = std::string("BindSRV(Buffer): invalid resource with name ") + a_str;
        RUN_TIME_ERROR_AT(err_msg.c_str(), file, line);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

    if(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.NumElements = desc.ByteWidth/desc.StructureByteStride;
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN; //a_buff->GetDXGIFormat();
    }
    else
    {
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.NumElements = a_buff->Size();
        SRVDesc.Format = a_buff->GetDXGIFormat();
    }

    if(m_SRVCache.find(a_pBuff) == m_SRVCache.end())
    {
        ID3D11ShaderResourceView* pSRV = 0;
        DX_SAFE_CALL(m_pDevice->CreateShaderResourceView(a_pBuff, &SRVDesc, &pSRV));
        DX_SAFE_CALL(pResource->SetResource(pSRV));
        m_SRVCache[a_pBuff] = pSRV;
    }
    else
        DX_SAFE_CALL(pResource->SetResource(m_SRVCache[a_pBuff]));
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindUAV_F(std::string a_str, ID3D11Texture3D* a_pTex, const char* file, int line)
{
    if(a_pTex==0)
        RUN_TIME_ERROR_AT("BindUAV(texture3D): zero pointer to texture", file, line);

    D3D11_TEXTURE3D_DESC desc;
    a_pTex->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture2D): no texture with name ") + a_str).c_str(), file, line);

    ID3DX11EffectUnorderedAccessViewVariable* pUAV_Var = pVar->AsUnorderedAccessView();
    if(!pUAV_Var->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture3D): invalid UAV with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS))
        RUN_TIME_ERROR_AT("BindUAV(texture3D): given textures has D3D11_BIND_UNORDERED_ACCESS flag as false", file, line);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
    UAVDesc.Format = desc.Format;
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
    UAVDesc.Texture3D.FirstWSlice = 0;
    UAVDesc.Texture3D.MipSlice = 0;
    UAVDesc.Texture3D.WSize = desc.Depth;


    if(m_UAVCache.find(a_pTex) == m_UAVCache.end())
    {
        ID3D11UnorderedAccessView* pUAV = 0;
        DX_SAFE_CALL(m_pDevice->CreateUnorderedAccessView(a_pTex, &UAVDesc, &pUAV));
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(pUAV));
        m_UAVCache[a_pTex] = pUAV;
    }
    else
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(m_UAVCache[a_pTex]));
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindUAV_F(std::string a_str, ID3D11Texture2D* a_pTex, const char* file, int line)
{
    if(a_pTex==0)
        RUN_TIME_ERROR_AT("BindUAV(texture2D): zero pointer to texture", file, line);

    D3D11_TEXTURE2D_DESC desc;
    a_pTex->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture2D): no texture with name ") + a_str).c_str(), file, line);

    ID3DX11EffectUnorderedAccessViewVariable* pUAV_Var = pVar->AsUnorderedAccessView();
    if(!pUAV_Var->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture2D): invalid UAV with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS))
        RUN_TIME_ERROR_AT("BindUAV(texture2D): given textures has D3D11_BIND_UNORDERED_ACCESS flag as false", file, line);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
    UAVDesc.Format = desc.Format;
    UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

    if(m_UAVCache.find(a_pTex) == m_UAVCache.end())
    {
        ID3D11UnorderedAccessView* pUAV = 0;
        DX_SAFE_CALL(m_pDevice->CreateUnorderedAccessView(a_pTex, &UAVDesc, &pUAV));
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(pUAV));
        m_UAVCache[a_pTex] = pUAV;
    }
    else
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(m_UAVCache[a_pTex]));
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindUAV_F(std::string a_str, ID3D11Buffer* a_pBuff, const char* file, int line)
{
    if(a_pBuff==0)
        RUN_TIME_ERROR("BindUAV(ID3D11Buffer): zero pointer to texture");

    D3D11_BUFFER_DESC desc;
    a_pBuff->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(ID3D11Buffer): no texture with name ") + a_str).c_str(), file, line);

    ID3DX11EffectUnorderedAccessViewVariable* pUAV_Var = pVar->AsUnorderedAccessView();
    if(!pUAV_Var->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(ID3D11Buffer): invalid RT with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS))
        RUN_TIME_ERROR_AT("BindUAV(ID3D11Buffer): given textures has D3D11_BIND_UNORDERED_ACCESS flag as false", file, line);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));  

    if(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        ASSERT(desc.StructureByteStride>0);

        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = desc.ByteWidth/desc.StructureByteStride;
        UAVDesc.Buffer.Flags = 0;
    }
    else
    {
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        RUN_TIME_ERROR_AT("Use D3D11_RESOURCE_MISC_BUFFER_STRUCTURED or IBuffer ", file, line);
        //UAVDesc.Buffer.NumElements = desc.ByteWidth/16 ;
        //UAVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }

    if(m_UAVCache.find(a_pBuff) == m_UAVCache.end())
    {
        ID3D11UnorderedAccessView* pUAV = 0;
        DX_SAFE_CALL(m_pDevice->CreateUnorderedAccessView(a_pBuff, &UAVDesc, &pUAV));
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(pUAV));
        m_UAVCache[a_pBuff] = pUAV;
    }
    else
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(m_UAVCache[a_pBuff]));
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindUAVAppend_F(std::string a_str, ID3D11Buffer* a_pBuff, const char* file, int line)
{
    if(a_pBuff==0)
        RUN_TIME_ERROR("BindUAVAppend(ID3D11Buffer): zero pointer to texture");

    D3D11_BUFFER_DESC desc;
    a_pBuff->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAVAppend(ID3D11Buffer): no texture with name ") + a_str).c_str(), file, line);

    ID3DX11EffectUnorderedAccessViewVariable* pUAV_Var = pVar->AsUnorderedAccessView();
    if(!pUAV_Var->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAVAppend(ID3D11Buffer): invalid RT with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS))
        RUN_TIME_ERROR_AT("BindUAVAppend(ID3D11Buffer): given textures has D3D11_BIND_UNORDERED_ACCESS flag as false", file, line);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));  

    if(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        ASSERT(desc.StructureByteStride>0);

        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = desc.ByteWidth/desc.StructureByteStride;
        UAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND; ////////////////////////// !!!!!!!!!!!!!!!!!!!!!!!! //
    }
    else
    {
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        RUN_TIME_ERROR_AT("Use D3D11_RESOURCE_MISC_BUFFER_STRUCTURED or IBuffer ", file, line);
        //UAVDesc.Buffer.NumElements = desc.ByteWidth/16 ;
        //UAVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }

    if(m_UAVCache.find(a_pBuff) == m_UAVCache.end())
    {
        ID3D11UnorderedAccessView* pUAV = 0;
        DX_SAFE_CALL(m_pDevice->CreateUnorderedAccessView(a_pBuff, &UAVDesc, &pUAV));
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(pUAV));
        m_UAVCache[a_pBuff] = pUAV;
    }
    else
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(m_UAVCache[a_pBuff]));
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BindUAV_F(std::string a_str, IBuffer* a_buff, const char* file, int line)
{
    ID3D11Buffer* a_pBuff = a_buff->GetD3DBuffer();

    if(a_pBuff==0)
        RUN_TIME_ERROR("BindUAV(texture2D): zero pointer to texture");

    D3D11_BUFFER_DESC desc;
    a_pBuff->GetDesc(&desc);

    ID3DX11EffectVariable* pVar = m_pEffect->GetVariableByName(a_str.c_str());
    if(!pVar->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture2D): no texture with name ") + a_str).c_str(), file, line);

    ID3DX11EffectUnorderedAccessViewVariable* pUAV_Var = pVar->AsUnorderedAccessView();
    if(!pUAV_Var->IsValid())
        RUN_TIME_ERROR_AT((std::string("BindUAV(texture2D): invalid RT with name ") + a_str).c_str(), file, line);

    if(!(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS))
        RUN_TIME_ERROR_AT("BindUAV(texture2D): given textures has D3D11_BIND_UNORDERED_ACCESS flag as false", file, line);

    D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
    ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));  

    if(desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        ASSERT(desc.StructureByteStride>0);

        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = desc.ByteWidth/desc.StructureByteStride;
        UAVDesc.Buffer.Flags = 0;
    }
    else
    {
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = a_buff->Size();
        UAVDesc.Format = a_buff->GetDXGIFormat();
    }

    if(m_UAVCache.find(a_pBuff) == m_UAVCache.end())
    {
        ID3D11UnorderedAccessView* pUAV = 0;
        DX_SAFE_CALL(m_pDevice->CreateUnorderedAccessView(a_pBuff, &UAVDesc, &pUAV));
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(pUAV));
        m_UAVCache[a_pBuff] = pUAV;
    }
    else
        DX_SAFE_CALL(pUAV_Var->SetUnorderedAccessView(m_UAVCache[a_pBuff]));
}


void d3d11::Pipeline::ApplyTechnique_F(const char* name, int pass, const char* file, int line)
{
    ID3DX11EffectTechnique* pTechnique = m_pEffect->GetTechniqueByName(name);
    if(pTechnique==0 || !pTechnique->IsValid())
        RUN_TIME_ERROR_AT((std::string("invalid technique ") + name).c_str(), file, line);
    
    pTechnique->GetPassByIndex(pass)->Apply(0, m_pContext);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetDepthStencilState(int flag)
{
    if(flag == DEFAULT)
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        desc.DepthEnable = true;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_LESS;
        desc.StencilEnable = false;
        desc.StencilReadMask = D3D11_STENCIL_OP_KEEP;
        desc.StencilWriteMask = D3D11_STENCIL_OP_KEEP;
  
        ID3D11DepthStencilState* pDepthStensilState = 0;
        DX_SAFE_CALL(m_pDevice->CreateDepthStencilState(&desc,&pDepthStensilState));
    
        m_pContext->OMSetDepthStencilState(pDepthStensilState,0);
 
        SAFE_RELEASE(pDepthStensilState);
    }
    else if(flag == DISABLE_DEPTH_TEST)
    {
        D3D11_DEPTH_STENCIL_DESC desc;
        desc.DepthEnable = false;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_NEVER;
        desc.StencilEnable = false;
        desc.StencilReadMask = D3D11_STENCIL_OP_KEEP;
        desc.StencilWriteMask = D3D11_STENCIL_OP_KEEP;
  
        ID3D11DepthStencilState* pDepthStensilState = 0;
        DX_SAFE_CALL(m_pDevice->CreateDepthStencilState(&desc,&pDepthStensilState));
    
        m_pContext->OMSetDepthStencilState(pDepthStensilState,0);
    
        SAFE_RELEASE(pDepthStensilState);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::CreateRenderToTextureResources(ID3D11Texture2D* a_pTex, const char* file, int line)
{
    // first - release all resources
    //
    SAFE_RELEASE(m_pDepthTex);
    //SAFE_RELEASE(m_pTexRTV);
    SAFE_RELEASE(m_pTexDSV);

    // get the description of our texture
    //
    D3D11_TEXTURE2D_DESC texDesc;
    a_pTex->GetDesc(&texDesc);

    m_oldTexDesc = texDesc; // save old texture description

    // create fucking render terget view
    //    
    /*D3D11_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.Format = texDesc.Format;
    DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    DescRT.Texture2DArray.FirstArraySlice = 0;
    DescRT.Texture2DArray.ArraySize = 1;
    DescRT.Texture2DArray.MipSlice = 0;

    DX_SAFE_CALL(m_pDevice->CreateRenderTargetView(a_pTex, &DescRT, &m_pTexRTV));
*/

    // create depth stencil texture.
    //
    D3D11_TEXTURE2D_DESC dstex;
    dstex.Width = texDesc.Width;
    dstex.Height = texDesc.Height;
    dstex.MipLevels = 1;
    dstex.ArraySize = 1;
    dstex.SampleDesc.Count = 1;
    dstex.SampleDesc.Quality = 0;
    dstex.Format = DXGI_FORMAT_D32_FLOAT; //DXGI_FORMAT_D24_UNORM_S8_UINT;
    dstex.Usage = D3D11_USAGE_DEFAULT;
    dstex.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
   
    DX_SAFE_CALL(m_pDevice->CreateTexture2D(&dstex, NULL, &m_pDepthTex));

    // Create fucking depth-stencil view
    //
    
    D3D11_DEPTH_STENCIL_VIEW_DESC DescDS;
    ZeroMemory(&DescDS, sizeof(D3D11_DEPTH_STENCIL_VIEW_DESC));
    DescDS.Format = DXGI_FORMAT_D32_FLOAT; //DXGI_FORMAT_D24_UNORM_S8_UINT;
    DescDS.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    DescDS.Texture2DArray.FirstArraySlice = 0;
    DescDS.Texture2DArray.ArraySize = 1;
    DescDS.Texture2DArray.MipSlice = 0;

    DX_SAFE_CALL(m_pDevice->CreateDepthStencilView(m_pDepthTex, &DescDS, &m_pTexDSV));
   
}

///////////////////////////////////////////////////////////////////////////////////////////
////
bool d3d11::Pipeline::SameTextureDescription(const D3D11_TEXTURE2D_DESC a, const D3D11_TEXTURE2D_DESC& b)
{
    return (a.ArraySize == b.ArraySize) &&
           (a.BindFlags == b.BindFlags) &&
           (a.CPUAccessFlags == b.CPUAccessFlags) &&
           (a.Format == b.Format) &&
           (a.Height == b.Height) &&
           (a.MipLevels == b.MipLevels) &&
           (a.MiscFlags == b.MiscFlags) &&
           (a.Usage == b.Usage) &&
           (a.Width == b.Width);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BeginRenderingToTexture(ID3D11Texture2D* a_pTex, int X, int Y, int Z)
{
    // Save the old RT and DS buffer views
    //
    m_pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_apOldRTVs, &m_pOldDS);

    // Save the old viewport
    //
    UINT cRT = 1;
    m_pContext->RSGetViewports(&cRT, &m_OldVP);

    if (a_pTex == NULL)
    {

        D3D11_VIEWPORT SMVP;
        SMVP.Height = (FLOAT)Y;
        SMVP.Width = (FLOAT)X;
        SMVP.MinDepth = 0;
        SMVP.MaxDepth = 1;
        SMVP.TopLeftX = 0;
        SMVP.TopLeftY = 0;
        m_pContext->RSSetViewports(1, &SMVP);

        m_pContext->OMSetRenderTargets(0, NULL, NULL);
        return;
    }

    // get the description of our texture
    //
    D3D11_TEXTURE2D_DESC texDesc;
    a_pTex->GetDesc(&texDesc);

		if(!(texDesc.BindFlags & D3D11_BIND_RENDER_TARGET))
			RUN_TIME_ERROR("Rendering to texture: texture must have D3D11_BIND_RENDER_TARGET flag");

    // Set a new viewport for rendering to cube map
    //
    D3D11_VIEWPORT SMVP;
    SMVP.Height = (FLOAT)texDesc.Height;
    SMVP.Width = (FLOAT)texDesc.Width;
    SMVP.MinDepth = 0;
    SMVP.MaxDepth = 1;
    SMVP.TopLeftX = 0;
    SMVP.TopLeftY = 0;
    m_pContext->RSSetViewports(1, &SMVP);

    //ASSERT(m_pTexRTV == 0);

    // try to cache all RTV's
    //
    RTVCache::iterator p;
    if(m_RTVCache.find(a_pTex) == m_RTVCache.end())
    {
        // this texture is not in cache - create RTV for it
        //
        D3D11_RENDER_TARGET_VIEW_DESC DescRT;
        DescRT.Format = texDesc.Format;
        DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        DescRT.Texture2DArray.FirstArraySlice = 0;
        DescRT.Texture2DArray.ArraySize = 1;
        DescRT.Texture2DArray.MipSlice = 0;

        DX_SAFE_CALL(m_pDevice->CreateRenderTargetView(a_pTex, &DescRT, &m_pTexRTV));

        m_RTVCache[a_pTex] = m_pTexRTV;
    }
    else
        m_pTexRTV = m_RTVCache[a_pTex]; // ok, we already have RTV in cache

    // cache only the last texture resources
    //
    if(!SameTextureDescription(m_oldTexDesc,texDesc))
        CreateRenderToTextureResources(a_pTex,"",0);

    // cleare the texture where we are to render
    //
    m_pContext->ClearRenderTargetView(m_pTexRTV, m_clearColor);
    m_pContext->ClearDepthStencilView(m_pTexDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

    // Set Render Targets and Depth Stencil views.
    //
    ID3D11RenderTargetView* aRTViews[1] = { m_pTexRTV };
    m_pContext->OMSetRenderTargets(sizeof(aRTViews)/sizeof(aRTViews[0]), aRTViews, m_pTexDSV);

}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::EndRenderingToTexture()
{
    // Restore old view port, RT and DS buffer views
    //
    m_pContext->RSSetViewports(1, &m_OldVP);
    m_pContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_apOldRTVs, m_pOldDS);

    // release fucking directX reference counters
    //
		for(int i=0;i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;i++)
			SAFE_RELEASE(m_apOldRTVs[i]);
    SAFE_RELEASE(m_pOldDS);    
   
    // Generate Mip Maps
    //
    //m_pContext->GenerateMips(m_pTexSRV);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::CreateRenderToTextureResources(ID3D11Texture3D* a_pTex)
{
    
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::BeginRenderingTo3DTexture(ID3D11Texture3D* a_pTex, int X, int Y, int Z)
{
    // Save the old RT and DS buffer views
    //
		m_pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_apOldRTVs, &m_pOldDS);

    // Save the old viewport
    //
    UINT cRT = 1;
    m_pContext->RSGetViewports(&cRT, &m_OldVP);

    if (a_pTex == NULL)
    {

        D3D11_VIEWPORT SMVP;
        SMVP.Height = (FLOAT)Y;
        SMVP.Width = (FLOAT)X;
        SMVP.MinDepth = 0;
        SMVP.MaxDepth = 1;
        SMVP.TopLeftX = 0;
        SMVP.TopLeftY = 0;
        m_pContext->RSSetViewports(1, &SMVP);

        m_pContext->OMSetRenderTargets(0, NULL, NULL);
        return;
    }

    // get the description of our texture
    //
    D3D11_TEXTURE3D_DESC texDesc;
    a_pTex->GetDesc(&texDesc);

    // Set a new viewport for rendering to cube map
    //
    D3D11_VIEWPORT SMVP;
    SMVP.Height = (FLOAT)texDesc.Height;
    SMVP.Width = (FLOAT)texDesc.Width;
    SMVP.MinDepth = 0;
    SMVP.MaxDepth = 1;
    SMVP.TopLeftX = 0;
    SMVP.TopLeftY = 0;
    m_pContext->RSSetViewports(1, &SMVP);

    //ASSERT(m_pTexRTV == 0);

    // try to cache all RTV's
    //
    RTVCache::iterator p;
    if(m_RTVCache.find(a_pTex) == m_RTVCache.end())
    {
        // this texture is not in cache - create RTV for it
        //
        D3D11_RENDER_TARGET_VIEW_DESC DescRT;
        DescRT.Format = texDesc.Format;
        DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        DescRT.Texture3D.FirstWSlice = 0;
        DescRT.Texture3D.MipSlice = 0;
        DescRT.Texture3D.WSize = texDesc.Depth;

        DX_SAFE_CALL(m_pDevice->CreateRenderTargetView(a_pTex, &DescRT, &m_pTex3DRTV));

        m_RTVCache[a_pTex] = m_pTex3DRTV;
    }
    else
        m_pTex3DRTV = m_RTVCache[a_pTex]; // ok, we already have RTV in cache

    // cleare the texture where we are to render
    //
    float clearColor[4] = {0,0,0,0};
    m_pContext->ClearRenderTargetView(m_pTex3DRTV, clearColor);

    // Set Render Targets
    //
    ID3D11RenderTargetView* aRTViews[1] = { m_pTex3DRTV };
    m_pContext->OMSetRenderTargets(1, aRTViews, NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::EndRenderingTo3DTexture()
{
    // Restore old view port, RT and DS buffer views
    //
    m_pContext->RSSetViewports(1, &m_OldVP);
    m_pContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, m_apOldRTVs, m_pOldDS);

    // release fucking directX reference counters
    //
		for(int i=0;i<D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;i++)
			SAFE_RELEASE(m_apOldRTVs[i]);
    SAFE_RELEASE(m_pOldDS);    
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::RunOldSchoolKernel(const char* name, ID3D11Texture2D* output, 
                                         int X, int Y, int Z)
{
    ID3DX11EffectTechnique* pTechnique = m_pEffect->GetTechniqueByName(name);
    
    if(pTechnique==0 || !pTechnique->IsValid())
        RUN_TIME_ERROR((std::string("invalid technique:") + std::string(name)).c_str());

    pTechnique->GetPassByIndex(0)->Apply(0, m_pContext);

    this->BeginRenderingToTexture(output,X,Y,Z);
      
    // do no Draw() quad because it apply DrawTexturedQuad technique.
    // use DrawCall() instead of this just to draw quad with custom pixel shader
    //
    m_pFullScreenQuad->DrawCall();

    this->EndRenderingToTexture();
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::RunKernel(const char* name, int X, int Y, int Z)
{
    ID3DX11EffectTechnique* pTechnique = m_pEffect->GetTechniqueByName(name);
    if(pTechnique==0 || !pTechnique->IsValid())
        RUN_TIME_ERROR((std::string("invalid technique:") + std::string(name)).c_str());
    
    pTechnique->GetPassByIndex(0)->Apply(0, m_pContext);

    m_pContext->Dispatch(X,Y,Z);
}   


void d3d11::Pipeline::FreeCaches()
{
    RTVCache::iterator p1;
    for(p1=m_RTVCache.begin();p1!=m_RTVCache.end();++p1)
        SAFE_RELEASE(p1->second);
    m_RTVCache.clear();

    SRVCache::iterator p2;
    for(p2=m_SRVCache.begin();p2!=m_SRVCache.end();++p2)
        SAFE_RELEASE(p2->second);
    m_SRVCache.clear();

    UAVCache::iterator p3;
    for(p3=m_UAVCache.begin();p3!=m_UAVCache.end();++p3)
        SAFE_RELEASE(p3->second);
    m_UAVCache.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::UnbindAllUAV()
{
    ID3D11UnorderedAccessView* uavs[] = { NULL, NULL, NULL, NULL,
                                          NULL, NULL, NULL, NULL };

    m_pContext->CSSetUnorderedAccessViews(0,8,uavs,0);

}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::UnbindAllSRV()
{
    ID3D11ShaderResourceView* vs[] = { NULL, NULL, NULL, NULL,
                                       NULL, NULL, NULL, NULL };

    m_pContext->CSSetShaderResources(0,8,vs);
    m_pContext->PSSetShaderResources(0,8,vs);
    m_pContext->VSSetShaderResources(0,8,vs);
    m_pContext->HSSetShaderResources(0,8,vs);
    m_pContext->DSSetShaderResources(0,8,vs);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::CopyBufferToTexture2D(ID3D11Texture2D* a_pTex, ID3D11Buffer* a_pBuff)
{
    this->BindSRV("in_buffer", a_pBuff);
    this->RunOldSchoolKernel("BufferToTexture_kernel", a_pTex);   
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::CopyTexture2DToBuffer(ID3D11Buffer* a_pBuff, ID3D11Texture2D* a_pTex)
{
    D3D11_TEXTURE2D_DESC texDesc;
    a_pTex->GetDesc(&texDesc);
    
    int w = texDesc.Width;
    int h = texDesc.Height;

    this->BindUAV("out_buffer", a_pBuff);
    this->BindSRV("in_texture", a_pTex);

    this->ApplyTechnique("TextureToBuffer",0);

    m_pContext->Dispatch(w/16, h/16, 1);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, float val, const char* file, int line)
{
    ID3DX11EffectScalarVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsScalar();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloat(val);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, int val, const char* file, int line)
{
    ID3DX11EffectScalarVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsScalar();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetInt(val);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const D3DXVECTOR2& val, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatVector((float*)&val);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const D3DXVECTOR3& val, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatVector((float*)&val);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const D3DXVECTOR4& val, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatVector((float*)&val);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const int* vals, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetIntVector((int*)vals);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const float* vals, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatVector((float*)vals);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariable_F(std::string a_name, const D3DXMATRIX& val, const char* file, int line)
{
    ID3DX11EffectMatrixVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsMatrix();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetMatrix((float*)&val);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariableScalarArray_F(std::string a_name, const float* arr, int size, const char* file, int line)
{
    ID3DX11EffectScalarVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsScalar();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatArray((float*)arr, 0, size);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::SetShaderVariableVectorArray_F(std::string a_name, const float* arr, int size, const char* file, int line)
{
    ID3DX11EffectVectorVariable* v = m_pEffect->GetVariableByName(a_name.c_str())->AsVector();

    if(!v->IsValid())
        RUN_TIME_ERROR_AT((std::string("undefined shader variable ") + a_name).c_str(), file, line);

    v->SetFloatVectorArray((float*)arr, 0, size);
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::ReleaseRTV(ID3D11Resource* res)
{
    RTVCache::iterator p1 = m_RTVCache.find(res);
    if(p1!=m_RTVCache.end())
    {
        SAFE_RELEASE(p1->second);
        m_RTVCache.erase(p1);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::ReleaseSRV(ID3D11Resource* res)
{
    SRVCache::iterator p1 = m_SRVCache.find(res);
    if(p1!=m_SRVCache.end())
    {
        SAFE_RELEASE(p1->second);
        m_SRVCache.erase(p1);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::ReleaseUAV(ID3D11Resource* res)
{
    UAVCache::iterator p1 = m_UAVCache.find(res);
    if(p1!=m_UAVCache.end())
    {
        SAFE_RELEASE(p1->second);
        m_UAVCache.erase(p1);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Pipeline::ReleaseAllViews(ID3D11Resource* res)
{
    ReleaseRTV(res);
    ReleaseSRV(res);
    ReleaseUAV(res);
}

///////////////////////////////////////////////////////////////////////////////////////////
////
ID3D11ShaderResourceView*  d3d11::Pipeline::GetSRV(ID3D11Resource* key)
{
	SRVCache::iterator p1 = m_SRVCache.find(key);
  if(p1!=m_SRVCache.end())
		return p1->second;
	else
		RUN_TIME_ERROR("Pipeline::GetSRV: no SRV for this resource");
	
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////
////
ID3D11RenderTargetView*    d3d11::Pipeline::GetRTV(ID3D11Resource* key)
{
	RTVCache::iterator p1 = m_RTVCache.find(key);
  if(p1!=m_RTVCache.end())
		return p1->second;
	else
		RUN_TIME_ERROR("Pipeline::GetSRT: no RTV for this resource");
	
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////
////
ID3D11UnorderedAccessView* d3d11::Pipeline::GetUAV(ID3D11Resource* key)
{
	UAVCache::iterator p1 = m_UAVCache.find(key);
  if(p1!=m_UAVCache.end())
		return p1->second;
	else
		RUN_TIME_ERROR("Pipeline::GetUAV: no UAV for this resource");

	return NULL;
}



