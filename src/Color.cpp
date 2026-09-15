// CHyprColor definitions that do not pull in hyprgraphics.
//
// Hyprland's helpers/Color.cpp converts every color into OkLab on construction, which
// links against libhyprgraphics, which in turn drags in cairo, pango and the image
// loaders. A converter only ever reads a color in and writes the same color back out as
// hex, so the sRGB components and the hex round trip are implemented here and the
// perceptual conversions are not.
//
// Anything that would need a real OkLab value is defined to trap rather than to return a
// plausible wrong answer.

#include <print>
#include <cstdlib>

#include "helpers/Color.hpp"

[[noreturn]] static void notInConverter(const char* what) {
    std::println(stderr, "hyprlang2lua: {} is not available in the converter build", what);
    std::abort();
}

CHyprColor::CHyprColor() = default;

CHyprColor::CHyprColor(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {
    ;
}

// AR32, the order Hyprland stores colors in
CHyprColor::CHyprColor(uint64_t hex) :
    r(static_cast<double>((hex >> 16) & 0xFF) / 255.0), g(static_cast<double>((hex >> 8) & 0xFF) / 255.0), b(static_cast<double>(hex & 0xFF) / 255.0),
    a(static_cast<double>((hex >> 24) & 0xFF) / 255.0) {
    ;
}

// converting from a hyprgraphics color is the one path that would need the library, and
// nothing in the converter takes it: colors arrive as text and leave as hex
CHyprColor::CHyprColor(const Hyprgraphics::CColor&, float a_) : a(a_) {
    notInConverter("CHyprColor from Hyprgraphics::CColor");
}

uint32_t CHyprColor::getAsHex() const {
    const auto CHANNEL = [](double v) { return static_cast<uint32_t>(v * 255.0 + 0.5) & 0xFFU; };
    return (CHANNEL(a) << 24) | (CHANNEL(r) << 16) | (CHANNEL(g) << 8) | CHANNEL(b);
}

Hyprgraphics::CColor::SSRGB CHyprColor::asRGB() const {
    return {.r = static_cast<float>(r), .g = static_cast<float>(g), .b = static_cast<float>(b)};
}

Hyprgraphics::CColor::SOkLab CHyprColor::asOkLab() const {
    return m_okLab;
}

Hyprgraphics::CColor::SHSL CHyprColor::asHSL() const {
    notInConverter("CHyprColor::asHSL");
}

CHyprColor CHyprColor::stripA() const {
    return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), 1.F};
}

CHyprColor CHyprColor::modifyA(float newa) const {
    return {static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), newa};
}
