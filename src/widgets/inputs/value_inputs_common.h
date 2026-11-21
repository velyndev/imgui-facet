#pragma once

// Helpers shared by the number-like inputs of inputs.h (NumberInput, DragNumber, UnitInput,
// VectorInput, SizeInput, HexInput, DurationInput, KeybindCapture, Rating). Not used by other
// families; the building blocks they sit on are in src/internal.
//
// Rejected text: a number field writes nothing when the committed text does not parse ("-",
// "1e", "12:x"). The widget then shows Strings::invalidValue as its error until the field is
// activated again or a value is written, so the user sees why nothing changed:
//
//     const char* error = ValueInputError(id, options.error);    // before the field call
//     const NumberFieldResult edit = NumberFieldBehavior(label, &value, {..., .error = error});
//     ValueInputTrackRejected(id, edit.parseFailed, edit.changed); // right after it
//
// The caller's `error` always wins over the rejected message.

#include <imgui.h>

namespace facet::internal
{

// The error a value input shows this frame: the caller's error, else Strings::invalidValue while
// a rejected commit of field `id` is pending, else null.
const char* ValueInputError(ImGuiID id, const char* callerError);

// After the field call (the field is the last item): a commit that did not parse marks `id`
// rejected; activating the field again or writing a value clears the mark.
void ValueInputTrackRejected(ImGuiID id, bool parseFailed, bool written);

// Sets or clears the rejected mark of `id` directly (widgets that parse their own text).
void ValueInputSetRejected(ImGuiID id, bool rejected);

// Pixels to the design units the blocks' width parameters take.
float ValueInputUnits(float pixels);

} // namespace facet::internal
