#pragma once
#include <d3d11.h>
#include <string>

namespace Assets {
    void Init(ID3D11Device* device, const std::string& folder);
    void Shutdown();
    ID3D11ShaderResourceView* GetIcon(const std::string& name);
}