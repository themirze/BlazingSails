#include "render/Assets.h"

namespace Assets {
    // TODO: PNG yükleyici sonra yazılacak
    void Init(ID3D11Device*, const std::string&) {}
    void Shutdown() {}
    ID3D11ShaderResourceView* GetIcon(const std::string&) { return nullptr; }
}