#pragma once
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <string_view>

namespace workshop_window {
struct Box { int left, top, right, bottom; };
inline bool RecompiledImage(std::wstring_view name) {
    for (const auto expected : {std::wstring_view(L"legodimensions.exe"), std::wstring_view(L"lego_dimensions.exe")}) {
        if (name.size() != expected.size()) continue;
        bool same = true;
        for (size_t i = 0; i < name.size(); ++i) same = same && std::towlower(name[i]) == expected[i];
        if (same) return true;
    }
    return false;
}
inline Box Fit(Box bounds, int requestedWidth, int requestedHeight, const Box* saved = nullptr) {
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0 || requestedWidth <= 0 || requestedHeight <= 0)
        throw std::invalid_argument("Overlay bounds must have positive dimensions");
    const double scale = std::min({1.0, double(width) / requestedWidth, double(height) / requestedHeight});
    const int w = std::max(1, int(std::floor(requestedWidth * scale)));
    const int h = std::max(1, int(std::floor(requestedHeight * scale)));
    const int x = saved ? std::clamp(saved->left, bounds.left, bounds.right - w) : bounds.left + (width - w) / 2;
    const int y = saved ? std::clamp(saved->top, bounds.top, bounds.bottom - h) : bounds.top + (height - h) / 2;
    return {x, y, x + w, y + h};
}
}
