// The interfaces the popup host, scroll core, list core, drag and drop core and text field core
// offer each other (src/internal/popup_host.h, scroll.h, list_core.h, drag_drop.h,
// text_field.h): every declaration has exactly the signature the other blocks call, and links.
// Their behaviour is tested in each block's own test file.

#include <cstddef>
#include <span>
#include <string>
#include <type_traits>

#include <doctest/doctest.h>

#include "internal/drag_drop.h"
#include "internal/list_core.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/scroll.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"

namespace
{

namespace fi = facet::internal;

template <class Signature>
bool Links(Signature* function)
{
    return function != nullptr;
}

// Option structs are aggregates with defaults, like every options struct (rule 4).
static_assert(std::is_aggregate_v<fi::PopupAnchor>);
static_assert(std::is_aggregate_v<fi::PopupOptions>);
static_assert(std::is_aggregate_v<fi::DragDropPreview>);
static_assert(std::is_aggregate_v<fi::DragDropZoneOptions>);
static_assert(std::is_aggregate_v<fi::DragDropZoneResult>);
static_assert(std::is_aggregate_v<fi::ListRowsDesc>);
static_assert(std::is_aggregate_v<fi::ListSelectionOptions>);
static_assert(std::is_aggregate_v<fi::TextFieldParams>);
static_assert(std::is_aggregate_v<fi::TextFieldResult>);
static_assert(std::is_same_v<fi::ListSelection, ImGuiSelectionBasicStorage>);

} // namespace

TEST_CASE("The scroll core interface links with its declared signatures")
{
    CHECK(Links<void(ImGuiWindow*, ImVec2, bool)>(&fi::ScrollAreaTo));
    CHECK(Links<void(ImGuiWindow*, const ImRect&, fi::ScrollAlign, bool)>(&fi::ScrollAreaRevealRect));
    CHECK(Links<ImVec2(ImGuiWindow*)>(&fi::ScrollAreaTarget));
    CHECK(Links<bool(ImGuiWindow*)>(&fi::IsScrollAreaScrolling));
    CHECK(Links<ImGuiWindow*(ImVec2)>(&fi::ScrollAreaAt));
    CHECK(Links<bool(ImGuiWindow*, ImVec2)>(&fi::RequestScrollAreaEdgeScroll));
}

TEST_CASE("The popup host interface links with its declared signatures")
{
    CHECK(Links<void(ImGuiID, const fi::PopupAnchor&, const fi::PopupOptions&)>(&fi::OpenPopupLevel));
    CHECK(Links<void(ImGuiID)>(&fi::ClosePopupLevel));
    CHECK(Links<bool(ImGuiID)>(&fi::IsPopupLevelOpen));
    CHECK(Links<bool(ImGuiID)>(&fi::BeginPopupLevel));
    CHECK(Links<void()>(&fi::EndPopupLevel));
    CHECK(Links<fi::PopupCloseReason(ImGuiID)>(&fi::PopupLevelClosedReason));
    CHECK(Links<ImGuiID()>(&fi::TopPopupLevel));
    CHECK(Links<void(ImGuiID, const fi::PopupAnchor&, const fi::PopupOptions&)>(&fi::ForcePopupLevelOpen));
    CHECK(Links<bool(ImGuiID, fi::EscapeLayer)>(&fi::ClaimEscape));
    CHECK(Links<bool(fi::ContextRequestTarget, ImVec2*)>(&fi::ContextMenuRequested));
    // The zero values are the defaults the widgets rely on.
    const fi::PopupAnchor anchor;
    CHECK(anchor.side == fi::PopupSide::Below);
    CHECK(anchor.align == fi::PopupAlign::Start);
    CHECK(fi::PopupOptions{}.kind == fi::PopupKind::Popup);
    CHECK(fi::PopupOptions{}.takeFocus);
    CHECK(fi::PopupOptions{}.closeOnEscape);
}

TEST_CASE("The drag and drop core interface links with its declared signatures")
{
    using Source = bool(const char*, const void*, std::size_t, const fi::DragDropPreview&);
    using Zone = fi::DragDropZoneResult(ImGuiID, const ImRect&, const char*, const fi::DragDropZoneOptions&);
    CHECK(Links<Source>(&fi::DragDropBeginSource));
    CHECK(Links<void()>(&fi::DragDropEndSource));
    CHECK(Links<bool(const char*, fi::DragDropInfo*)>(&fi::DragDropActive));
    CHECK(Links<Zone>(&fi::DragDropZone));
    CHECK(Links<int(std::span<const ImRect>, facet::Orientation, ImVec2)>(&fi::DragDropInsertIndex));
    CHECK(fi::DragDropZoneResult{}.insertIndex == -1);
    CHECK(fi::DragDropZoneOptions{}.axis == facet::Orientation::Vertical);
    // The keyboard pick-up is opt-in, with Space unless a source names another chord.
    CHECK_FALSE(fi::DragDropPreview{}.keyboard);
    CHECK(fi::DragDropPreview{}.pickUpChord == ImGuiKey_Space);
}

TEST_CASE("The appended members of the list view, the number core and the menu rows keep their defaults")
{
    CHECK(fi::ListViewDesc{}.highlight == nullptr);
    CHECK(fi::ListViewDesc{}.keyOwner == 0);
    CHECK_FALSE(fi::ListRowDesc{}.highlighted);
    CHECK(fi::ListRowDesc{}.selection == ImGuiSelectionUserData_Invalid);
    CHECK(fi::NumberFieldParams{}.scale == facet::ValueScale::Linear);
    CHECK(fi::NumberFieldParams{}.parser == nullptr);
    CHECK(fi::MenuRowDesc{}.submenu == 0);
    CHECK(Links<fi::MenuRowResult(ImGuiID, const char*, const fi::MenuRowDesc&)>(&fi::MenuRow));
    CHECK(Links<void(ImGuiID)>(&fi::MenuLevelBegin));
    CHECK(Links<bool()>(&fi::ScrollWheelBelongsToScroll));
    CHECK(Links<int(const fi::ListJumpDesc&, bool, bool*)>(&fi::ListJumpToEnd));
}

TEST_CASE("The list core interface links with its declared signatures")
{
    CHECK(Links<void(const fi::ListRowsDesc&)>(&fi::BeginListRows));
    CHECK(Links<bool(facet::RowRange*)>(&fi::NextListRows));
    CHECK(Links<void()>(&fi::EndListRows));
    CHECK(Links<void(fi::ListSelection*, int, const fi::ListSelectionOptions&)>(&fi::BeginListSelection));
    CHECK(Links<bool()>(&fi::EndListSelection));
    CHECK(Links<bool(fi::ListSelection*, int)>(&fi::IsListEntrySelected));
    CHECK(fi::ListRowsDesc{}.columns == 1);
    CHECK_FALSE(fi::ListSelectionOptions{}.grid);
    CHECK(fi::ListRowsDesc{}.keepAlive == -1);
}

TEST_CASE("The text field core interface links with its declared signatures")
{
    using StringField = fi::TextFieldResult(const char*, std::string*, const fi::TextFieldParams&);
    using BufferField = fi::TextFieldResult(const char*, char*, std::size_t, const fi::TextFieldParams&);
    CHECK(Links<StringField>(&fi::TextFieldBehavior));
    CHECK(Links<BufferField>(&fi::TextFieldBehavior));
    CHECK(Links<fi::PopupAnchor(const fi::TextFieldResult&, fi::TextFieldAnchor)>(&fi::TextFieldPopupAnchor));
    CHECK(fi::TextFieldParams{}.completionPopup == 0);
}
