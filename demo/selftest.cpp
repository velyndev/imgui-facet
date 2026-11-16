#include "selftest.h"

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

class PageRun
{
public:
    PageRun(const PageInfo& pageInfo, int pageIndex, HeadlessSession& headless)
        : page(pageInfo), session(headless)
    {
        gallery.selectedPage = pageIndex;
        ctx.probes = &probes;
    }

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
        {
            const ProbeRecord* probe = probes.Find(step.text);
            if (probe == nullptr)
                return Report(false, "click '" + step.text + "': probe not found");
            const ImVec2 center((probe->min.x + probe->max.x) * 0.5f, (probe->min.y + probe->max.y) * 0.5f);
            io.AddMousePosEvent(center.x, center.y);
            Frame();
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
            Frame();
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
            Frames(2);
            return true;
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
        }
        return Report(false, "unknown step");
    }

    int passed = 0;
    int failed = 0;

private:
    bool Report(bool ok, const std::string& description)
    {
        std::printf("%s %s: %s\n", ok ? "PASS" : "FAIL", page.id.c_str(), description.c_str());
        std::fflush(stdout);
        if (ok)
            ++passed;
        else
            ++failed;
        return ok;
    }

    const PageInfo& page;
    HeadlessSession& session;
    GalleryState gallery;
    PageCtx ctx;
    ProbeTable probes;
};

} // namespace

void ProbeTable::Clear()
{
    entries.clear();
}

void ProbeTable::RecordLastItem(std::string_view name)
{
    ProbeRecord record;
    record.id = ImGui::GetItemID();
    record.min = ImGui::GetItemRectMin();
    record.max = ImGui::GetItemRectMax();
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

void SelftestScript::Click(std::string_view probe)
{
    Step step;
    step.kind = StepKind::Click;
    step.text = probe;
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

        HeadlessSession session;
        if (!session.Begin(shell, HeadlessOptions{}))
        {
            std::printf("FAIL %s: offscreen session could not start\n", page.id.c_str());
            ++totalFailed;
            continue;
        }
        PageRun run(page, pageIndex, session);
        run.Frames(kWarmupFrames);
        for (const SelftestScript::Step& step : script.Steps())
            run.Execute(step);
        totalPassed += run.passed;
        totalFailed += run.failed;
        if (!session.Healthy())
        {
            std::printf("FAIL %s: a D3D call failed\n", page.id.c_str());
            ++totalFailed;
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

    std::printf("selftest: %d script(s), %d passed, %d failed\n", scripts, totalPassed, totalFailed);
    return totalFailed == 0 && totalPassed > 0 ? 0 : 1;
}

} // namespace facet::demo
