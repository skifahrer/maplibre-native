#include <mln/style/conversion/terrain.hpp>
#include <mln/style/conversion/constant.hpp>
#include <mln/style/conversion_impl.hpp>

namespace mln {
namespace style {
namespace conversion {

std::optional<Terrain> Converter<Terrain>::operator()(const Convertible& value, Error& error) const {
    if (!isObject(value)) {
        error.message = "terrain must be an object";
        return std::nullopt;
    }

    // Parse source (required)
    std::optional<Convertible> sourceValue = objectMember(value, "source");
    if (!sourceValue) {
        error.message = "terrain must have a source";
        return std::nullopt;
    }

    std::optional<std::string> source = toString(*sourceValue);
    if (!source) {
        error.message = "terrain source must be a string";
        return std::nullopt;
    }

    // Parse exaggeration (optional, default: 1.0)
    float exaggeration = 1.0f;
    std::optional<Convertible> exaggerationValue = objectMember(value, "exaggeration");
    if (exaggerationValue) {
        std::optional<float> converted = toNumber(*exaggerationValue);
        if (!converted) {
            error.message = "terrain exaggeration must be a number";
            return std::nullopt;
        }
        exaggeration = *converted;
    }

    // Parse plane (optional): the height the DEM writes where it has no ground
    std::optional<float> plane;
    if (std::optional<Convertible> planeValue = objectMember(value, "plane")) {
        plane = toNumber(*planeValue);
        if (!plane) {
            error.message = "terrain plane must be a number";
            return std::nullopt;
        }
    }

    Terrain terrain(*source, exaggeration);
    if (plane) {
        terrain.setPlane(plane);
    }
    return terrain;
}

} // namespace conversion
} // namespace style
} // namespace mln
