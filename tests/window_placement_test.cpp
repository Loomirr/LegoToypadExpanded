#include "../WindowPlacement.hpp"
#include <cassert>
int main() {
    using namespace workshop_window;
    assert(RecompiledImage(L"lego_dimensions.exe"));
    assert(RecompiledImage(L"LEGODIMENSIONS.EXE"));
    assert(!RecompiledImage(L"LegoDimensionsLauncher.exe"));
    assert(!RecompiledImage(L"notlego_dimensions.exe"));
    for (Box bounds : {Box{100,200,1380,920}, Box{-1900,-100,-1100,400}, Box{0,0,320,180}}) {
        const auto r = Fit(bounds, 1600, 900);
        assert(r.left >= bounds.left && r.top >= bounds.top && r.right <= bounds.right && r.bottom <= bounds.bottom);
        assert((r.right-r.left)*9 == (r.bottom-r.top)*16);
        const Box offscreen{-10000,10000,0,0};
        const auto moved = Fit(bounds, 1600, 900, &offscreen);
        assert(moved.left == bounds.left && moved.bottom == bounds.bottom);
    }
    const auto centered=Fit({100,200,1700,1100},800,450);
    assert(centered.left==500 && centered.top==425 && centered.right==1300 && centered.bottom==875);
    bool refused=false;try { Fit({0,0,0,10},100,100); } catch(const std::invalid_argument&) { refused=true; }
    assert(refused);
}
