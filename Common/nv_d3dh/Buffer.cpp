#include "PrecompiledHeader.h"
#include "Buffer.h"
#include <iostream>

using namespace d3d11;


/////////////////////////////////////////////////////////////////////////////////
////
void d3d11::IndexBuffer::Attach(ID3D11DeviceContext* pContext)
{
  if(pContext == 0)
    throw std::runtime_error("IndexBuffer::Attach(): zero immediate context pointer");

  uint offset = 0;
  uint stride = sizeof(uint);

  pContext->IASetIndexBuffer(m_pD3DBuffer, DXGI_FORMAT_R32_UINT, 0);
}


