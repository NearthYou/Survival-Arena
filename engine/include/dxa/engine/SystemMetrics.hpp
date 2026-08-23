#pragma once

#include <d3d11.h>

#include <cstdint>
#include <string>

namespace dxa::engine
{
[[nodiscard]] std::uint64_t GetCurrentProcessWorkingSetBytes();
[[nodiscard]] std::string GetAdapterNameUtf8(ID3D11Device* device);
} // namespace dxa::engine
