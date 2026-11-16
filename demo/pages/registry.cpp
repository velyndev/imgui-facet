#include "pages/pages.h"

#include <algorithm>
#include <map>
#include <utility>

namespace facet::demo
{
namespace
{

struct Registry
{
    std::vector<PageInfo> pages;
    bool sorted = true;
};

// Function-local static: registrars in other translation units may run before any
// namespace-scope object of this file is constructed.
Registry& GetRegistry()
{
    static Registry registry;
    return registry;
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
    registry.pages.push_back(std::move(info));
    registry.sorted = false;
}

} // namespace facet::demo
