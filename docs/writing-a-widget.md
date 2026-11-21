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

Some families are split into groups that are built apart, each with files of its own. New source
files are picked up by the build without a CMake edit.

| Group | Frame hook | API tests | Behaviour tests | Demo page (id) |
|---|---|---|---|---|
| Text inputs | `inputs/text_inputs_frame.cpp` | `test_api_inputs_text.cpp` | `test_inputs_text.cpp` | `page_w_inputs_text.cpp` (`inputs-text`) |
| Value inputs | `inputs/value_inputs_frame.cpp` | `test_api_inputs_value.cpp` | `test_inputs_value.cpp` | `page_w_inputs_value.cpp` (`inputs-value`) |
| Color pickers | `pickers/color_pickers_frame.cpp` | `test_api_pickers_color.cpp` | `test_pickers_color.cpp` | `page_w_pickers_color.cpp` (`pickers-color`) |
| Date, time and other pickers | `pickers/date_pickers_frame.cpp` | `test_api_pickers_date.cpp` | `test_pickers_date.cpp` | `page_w_pickers_date.cpp` (`pickers-date`) |
| Display without DataTable | `display/display_frame.cpp` | `test_api_display.cpp` | `test_display.cpp` | `page_w_display.cpp` (`display`) |
| DataTable | `display/data_table_frame.cpp` | `test_api_data_table.cpp` | `test_data_table.cpp` | `page_w_data_table.cpp` (`data-table`) |

The layout family keeps its behaviour tests in `test_layout_widgets.cpp`, because
`test_layout.cpp` tests the core layout of `core/layout.h`. The placeholder helpers themselves are
tested in `test_api_controls.cpp` and `test_api_structure.cpp`; a widget never needs an edit
there.

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
| Confirmation after a copy | `CopyFeedbackHold`, `CopyFeedbackActive` (`internal/copy_feedback.h`): `motion.feedbackHold` times `motion.scale`, the same for every widget that copies |
| Check mark of a chosen row (menus, lists, pickers) | `ChoiceCheckColor` (`internal/popup_menu.h`): accentText, labelTertiary when disabled |
| Key caps of a chord | `KeyCapNames`, `KeyCapLook` (`internal/key_caps.h`) |
| Checkerboard behind translucent colors and images | `FillCheckerboard` (`core/draw.h`) with `colors.checker` and `metrics.checkerCell` |
| Help and error text under a control | `PickControlMessage`, `ControlMessageHeight`, `DrawControlMessage` (an error is also recorded for `ValidationSummary`) |
| Focus ring | `DrawItemFocusRing`, `DrawFocusRing` (`internal/focus.h`) |
| Motion | `Tween`, `Spring`, `Smooth` (`core/anim.h`) under the widget id with sub keys below `kItemAnimSubBase`; the item behaviour's `hoverAmount`, `pressAmount`, `focusAmount` |
| State kept across frames | `GetState`, `GetPersistentState` (`internal/state_storage.h`) |
| Selection highlight | `SelectionPill`, `PushPillLabelStyle` (`internal/selection_pill.h`) |
| Items that move into a "more" menu | `FitOverflow` (`internal/overflow.h`) |
| Dragging a value, several handles, a point on two axes | `internal/value_drag.h` (`ValueDragBehavior`, `ValueDrag2DBehavior`), `internal/multi_handle.h` |
| Popups, menus, Escape and context menu keys | `internal/popup_host.h`, `internal/popup_menu.h`, `internal/popup_place.h` |
| Lists, rows, selection, typeahead | `internal/list_core.h`, `internal/list_view.h`; `ListTabProxy` and `ListClaimTabExit` for a container that places its own rows in a selection scope |
| Scrolling | `internal/scroll.h`; `BeginScrollAreaBars` gives the overlay bars to a window another call begins (a Dear ImGui table) |
| Drag and drop | `internal/drag_drop.h` |
| Labelled rows | `internal/row_layout.h` |
| Fuzzy matching, key chords, charts | `internal/fuzzy.h`, `internal/keys.h`, `internal/chart_core.h` |

The theme is fixed for widget work: every token a widget of this stage needs exists in
`include/facet/core/theme.h`, with its reason in `DESIGN.md`. A widget never uses a color or pixel
literal; a factor that has no token (the share of a spinner stroke) is a named constant with a
comment. In the same way, built-in text comes from a `Strings` member (`widgets/common.h`), and the
members the widgets of a stage name exist before the stage starts.

A part that a helper above does not cover yet, or a token or string that is missing, is asked
for, not added: shared headers (`src/internal/`, `include/facet/core/`, `widgets/common.h`) change
only additively, through the engineer who merges the stage, so two widgets never grow two versions
of one part.

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

When a widget stops being a placeholder, take its calls out of the placeholder checks in
`tests/test_api_<family>.cpp`: the `FACET_CHECK_STUB` list and its call count
(`support/controls_stub_check.h`), the `CHECK_BALANCED` calls (`support/structure_stub_check.h`)
or the `Check` calls (`support/content_stub_check.h`), whichever the file uses. A placeholder test
that used it moves to a placeholder of the same family that is still planned, or is deleted when
it only checked placeholder behaviour; the checks of the options structs stay. Its behaviour tests
go into the behaviour file of its group.

## The demo page

Each family page calls every widget of the family once, grouped into sections
(`facet::demo::SectionHeader`, `Paragraph`, `LabeledRow`, `Note`, `NoteBeside` in
`demo/gallery.h`). A ready widget is shown in each variant, size and state its declaration has,
and it really works: every press changes something the page shows. Content is neutral and honest:
no invented people, numbers or product names; placeholders are labelled as placeholders. The page
settings (a layer that shows or hides, a data state, a value) are Facet controls too; a stock Dear
ImGui control appears only where the page shows Facet working with stock items, and its row label
says so.

- A set width goes through `DemoWidth(units)`, which clamps it to the room left in the row, so the
  page holds at scale 2 in a small window.
- Controls of different sizes on one line go through `MixedHeightRow`: construct it with the height
  of the tallest control, call `Item(height)` before each control. Pass the same height to the row
  label, `LabeledRow(label, lineHeight)`, so the label is centered on that line too.
  `LastLabeledRowBox()` gives the box the label was centered in, for a selftest check.
- Wrapping rows of buttons use `ContentRight()` and `SameLineIfFits`.
- The note under the page header is `PlannedFamilyNote(family)` while any widget on the page is
  planned and `ReadyFamilyNote(family)` once all of them are ready.

- `ctx.Probe("button.save")` right after a widget records it for the selftest; probe names start
  with the family.
- Capture states are named in `FACET_DEMO_PAGE(...)`. For a state, call
  `ctx.ForceNextItemState(ForcedItemState::Hovered)` right before the widget (`Pressed`,
  `Focused`, `Dragging`, `Open`; a part index for widgets with several items), and
  `ctx.RevealInCapture()` after content that a state changes below the first screen.
- One `FACET_DEMO_SELFTEST("<page id>")` per page. It resets the page state with `test.Do`, then
  operates every ready widget with the mouse and the keyboard and checks the results
  (`Expect`, `ExpectNear`, `ExpectFocused`, `ExpectVisible`). Its log is the click-through
  evidence for a widget. A page without a script fails `--selftest --page <id>`, so a page of
  placeholders checks at least that they write nothing.
- Positions in a script come from probes of the latest frame. A popup row is found from the level
  rect (`internal::PopupLevelRect`) recorded with `ProbeRect`; before aiming at one, the lists
  page reveals the field (`test.Reveal`) and waits (`test.WaitSeconds(0.4f)`), so the field is
  away from the window edge and nothing moves between the probe and the click.

Run `facet_demo --capture <dir> --page <id> --warp --theme both` and look at the images in both
themes, and `facet_demo --selftest --page <id> --warp --theme both` at `--size 1280x800 --scale 1`
and at `--size 900x600 --scale 2`.

## The documentation entry

The entry of a widget on its family page is a `### Name` heading, the declarations in a `cpp`
block, a short description of what it draws and does (variants, sizes, keys, states, width), and
as its last sentence its status: `Status: planned.` or `Status: ready.` When the last widget of a
page becomes ready, the page switches its note from `PlannedFamilyNote` to `ReadyFamilyNote`
(`demo/gallery.h`).

Three files describe every family at once: the lists of what is ready in the status section of
`README.md` and in `docs/widgets.md`, and the one-line reason for every new visual choice in
`DESIGN.md`. Working alone, update them with the widget. When several groups are built at the same
time, none of them edits these files: each lists its lines (the widgets that became ready, each
new look with its reason) in its report, and the engineer who merges the work writes them once.
The same holds for a page shared by two groups (`docs/widgets/inputs.md`, `pickers.md`,
`display.md`): each group edits only its own entries and its own usage section.
