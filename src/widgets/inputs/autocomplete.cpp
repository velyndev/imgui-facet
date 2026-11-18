// Autocomplete: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core and the fuzzy matcher.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const AutocompleteOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("Autocomplete", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool Autocomplete(const char* label, std::string*, std::span<const ChoiceItem>, const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

bool Autocomplete(const char* label, std::string*, std::span<const char* const>, const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

bool Autocomplete(const char* label, std::string*, int, ChoiceGetter, void*, const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

bool Autocomplete(const char* label, char*, std::size_t, std::span<const ChoiceItem>, const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

bool Autocomplete(const char* label, char*, std::size_t, std::span<const char* const>,
                  const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

bool Autocomplete(const char* label, char*, std::size_t, int, ChoiceGetter, void*, const AutocompleteOptions& options)
{
    Stub(label, options);
    return false;
}

int AutocompleteAcceptedItem()
{
    return -1;
}

} // namespace facet
