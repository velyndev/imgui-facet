# Writing a widget

How a widget of `include/facet/widgets/` is built, tested, shown and documented. The binding rules
for its declaration are [the API conventions](api-conventions.md); this guide covers the
implementation. `src/widgets/buttons/button.cpp` (Button) is the model to copy, and
`src/widgets/display/progress_bar.cpp` (ProgressBar) shows a widget that is not interactive.

## Where the files are

| What | Where |
|---|---|
| The widget | `src/widgets/<family>/<widget>.cpp`, one file per widget, already present as a placeholder |
| Helpers shared by widgets of one family | `src/widgets/<family>/`, for example `buttons/button_look.h` |
| Building blocks shared by every family | `src/internal/` (each header starts with a usage sketch) |
| Per-frame or per-device work of a family | the accessor in `src/widgets/<family>/<family>_frame.cpp` (the subsystem table in `src/core/subsystems.cpp` already lists it) |
| API and placeholder checks | `tests/test_api_<family>.cpp` |
| Behaviour tests | `tests/test_<family>.cpp` |
| Demo page with its selftest | `demo/pages/page_w_<family>.cpp` |
| Documentation entry | `docs/widgets/<family>.md`; the reason for every new visual choice in `DESIGN.md` |

The inputs family is split in two: the text-like inputs have `text_inputs_frame.cpp`,
`test_api_inputs_text.cpp`, `test_inputs_text.cpp` and `page_w_inputs_text.cpp`; the number-like
ones have `value_inputs_frame.cpp`, `test_api_inputs_value.cpp`, `test_inputs_value.cpp` and
`page_w_inputs_value.cpp`. New source files are picked up by the build without a CMake edit.

## The structure of a widget file

A widget does the same five things in the same order:

1. **Entry.** `internal::ControlWindow()` returns the window to submit to, or null outside a frame
   or in a window whose items are skipped; then return the neutral result. Treat a null label as
   `""`. Take the next-item data with `internal::TakeNextItemData()` before the first item, because
   `ItemAdd` clears it. A widget made of several items also takes the forced capture state once
   with `internal::TakeForcedItemState()` and passes it to each part.
2. **Measure.** `internal::GetControlMetrics(options.size)` gives the height, paddings, icon gap
   and radius of the size; `internal::ControlTextScope` pushes the control text style and gives
   the font and icon size. Measure the content, then resolve the width with
   `internal::ResolveItemWidth(options.width, next, default)`: compact controls default to their
   content, field widgets to `kFill`.
3. **Items.** Put the next-item data back with `internal::ApplyNextItemData(next)` and run
   `internal::ItemBehavior` (or `PartBehavior` for the parts of a widget that laid out its space).
   It handles hover, press, keyboard activation, focus, the disabled flag, the hit target, the
   cursors and the forced capture states. A widget made of several items wraps them in
   `ImGui::BeginGroup` / `EndGroup` and calls `internal::FinishWidgetGroup(id)`.
4. **Draw** from tokens only: colors from `facet::T().colors` through the helpers below, every
   length through `facet::Px()`, motion from the animation store, then the focus ring.
5. **After the item.** The tooltip of a cut label (`internal::CutTextTooltip`), the message line
   under the control, then the result.

Names a widget file adds in `facet::internal` carry the widget or family name (`ButtonLook`, not
`Look`), because every family links into the same library.

## The helpers for each part

Every part that controls of different families share has one helper. Use it instead of drawing
the part yourself; if it does not fit, ask for the helper to be extended rather than writing a
second one.

| Part | Helper |
|---|---|
| Control sizes, paddings, radius | `GetControlMetrics`, `ControlTextScope` (`internal/control.h`); `ControlHeight`, `ControlTextStyle`, `PushControlSize` (`widgets/common.h`) |
| Width rule | `ResolveItemWidth` (`internal/item.h`) |
| Hover, press, focus, disabled, read-only, mixed, forced states | `ItemBehavior`, `PartBehavior`, `ItemOptions` (`internal/item.h`) |
| Label in front of a field or one-line control | `MeasureFrontLabel`, `DrawFrontLabel` (`internal/control.h`) |
| Label above a group or tall control | `MeasureAboveLabel`, `DrawAboveLabel` |
| Label after a mark, any text cut to a width | `FitText` |
| Full text of a cut label on hover and focus | `CutTextTooltip`; other tooltips `BeginPopupTooltip` (`internal/popup_host.h`) |
| Fill feedback for hover and press | `StateFill`, `NeutralFill` |
| Text and glyph colors, disabled included | `ControlLabelColor`, `ControlSecondaryColor` |
| Frame of a field-like control | `DrawFieldFrame`; text editing through `TextFieldBehavior` (`internal/text_field.h`), numbers through `internal/text_field_number.h` |
| Icons | `ControlTextScope::IconSize`, `DrawControlIcon`; icons by name, `Icon("chevron-down")` |
| Loading state | `DrawControlSpinner`; skeletons and the shared loading clock in `internal/loading.h` |
| Help and error text under a control | `PickControlMessage`, `ControlMessageHeight`, `DrawControlMessage` (an error is also recorded for `ValidationSummary`) |
| Focus ring | `DrawItemFocusRing`, `DrawFocusRing` (`internal/focus.h`) |
| Motion | `Tween`, `Spring`, `Smooth` (`core/anim.h`) under the widget id with sub keys below `kItemAnimSubBase`; the item behaviour's `hoverAmount`, `pressAmount`, `focusAmount` |
| State kept across frames | `GetState`, `GetPersistentState` (`internal/state_storage.h`) |
| Selection highlight | `SelectionPill`, `PushPillLabelStyle` (`internal/selection_pill.h`) |
| Items that move into a "more" menu | `FitOverflow` (`internal/overflow.h`) |
| Dragging a value, several handles | `internal/value_drag.h`, `internal/multi_handle.h` |
| Popups, menus, Escape and context menu keys | `internal/popup_host.h`, `internal/popup_menu.h`, `internal/popup_place.h` |
| Lists, rows, selection, typeahead | `internal/list_core.h`, `internal/list_view.h` |
| Scrolling | `internal/scroll.h` |
| Drag and drop | `internal/drag_drop.h` |
| Labelled rows | `internal/row_layout.h` |
| Fuzzy matching, key chords, charts | `internal/fuzzy.h`, `internal/keys.h`, `internal/chart_core.h` |

The theme is fixed for widget work: every token a widget of this stage needs exists in
`include/facet/core/theme.h`, with its reason in `DESIGN.md`. A widget never uses a color or pixel
literal; a factor that has no token (the share of a spinner stroke) is a named constant with a
comment.

## States and keys

Before a widget's status becomes ready it covers, where its declaration has them:

- **Looks:** rest, hovered, pressed, focused (the focus ring only after keyboard input), disabled
  (`ImGui::BeginDisabled`: the disabled colors, the not-allowed cursor, not a Tab stop, returns
  false), read-only (shown, focusable, never written), mixed, loading (size kept, input ignored),
  error and help text, and for widgets with data the empty, loading and error states.
- **Keys:** reached with Tab and Shift+Tab in visual order; Space and Enter activate; the arrows,
  Home, End, Page Up and Page Down where the widget moves a value or a selection; Escape cancels
  an edit or closes the topmost popup and nothing more (rule 8).
- **Layout:** both themes, scale 1 and 2, a narrow width (labels cut with their tooltip, nothing
  leaves its row), `ImGui::SameLine`, settings rows and table cells.
- **Interplay:** the item queries after the call describe the whole widget, every Dear ImGui and
  Facet stack is as before, nothing is allocated per frame once the widget is shown, and a call
  outside a frame or with a null value pointer gives the neutral result.

## Tests

`tests/test_<family>.cpp` holds one test case per behaviour, each with its own
`facet::test::Session` (`tests/support/session.h`), so it passes alone (`facet_tests
-tc="name"`) and in the full suite:

```cpp
TEST_CASE("A focused button activates with Space and Enter and is a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::Button("Apply"))
            ++presses;
        session.Track("apply");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);     // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("apply"));
    session.KeyPress(ImGuiKey_Space);
    CHECK(presses == 1);
}
```

A test that changes the theme, the accent or the motion settings restores them. Colors are
checked by counting the draw list vertices of a token color; geometry with `session.ItemRect`.

When a widget stops being a placeholder, take its calls out of the placeholder list in
`tests/test_api_<family>.cpp` (and lower the call count there), and move any placeholder test that
used it to a placeholder of the same family that is still planned.

## The demo page

Each family page calls every widget of the family once, grouped into sections
(`facet::demo::SectionHeader`, `Paragraph`, `LabeledRow`, `Note` in `demo/gallery.h`). A ready
widget is shown in each variant, size and state its declaration has, and it really works: every
press changes something the page shows. Content is neutral and honest: no invented people,
numbers or product names; placeholders are labelled as placeholders.

- `ctx.Probe("button.save")` right after a widget records it for the selftest; probe names start
  with the family.
- Capture states are named in `FACET_DEMO_PAGE(...)`. For a state, call
  `ctx.ForceNextItemState(ForcedItemState::Hovered)` right before the widget (`Pressed`,
  `Focused`, `Dragging`, `Open`; a part index for widgets with several items), and
  `ctx.RevealInCapture()` after content that a state changes below the first screen.
- One `FACET_DEMO_SELFTEST("<page id>")` per page. It resets the page state with `test.Do`, then
  operates every ready widget with the mouse and the keyboard and checks the results
  (`Expect`, `ExpectNear`, `ExpectFocused`, `ExpectVisible`). Its log is the click-through
  evidence for a widget.

Run `facet_demo --capture <dir> --page <id> --warp --theme both` and look at the images in both
themes, and `facet_demo --selftest --page <id> --warp --theme both` at `--size 1280x800 --scale 1`
and at `--size 900x600 --scale 2`.

## The documentation entry

The entry of a widget on its family page is a `### Name` heading, the declarations in a `cpp`
block, a short description of what it draws and does (variants, sizes, keys, states, width), and
as its last sentence its status: `Status: planned.` or `Status: ready.` Nothing else changes when
a widget becomes ready: `docs/widgets.md` and the note at the top of each demo family page point
to the family pages for the status. Every visual choice that is new gets a one-line reason in `DESIGN.md`.
