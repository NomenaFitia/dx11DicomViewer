#include "Shader.h"
#include <d3dcompiler.h>     // D3DCompileFromFile
#include <filesystem>
#include <vector>
#include <stdexcept>

using std::wstring;
namespace fs = std::filesystem;

// (Optionnel) Si tu veux linker automatiquement sans CMake :
// #pragma comment(lib, "d3dcompiler.lib")

static std::string Narrow(const wchar_t* w)
{
    if (!w) return {};
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s; s.resize(len > 0 ? size_t(len - 1) : 0);
    if (len > 1) ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len - 1, nullptr, nullptr);
    return s;
}

static wstring ResolveShaderPath(const wstring& hint)
{
    // 1) Si le chemin donné existe tel quel, on l'utilise
    if (fs::exists(hint)) return hint;

    // 2) Essaye relatif au dossier de l'exécutable
    wchar_t exePath[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path exeDir = fs::path(exePath).parent_path();

    fs::path cand = exeDir / hint;                 // e.g. <TargetDir>\shaders\mesh.hlsl
    if (fs::exists(cand)) return cand.wstring();

    // 3) Essaye relatif au CWD (au cas où)
    fs::path cwd = fs::current_path();
    cand = cwd / hint;
    if (fs::exists(cand)) return cand.wstring();

    // 4) Sinon on renvoie l'hint (D3DCompileFromFile retournera un HRESULT clair)
    return hint;
}

static ComPtr<ID3DBlob> CompileFromFile(const wstring& pathHint,
    const char* entry, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    wstring path = ResolveShaderPath(pathHint);

    ComPtr<ID3DBlob> bytecode, errors;
    HRESULT hr = D3DCompileFromFile(path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0,
        &bytecode, &errors);

    if (FAILED(hr)) {
        std::string msg;
        if (errors) {
            msg.assign((const char*)errors->GetBufferPointer(), errors->GetBufferSize());
        }
        else {
            // HRESULT format + chemin tenté
            char buf[128];
            sprintf_s(buf, "HRESULT=0x%08X", (unsigned)hr);
            msg = std::string(buf) + " | file=" + Narrow(path.c_str());
        }
        OutputDebugStringA(("[HLSL] compile failed: " + msg + "\n").c_str());
        throw std::runtime_error("Shader compile failed (" + std::string(entry) + "," + target + "): " + msg);
    }

    return bytecode;
}

Shader::VS Shader::LoadVS(ID3D11Device* dev, const wstring& path,
    const D3D11_INPUT_ELEMENT_DESC* layout, UINT count,
    const char* entry)
{
    if (!dev) throw std::invalid_argument("LoadVS: dev == nullptr");
    if (!layout || count == 0) throw std::invalid_argument("LoadVS: invalid input layout");

    auto bc = CompileFromFile(path, entry, "vs_5_0");

    Shader::VS vs{};
    HR_CHECK(dev->CreateVertexShader(bc->GetBufferPointer(), bc->GetBufferSize(),
        nullptr, &vs.shader));
    HR_CHECK(dev->CreateInputLayout(layout, count,
        bc->GetBufferPointer(), bc->GetBufferSize(),
        &vs.layout));

    DxUtil::SetDebugName(vs.shader.Get(), "VS");
    DxUtil::SetDebugName(vs.layout.Get(), "InputLayout");
    return vs;
}

Shader::PS Shader::LoadPS(ID3D11Device* dev, const wstring& path, const char* entry)
{
    if (!dev) throw std::invalid_argument("LoadPS: dev == nullptr");

    auto bc = CompileFromFile(path, entry, "ps_5_0");

    Shader::PS ps{};
    HR_CHECK(dev->CreatePixelShader(bc->GetBufferPointer(), bc->GetBufferSize(),
        nullptr, &ps.shader));

    DxUtil::SetDebugName(ps.shader.Get(), "PS");
    return ps;
}
