#include "selftest.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <utility>

#include <imgui_internal.h>

#include "app_shell.h"
#include "gallery.h"
#include "offscreen.h"
#include "pages/pages.h"

namespace facet::demo
{
namespace
{

// Frames rendered before the first step, so layout and fonts are settled.
constexpr int kWarmupFrames = 10;
// Frames rendered after asking for a scroll, so the scroll is applied and recorded again.
constexpr int kRevealFrames = 3;

std::map<std::string, SelftestFn, std::less<>>& SelftestRegistry()
{
    static std::map<std::string, SelftestFn, std::less<>> registry;
    return registry;
}

std::vector<std::string>& DuplicateSelftestStorage()
{
    static std::vector<std::string> duplicates;
    return duplicates;
}

ImGuiKeyChord ModifierKeys(ImGuiKeyChord chord)
{
    return chord & ImGuiMod_Mask_;
}

ImGuiKey MainKey(ImGuiKeyChord chord)
{
    return static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
}

void QueueModifiers(ImGuiIO& io, ImGuiKeyChord mods, bool down)
{
    if (mods & ImGuiMod_Ctrl)
        io.AddKeyEvent(ImGuiMod_Ctrl, down);
    if (mods & ImGuiMod_Shift)
        io.AddKeyEvent(ImGuiMod_Shift, down);
    if (mods & ImGuiMod_Alt)
        io.AddKeyEvent(ImGuiMod_Alt, down);
    if (mods & ImGuiMod_Super)
        io.AddKeyEvent(ImGuiMod_Super, down);
}

// Modifiers go down before the key and come up after it.
void QueueChord(ImGuiIO& io, ImGuiKeyChord chord, bool down)
{
    const ImGuiKey key = MainKey(chord);
    if (down)
        QueueModifiers(io, ModifierKeys(chord), true);
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, down);
    if (!down)
        QueueModifiers(io, ModifierKeys(chord), false);
}

// Frames that cover `seconds` of frame time at the fixed time step; at least one.
int FramesFor(float seconds)
{
    const float frames = std::ceil(seconds / HeadlessSession::kTimeStep - 1e-3f);
    return std::max(1, static_cast<int>(frames));
}

std::string Describe(const Target& target)
{
    char buffer[160];
    if (target.probe.empty())
        std::snprintf(buffer, sizeof(buffer), "the display at %.2f, %.2f", static_cast<double>(target.fraction.x),
                      static_cast<double>(target.fraction.y));
    else
        std::snprintf(buffer, sizeof(buffer), "'%s' at %.2f, %.2f", target.probe.c_str(),
                      static_cast<double>(target.fraction.x), static_cast<double>(target.fraction.y));
    return buffer;
}

// Clipboard hooks of a run: the text lives in the std::string the platform io points to.
const char* GetRunClipboard(ImGuiContext*)
{
    const auto* text = static_cast<const std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    return text != nullptr ? text->c_str() : "";
}

void SetRunClipboard(ImGuiContext*, const char* value)
{
    auto* text = static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    if (text != nullptr)
        *text = value != nullptr ? value : "";
}

class PageRun
{
public:
    PageRun(const PageInfo& pageInfo, int pageIndex, bool dark, float scale, HeadlessSession& headless)
        : page(pageInfo), session(headless), label(pageInfo.id + " (" + ThemeName(dark) + ")")
    {
        gallery.selectedPage = pageIndex;
        gallery.dark = dark;
        gallery.baseScale = scale;
        ctx.probes = &probes;
        // The run's own clipboard: copy and paste in a script never touch the system clipboard.
        ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
        platform.Platform_ClipboardUserData = &clipboard;
        platform.Platform_GetClipboardTextFn = &GetRunClipboard;
        platform.Platform_SetClipboardTextFn = &SetRunClipboard;
    }

    ~PageRun()
    {
        ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
        platform.Platform_GetClipboardTextFn = nullptr;
        platform.Platform_SetClipboardTextFn = nullptr;
        platform.Platform_ClipboardUserData = nullptr;
    }

    PageRun(const PageRun&) = delete;
    PageRun& operator=(const PageRun&) = delete;

    void Frame()
    {
        session.Frame([&] {
            probes.Clear();
            DrawGallery(gallery, ctx, nullptr);
        });
    }

    void Frames(int count)
    {
        for (int i = 0; i < count; ++i)
            Frame();
    }

    // Returns false on a failed expectation or a step that could not run.
    bool Execute(const SelftestScript::Step& step)
    {
        ImGuiIO& io = ImGui::GetIO();
        switch (step.kind)
        {
        case SelftestScript::StepKind::Do:
            step.action();
            Frame();
            return true;

        case SelftestScript::StepKind::Click:
        case SelftestScript::StepKind::DoubleClick:
        {
            const bool twice = step.kind == SelftestScript::StepKind::DoubleClick;
            const char* verb = twice ? "double click '" : "click '";
            const ProbeRecord* probe = probes.Find(step.text);
            if (probe == nullptr)
                return Report(false, verb + step.text + "': probe not found");
            if (!probe->visible)
            {
                probes.RequestReveal(step.text);
                Frames(kRevealFrames);
                probe = probes.Find(step.text);
                if (probe == nullptr || !probe->visible)
                    return Report(false, verb + step.text + "': item cannot be scrolled into view");
            }
            const ImVec2 center((probe->min.x + probe->max.x) * 0.5f, (probe->min.y + probe->max.y) * 0.5f);
            io.AddMousePosEvent(center.x, center.y);
            Frame();
            io.AddMouseButtonEvent(step.button, true);
            Frame();
            io.AddMouseButtonEvent(step.button, false);
            Frame();
            if (twice)
            {
                // Two frames from the first press: far inside io.MouseDoubleClickTime.
                io.AddMouseButtonEvent(step.button, true);
                Frame();
                io.AddMouseButtonEvent(step.button, false);
                Frame();
            }
            Frame();
            return true;
        }

        case SelftestScript::StepKind::SetClipboard:
            clipboard = step.text;
            return true;

        case SelftestScript::StepKind::ExpectClipboard:
        {
            const bool ok = clipboard == step.expectedText;
            return Report(ok, ok ? step.text : step.text + " (found \"" + clipboard + "\")");
        }

        case SelftestScript::StepKind::ExpectNotVisible:
        {
            const ProbeRecord* probe = probes.Find(step.text);
            std::string description = "'" + step.text + "' is not visible";
            if (probe == nullptr && !probes.SeenInRun(step.text))
                return Report(false, description + " (probe never recorded in this run)");
            const bool hidden = probe == nullptr || !probe->visible;
            if (probe == nullptr)
                description += " (not submitted in the latest frame)";
            return Report(hidden, description);
        }

        case SelftestScript::StepKind::KeyPress:
        {
            const ImGuiKeyChord mods = ModifierKeys(step.chord);
            const ImGuiKey key = MainKey(step.chord);
            QueueModifiers(io, mods, true);
            io.AddKeyEvent(key, true);
            Frame();
            io.AddKeyEvent(key, false);
            QueueModifiers(io, mods, false);
            Frames(2);
            return true;
        }

        case SelftestScript::StepKind::Wait:
            Frames(step.frames);
            return true;

        case SelftestScript::StepKind::Expect:
            return Report(step.check(), step.text);

        case SelftestScript::StepKind::ExpectFocused:
        {
            const ProbeRecord* probe = probes.Find(step.text);
            const ImGuiID focusId = ImGui::GetFocusID();
            const bool focused = probe != nullptr && probe->id != 0 && focusId == probe->id;
            std::string description = "keyboard focus is on '" + step.text + "'";
            if (!focused)
            {
                const char* actual = probes.NameOf(focusId);
                description += actual != nullptr ? std::string(" (found on '") + actual + "')"
                                                 : std::string(" (found on an unprobed item)");
            }
            return Report(focused, description);
        }

        case SelftestScript::StepKind::MouseMove:
        {
            ImVec2 pos;
            std::string why;
            if (!Resolve(step.path.front(), true, &pos, &why))
                return Report(false, "move to " + Describe(step.path.front()) + ": " + why);
            io.AddMousePosEvent(pos.x, pos.y);
            Frames(step.seconds > 0.0f ? FramesFor(step.seconds) : 1);
            return true;
        }

        case SelftestScript::StepKind::MouseButton:
            io.AddMouseButtonEvent(step.button, step.down);
            Frame();
            return true;

        case SelftestScript::StepKind::Drag:
            return Drag(step);

        case SelftestScript::StepKind::Wheel:
        {
            ImVec2 pos;
            std::string why;
            if (!Resolve(step.path.front(), true, &pos, &why))
                return Report(false, "wheel over " + Describe(step.path.front()) + ": " + why);
            io.AddMousePosEvent(pos.x, pos.y);
            Frame();
            io.AddMouseWheelEvent(step.wheel.x, step.wheel.y);
            Frames(2);
            return true;
        }

        case SelftestScript::StepKind::KeyHold:
            QueueChord(io, step.chord, true);
            Frames(FramesFor(step.seconds));
            QueueChord(io, step.chord, false);
            Frames(2);
            return true;

        case SelftestScript::StepKind::KeyChange:
            QueueChord(io, step.chord, step.down);
            Frame();
            return true;

        case SelftestScript::StepKind::Type:
            io.AddInputCharactersUTF8(step.text.c_str());
            Frame();
            return true;

        case SelftestScript::StepKind::ExpectNear:
        {
            const double value = step.number();
            const bool ok = std::fabs(value - step.expected) <= step.tolerance;
            char found[96];
            std::snprintf(found, sizeof(found), " (found %.6g, expected %.6g within %.3g)", value, step.expected,
                          step.tolerance);
            return Report(ok, step.text + found);
        }

        case SelftestScript::StepKind::ExpectText:
        {
            const std::string value = step.string();
            const bool ok = value == step.expectedText;
            return Report(ok, ok ? step.text : step.text + " (found \"" + value + "\")");
        }

        case SelftestScript::StepKind::ExpectVisible:
        {
            const ProbeRecord* probe = probes.Find(step.text);
            const bool visible = probe != nullptr && probe->visible;
            return Report(visible, "'" + step.text + "' is visible" +
                                       (probe == nullptr ? std::string(" (probe not found)") : std::string()));
        }

        case SelftestScript::StepKind::Reveal:
        {
            if (probes.Find(step.text) == nullptr)
                return Report(false, "reveal '" + step.text + "': probe not found");
            probes.RequestReveal(step.text);
            Frames(kRevealFrames);
            return true;
        }

        case SelftestScript::StepKind::SelectPage:
        {
            const int index = FindPage(step.text);
            if (index < 0)
                return Report(false, "select page '" + step.text + "': no page with this id");
            gallery.selectedPage = index;
            Frames(kRevealFrames);
            return true;
        }

        case SelftestScript::StepKind::ExpectPage:
        {
            const std::span<const PageInfo> pages = Pages();
            const int shown = gallery.selectedPage;
            const bool ok = shown >= 0 && shown < static_cast<int>(pages.size()) &&
                            pages[static_cast<size_t>(shown)].id == step.text;
            std::string description = "the page '" + step.text + "' is shown";
            if (!ok)
                description += shown >= 0 && shown < static_cast<int>(pages.size())
                                   ? " (found '" + pages[static_cast<size_t>(shown)].id + "')"
                                   : std::string(" (found none)");
            return Report(ok, description);
        }
        }
        return Report(false, "unknown step");
    }

    int passed = 0;
    int failed = 0;

private:
    // The pixel position of a target in the latest frame. With `reveal`, an item outside the
    // visible area of its window is scrolled into view first.
    bool Resolve(const Target& target, bool reveal, ImVec2* pos, std::string* why)
    {
        const ImVec2 offset(target.offset.x * ctx.scale, target.offset.y * ctx.scale);
        if (target.probe.empty())
        {
            const ImVec2 display = ImGui::GetIO().DisplaySize;
            *pos = ImVec2(display.x * target.fraction.x + offset.x, display.y * target.fraction.y + offset.y);
            return true;
        }
        const ProbeRecord* probe = probes.Find(target.probe);
        if (probe == nullptr)
        {
            *why = "probe not found";
            return false;
        }
        if (reveal && !probe->visible)
        {
            probes.RequestReveal(target.probe);
            Frames(kRevealFrames);
            probe = probes.Find(target.probe);
            if (probe == nullptr || !probe->visible)
            {
                *why = "item cannot be scrolled into view";
                return false;
            }
        }
        *pos = ImVec2(probe->min.x + (probe->max.x - probe->min.x) * target.fraction.x + offset.x,
                      probe->min.y + (probe->max.y - probe->min.y) * target.fraction.y + offset.y);
        return true;
    }

    // Press at the first point, an even walk along the path, release at the last point.
    bool Drag(const SelftestScript::Step& step)
    {
        ImGuiIO& io = ImGui::GetIO();
        std::vector<ImVec2> points;
        for (size_t i = 0; i < step.path.size(); ++i)
        {
            ImVec2 pos;
            std::string why;
            if (!Resolve(step.path[i], i == 0, &pos, &why))
                return Report(false, "drag through " + Describe(step.path[i]) + ": " + why);
            points.push_back(pos);
        }
        if (points.empty())
            return Report(false, "drag without points");

        std::vector<float> lengths(points.size(), 0.0f);  // path length up to each point
        for (size_t i = 1; i < points.size(); ++i)
        {
            const float dx = points[i].x - points[i - 1].x;
            const float dy = points[i].y - points[i - 1].y;
            lengths[i] = lengths[i - 1] + std::sqrt(dx * dx + dy * dy);
        }
        const int segments = static_cast<int>(points.size()) - 1;
        const int frames = std::max(FramesFor(step.seconds), segments);

        io.AddMousePosEvent(points.front().x, points.front().y);
        Frame();
        io.AddMouseButtonEvent(step.button, true);
        Frame();
        size_t segment = 1;
        for (int frame = 1; frame <= frames && segments > 0; ++frame)
        {
            const float distance = lengths.back() * static_cast<float>(frame) / static_cast<float>(frames);
            while (segment + 1 < points.size() && lengths[segment] < distance)
                ++segment;
            const float span = lengths[segment] - lengths[segment - 1];
            const float t = span > 0.0f ? (distance - lengths[segment - 1]) / span : 1.0f;
            const ImVec2 a = points[segment - 1];
            const ImVec2 b = points[segment];
            io.AddMousePosEvent(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
            Frame();
        }
        io.AddMousePosEvent(points.back().x, points.back().y);
        if (step.hold > 0.0f)
            Frames(FramesFor(step.hold));  // the button stays down, the mouse rests on the target
        io.AddMouseButtonEvent(step.button, false);
        Frames(2);
        return true;
    }

    bool Report(bool ok, const std::string& description)
    {
        std::printf("%s %s: %s\n", ok ? "PASS" : "FAIL", label.c_str(), description.c_str());
        std::fflush(stdout);
        if (ok)
            ++passed;
        else
            ++failed;
        return ok;
    }

    const PageInfo& page;
    HeadlessSession& session;
    std::string label;  // "<page> (<theme>)" for result lines
    GalleryState gallery;
    PageCtx ctx;
    ProbeTable probes;
    std::string clipboard;  // the run's own clipboard, read and written by Dear ImGui
};

} // namespace

void ProbeTable::Clear()
{
    entries.clear();
}

void ProbeTable::RecordLastItem(std::string_view name)
{
    if (!pendingReveal.empty() && pendingReveal == name)
    {
        ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
        pendingReveal.clear();
    }

    ProbeRecord record;
    record.id = ImGui::GetItemID();
    record.min = ImGui::GetItemRectMin();
    record.max = ImGui::GetItemRectMax();
    const ImVec2 center((record.min.x + record.max.x) * 0.5f, (record.min.y + record.max.y) * 0.5f);
    // The clip rect in effect for the item: the window's visible area, narrowed by a clip rect
    // the code around the item pushed (a table column, a list region under a pinned header).
    record.visible = ImGui::GetCurrentWindowRead()->ClipRect.Contains(center);
    if (!SeenInRun(name))
        seen.emplace_back(name);
    for (auto& entry : entries)
    {
        if (entry.first == name)
        {
            entry.second = record;
            return;
        }
    }
    entries.emplace_back(std::string(name), record);
}

bool ProbeTable::SeenInRun(std::string_view name) const
{
    return std::find(seen.begin(), seen.end(), name) != seen.end();
}

const char* ProbeTable::NameOf(ImGuiID id) const
{
    if (id == 0)
        return nullptr;
    for (const auto& entry : entries)
    {
        if (entry.second.id == id)
            return entry.first.c_str();
    }
    return nullptr;
}

const ProbeRecord* ProbeTable::Find(std::string_view name) const
{
    for (const auto& entry : entries)
    {
        if (entry.first == name)
            return &entry.second;
    }
    return nullptr;
}

void SelftestScript::Do(std::function<void()> action)
{
    Step step;
    step.kind = StepKind::Do;
    step.action = std::move(action);
    steps.push_back(std::move(step));
}

void SelftestScript::Click(std::string_view probe, ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::Click;
    step.text = probe;
    step.button = button;
    steps.push_back(std::move(step));
}

void SelftestScript::DoubleClick(std::string_view probe, ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::DoubleClick;
    step.text = probe;
    step.button = button;
    steps.push_back(std::move(step));
}

void SelftestScript::KeyPress(ImGuiKeyChord chord)
{
    Step step;
    step.kind = StepKind::KeyPress;
    step.chord = chord;
    steps.push_back(std::move(step));
}

void SelftestScript::Wait(int frames)
{
    Step step;
    step.kind = StepKind::Wait;
    step.frames = frames;
    steps.push_back(std::move(step));
}

void SelftestScript::Expect(std::string_view description, std::function<bool()> check)
{
    Step step;
    step.kind = StepKind::Expect;
    step.text = description;
    step.check = std::move(check);
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectFocused(std::string_view probe)
{
    Step step;
    step.kind = StepKind::ExpectFocused;
    step.text = probe;
    steps.push_back(std::move(step));
}

Target Target::Item(std::string_view probe, float fx, float fy)
{
    Target target;
    target.probe = probe;
    target.fraction = ImVec2(fx, fy);
    return target;
}

Target Target::Display(float fx, float fy)
{
    Target target;
    target.fraction = ImVec2(fx, fy);
    return target;
}

Target Target::Offset(float dx, float dy) const
{
    Target target = *this;
    target.offset = ImVec2(offset.x + dx, offset.y + dy);
    return target;
}

void SelftestScript::WaitSeconds(float seconds)
{
    Wait(FramesFor(seconds));
}

void SelftestScript::MouseMove(const Target& target)
{
    Step step;
    step.kind = StepKind::MouseMove;
    step.path.push_back(target);
    steps.push_back(std::move(step));
}

void SelftestScript::Hover(const Target& target, float seconds)
{
    Step step;
    step.kind = StepKind::MouseMove;
    step.path.push_back(target);
    step.seconds = std::max(seconds, 0.0f);
    steps.push_back(std::move(step));
}

void SelftestScript::MouseDown(ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::MouseButton;
    step.button = button;
    step.down = true;
    steps.push_back(std::move(step));
}

void SelftestScript::MouseUp(ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::MouseButton;
    step.button = button;
    step.down = false;
    steps.push_back(std::move(step));
}

void SelftestScript::Drag(std::vector<Target> path, float seconds, ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::Drag;
    step.path = std::move(path);
    step.seconds = seconds;
    step.button = button;
    steps.push_back(std::move(step));
}

void SelftestScript::DragAndHold(std::vector<Target> path, float seconds, float holdSeconds, ImGuiMouseButton button)
{
    Step step;
    step.kind = StepKind::Drag;
    step.path = std::move(path);
    step.seconds = seconds;
    step.hold = std::max(holdSeconds, 0.0f);
    step.button = button;
    steps.push_back(std::move(step));
}

void SelftestScript::Wheel(const Target& target, float wheelY, float wheelX)
{
    Step step;
    step.kind = StepKind::Wheel;
    step.path.push_back(target);
    step.wheel = ImVec2(wheelX, wheelY);
    steps.push_back(std::move(step));
}

void SelftestScript::HoldKey(ImGuiKeyChord chord, float seconds)
{
    Step step;
    step.kind = StepKind::KeyHold;
    step.chord = chord;
    step.seconds = seconds;
    steps.push_back(std::move(step));
}

void SelftestScript::KeyDown(ImGuiKeyChord chord)
{
    Step step;
    step.kind = StepKind::KeyChange;
    step.chord = chord;
    step.down = true;
    steps.push_back(std::move(step));
}

void SelftestScript::KeyUp(ImGuiKeyChord chord)
{
    Step step;
    step.kind = StepKind::KeyChange;
    step.chord = chord;
    step.down = false;
    steps.push_back(std::move(step));
}

void SelftestScript::Type(std::string_view text)
{
    Step step;
    step.kind = StepKind::Type;
    step.text = text;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectNear(std::string_view description, std::function<double()> value, double expected,
                                double tolerance)
{
    Step step;
    step.kind = StepKind::ExpectNear;
    step.text = description;
    step.number = std::move(value);
    step.expected = expected;
    step.tolerance = tolerance;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectText(std::string_view description, std::function<std::string()> value,
                                std::string_view expected)
{
    Step step;
    step.kind = StepKind::ExpectText;
    step.text = description;
    step.string = std::move(value);
    step.expectedText = expected;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectVisible(std::string_view probe)
{
    Step step;
    step.kind = StepKind::ExpectVisible;
    step.text = probe;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectNotVisible(std::string_view probe)
{
    Step step;
    step.kind = StepKind::ExpectNotVisible;
    step.text = probe;
    steps.push_back(std::move(step));
}

void SelftestScript::SetClipboard(std::string_view text)
{
    Step step;
    step.kind = StepKind::SetClipboard;
    step.text = text;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectClipboard(std::string_view description, std::string_view expected)
{
    Step step;
    step.kind = StepKind::ExpectClipboard;
    step.text = description;
    step.expectedText = expected;
    steps.push_back(std::move(step));
}

void SelftestScript::Reveal(std::string_view probe)
{
    Step step;
    step.kind = StepKind::Reveal;
    step.text = probe;
    steps.push_back(std::move(step));
}

void SelftestScript::SelectPage(std::string_view pageId)
{
    Step step;
    step.kind = StepKind::SelectPage;
    step.text = pageId;
    steps.push_back(std::move(step));
}

void SelftestScript::ExpectPage(std::string_view pageId)
{
    Step step;
    step.kind = StepKind::ExpectPage;
    step.text = pageId;
    steps.push_back(std::move(step));
}

SelftestRegistrar::SelftestRegistrar(const char* pageId, SelftestFn build)
{
    if (!SelftestRegistry().emplace(pageId, build).second)
        DuplicateSelftestStorage().emplace_back(pageId);
}

std::vector<std::string> SelftestPageIds()
{
    std::vector<std::string> ids;
    for (const auto& entry : SelftestRegistry())
        ids.push_back(entry.first);
    return ids;
}

const std::vector<std::string>& DuplicateSelftestIds()
{
    return DuplicateSelftestStorage();
}

SelftestFn FindSelftest(std::string_view pageId)
{
    const auto& registry = SelftestRegistry();
    const auto it = registry.find(pageId);
    return it != registry.end() ? it->second : nullptr;
}

int RunSelftest(AppShell& shell, const SelftestOptions& options)
{
    const std::span<const PageInfo> pages = Pages();
    int totalPassed = 0;
    int totalFailed = 0;
    int scripts = 0;

    for (int pageIndex = 0; pageIndex < static_cast<int>(pages.size()); ++pageIndex)
    {
        const PageInfo& page = pages[static_cast<size_t>(pageIndex)];
        if (!options.page.empty() && page.id != options.page)
            continue;
        const SelftestFn build = FindSelftest(page.id);
        if (build == nullptr)
        {
            if (!options.page.empty())
            {
                std::printf("FAIL %s: page has no selftest script\n", page.id.c_str());
                ++totalFailed;
            }
            continue;
        }
        ++scripts;

        SelftestScript script;
        build(script);

        for (const bool dark : SelectedThemes(options.themes))
        {
            HeadlessSession session;
            HeadlessOptions headless;
            headless.width = options.width;
            headless.height = options.height;
            headless.scale = options.scale;
            if (!session.Begin(shell, headless))
            {
                std::printf("FAIL %s (%s): offscreen session could not start\n", page.id.c_str(),
                            ThemeName(dark));
                ++totalFailed;
                continue;
            }
            PageRun run(page, pageIndex, dark, options.scale, session);
            run.Frames(kWarmupFrames);
            for (const SelftestScript::Step& step : script.Steps())
                run.Execute(step);
            totalPassed += run.passed;
            totalFailed += run.failed;
            if (!session.Healthy())
            {
                std::printf("FAIL %s (%s): a D3D call failed\n", page.id.c_str(), ThemeName(dark));
                ++totalFailed;
            }
        }
    }

    if (!options.page.empty() && FindPage(options.page) < 0)
    {
        std::printf("FAIL %s: no page with this id\n", options.page.c_str());
        ++totalFailed;
    }
    if (!shell.DeviceHealthy())
    {
        std::printf("FAIL device: the D3D device was removed\n");
        ++totalFailed;
    }

    std::printf("selftest: %d script(s) at %dx%d, scale %g, %d passed, %d failed\n", scripts, options.width,
                options.height, static_cast<double>(options.scale), totalPassed, totalFailed);
    return totalFailed == 0 && totalPassed > 0 ? 0 : 1;
}

} // namespace facet::demo
