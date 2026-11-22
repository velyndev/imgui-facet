#include "pages/pages.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <map>
#include <utility>

#include <imgui.h>

namespace facet::demo
{
namespace
{

struct Registry
{
    std::vector<PageInfo> pages;
    std::vector<std::string> duplicateIds;  // ids registered more than once
    bool sorted = true;
};

// Function-local static: registrars in other translation units may run before any
// namespace-scope object of this file is constructed.
Registry& GetRegistry()
{
    static Registry registry;
    return registry;
}

// The navigation groups in their fixed order. Other groups follow them.
constexpr const char* kGroupOrder[] = {"Start", "Foundations", "Media", "Blocks", "Widgets"};

int GroupRank(const std::string& group)
{
    for (int i = 0; i < static_cast<int>(std::size(kGroupOrder)); ++i)
    {
        if (group == kGroupOrder[i])
            return i;
    }
    return static_cast<int>(std::size(kGroupOrder));
}

void SortPages(std::vector<PageInfo>& pages)
{
    std::map<std::string, int> groupOrder;
    for (const PageInfo& page : pages)
    {
        auto it = groupOrder.find(page.group);
        if (it == groupOrder.end())
            groupOrder.emplace(page.group, page.order);
        else
            it->second = std::min(it->second, page.order);
    }

    std::stable_sort(pages.begin(), pages.end(), [&](const PageInfo& a, const PageInfo& b) {
        const int rankA = GroupRank(a.group);
        const int rankB = GroupRank(b.group);
        if (rankA != rankB)
            return rankA < rankB;
        const int groupA = groupOrder[a.group];
        const int groupB = groupOrder[b.group];
        if (groupA != groupB)
            return groupA < groupB;
        if (a.group != b.group)
            return a.group < b.group;
        if (a.order != b.order)
            return a.order < b.order;
        return a.title < b.title;
    });
}

} // namespace

void PageCtx::Probe(std::string_view name) const
{
    if (probes != nullptr)
        probes->RecordLastItem(name);
}

void PageCtx::RevealInCapture(float ratio) const
{
    if (capture)
        ImGui::SetScrollHereY(ratio);
}

void PageCtx::ForceNextItemState(ForcedItemState state, int part) const
{
    if (capture)
        facet::internal::ForceNextItemState(state, part);
}

std::span<const PageInfo> Pages()
{
    Registry& registry = GetRegistry();
    if (!registry.sorted)
    {
        SortPages(registry.pages);
        registry.sorted = true;
    }
    return registry.pages;
}

int FindPage(std::string_view id)
{
    const std::span<const PageInfo> pages = Pages();
    for (size_t i = 0; i < pages.size(); ++i)
    {
        if (pages[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

PageRegistrar::PageRegistrar(const char* id, const char* title, const char* group, int order,
                             PageFn draw, std::initializer_list<const char*> states)
{
    PageInfo info;
    info.id = id;
    info.title = title;
    info.group = group;
    info.order = order;
    info.draw = draw;
    for (const char* state : states)
        info.states.emplace_back(state);
    if (info.states.empty())
        info.states.emplace_back("default");

    Registry& registry = GetRegistry();
    const bool duplicate = std::any_of(registry.pages.begin(), registry.pages.end(),
                                       [&](const PageInfo& page) { return page.id == info.id; });
    if (duplicate)
    {
        // Keep the first registration; ValidateRegistry reports the clash.
        registry.duplicateIds.push_back(info.id);
        return;
    }
    registry.pages.push_back(std::move(info));
    registry.sorted = false;
}

bool ValidateRegistry()
{
    bool valid = true;
    for (const std::string& id : GetRegistry().duplicateIds)
    {
        std::fprintf(stderr, "error: page id '%s' is registered more than once\n", id.c_str());
        valid = false;
    }
    for (const std::string& id : DuplicateSelftestIds())
    {
        std::fprintf(stderr, "error: page '%s' registers more than one selftest script\n",
                     id.c_str());
        valid = false;
    }
    for (const std::string& id : SelftestPageIds())
    {
        if (FindPage(id) < 0)
        {
            std::fprintf(stderr, "error: selftest script for unknown page id '%s'\n", id.c_str());
            valid = false;
        }
    }
    for (const std::string& name : DuplicateAppSelftestNames())
    {
        std::fprintf(stderr, "error: app selftest '%s' is registered more than once\n", name.c_str());
        valid = false;
    }
    for (const AppSelftestInfo& app : AppSelftests())
    {
        if (FindPage(app.startPage) < 0)
        {
            std::fprintf(stderr, "error: app selftest '%s' starts on unknown page id '%s'\n", app.name.c_str(),
                         app.startPage.c_str());
            valid = false;
        }
    }
    return valid;
}

} // namespace facet::demo
