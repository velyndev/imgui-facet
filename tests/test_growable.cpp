// Collections a widget grows or shrinks (docs/api-conventions.md, rule 5): every such widget
// takes the caller's storage with a count, and a vector overload where it fits; a fixed-span
// overload cannot silently drop the options that add and remove.

#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "support/session.h"

namespace
{

// The growable shapes, by exact signature: a sixth widget that grows a collection another way
// fails to compile against this list once it is added here.
using MultiSliderStorage = bool (*)(const char*, std::span<float>, int*, float, float, const facet::MultiSliderOptions&);
using MultiSliderVector = bool (*)(const char*, std::vector<float>*, float, float, const facet::MultiSliderOptions&);
using SwatchStorage = bool (*)(const char*, ImVec4*, std::span<ImVec4>, int*, facet::ColorHistory*,
                               const facet::SwatchPaletteOptions&);
using SwatchVector = bool (*)(const char*, ImVec4*, std::vector<ImVec4>*, facet::ColorHistory*,
                              const facet::SwatchPaletteOptions&);
using CurveStorage = bool (*)(const char*, std::span<ImVec2>, int*, ImVec2, ImVec2, const facet::CurveEditorOptions&);
using CurveVector = bool (*)(const char*, std::vector<ImVec2>*, ImVec2, ImVec2, const facet::CurveEditorOptions&);
using GradientStorage = bool (*)(const char*, std::span<facet::GradientStop>, int*, const facet::GradientEditorOptions&);
using GradientVector = bool (*)(const char*, std::vector<facet::GradientStop>*, const facet::GradientEditorOptions&);
// Tags own heap memory, so the vector is their only shape.
using TagVector = bool (*)(const char*, std::vector<std::string>*, const facet::TagInputOptions&);

} // namespace

TEST_CASE("Every growable widget has the storage-and-count shape, and a vector where it fits")
{
    CHECK(static_cast<MultiSliderStorage>(&facet::MultiSlider) != nullptr);
    CHECK(static_cast<MultiSliderVector>(&facet::MultiSlider) != nullptr);
    CHECK(static_cast<SwatchStorage>(&facet::SwatchPalette) != nullptr);
    CHECK(static_cast<SwatchVector>(&facet::SwatchPalette) != nullptr);
    CHECK(static_cast<CurveStorage>(&facet::CurveEditor) != nullptr);
    CHECK(static_cast<CurveVector>(&facet::CurveEditor) != nullptr);
    CHECK(static_cast<GradientStorage>(&facet::GradientEditor) != nullptr);
    CHECK(static_cast<GradientVector>(&facet::GradientEditor) != nullptr);
    CHECK(static_cast<TagVector>(&facet::TagInput) != nullptr);
}

TEST_CASE("A fixed-count MultiSlider reports allowAdd and allowRemove instead of ignoring them silently")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<float> handles = {0.2f, 0.7f};
    float storage[4] = {0.2f, 0.7f};
    int count = 2;
    int fixedErrors = 0;
    int growableErrors = 0;
    session.SetUi([&] {
        int before = facet::internal::UsageErrorCount();
        // A vector passed by value binds to the span overload.
        facet::MultiSlider("by value", handles, 0.0f, 1.0f, {.allowAdd = true});
        facet::MultiSlider("by value 2", std::span<float>(handles), 0.0f, 1.0f, {.allowRemove = true});
        facet::MultiSlider("plain", std::span<float>(handles), 0.0f, 1.0f, {.minGap = 0.1});
        fixedErrors = facet::internal::UsageErrorCount() - before;
        before = facet::internal::UsageErrorCount();
        facet::MultiSlider("vector", &handles, 0.0f, 1.0f, {.allowAdd = true, .allowRemove = true});
        facet::MultiSlider("storage", std::span<float>(storage), &count, 0.0f, 1.0f, {.allowAdd = true});
        growableErrors = facet::internal::UsageErrorCount() - before;
    });
    session.Frame();
    CHECK(fixedErrors == 2);
    CHECK(growableErrors == 0);
    CHECK(handles.size() == 2);
    CHECK(count == 2);
}
