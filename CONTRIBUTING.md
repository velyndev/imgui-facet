# Contributing to Facet

Bug reports, fixes, new widgets and corrections to the documentation are welcome. This page says
what a report needs and what a change has to pass before it can be merged.

## Reporting a bug or asking for a widget

Open an issue with one of the forms. A bug report needs:

- the Facet commit, the Windows version and the Visual Studio (MSVC) version;
- whether Facet fetches Dear ImGui or builds against your own Dear ImGui target;
- the smallest code that shows the problem, or the page of the test app that shows it;
- what you expected and what happened instead, with a screenshot when it is about the look.

A security problem does not go into a public issue. See [SECURITY.md](SECURITY.md).

## Building

The requirements are in the [README](README.md#requirements).

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64-release
ctest --preset msvc-x64-release
```

`facet_demo`, the test app, is built with the library. Its capture and selftest modes are how a
change to a widget is checked:

```
build\msvc-x64\bin\Release\facet_demo.exe --capture out --page <id> --theme both --warp
build\msvc-x64\bin\Release\facet_demo.exe --selftest --page <id> --theme both --warp
```

`--list-pages` prints the page ids. [docs/demo.md](docs/demo.md) describes every run option.

## Before you open a pull request

- The build has no warnings. The workflow configures with `-DFACET_WARNINGS_AS_ERRORS=ON` and
  `-DFACET_BUILD_CONSUMER_TEST=ON`; do the same before you push.
- `ctest` passes.
- For a change that a page of the test app shows: its captures look right in both themes, and
  its selftest passes at `--size 1280x800 --scale 1` and at `--size 900x600 --scale 2`.
- The documentation changes in the same pull request as the code: the widget's entry on its
  family page under `docs/widgets/`, the family tables in `README.md` and `docs/widgets.md` when
  a status changes, and one line in `DESIGN.md` with the reason for every new visual choice.
- Dear ImGui stays unmodified, and the vendored files under `third_party/` are not edited.

## Rules for the code

- C++20, built with MSVC. Public declarations follow
  [the API conventions](docs/api-conventions.md); a widget is built, tested, shown and documented
  as [writing a widget](docs/writing-a-widget.md) describes.
- Widgets take colors, sizes and motion from the theme (`facet::T()`) and lengths in design
  units. Widget code holds no color or pixel literals.
- Every outline is one hairline wide (`metrics.hairline`). A state changes the color or the
  fill, never the width. `tests/test_border_rule.cpp` scans the sources for this.
- Every interactive widget works with the keyboard alone and shows the focus ring after keyboard
  input. Both themes have to work; `tests/test_contrast.cpp` holds the contrast checks.
- A new behaviour comes with a test in `tests/`, and a new widget with its section on the demo
  page of its family and a selftest step that operates it.
- Comments say what the code does and why.

## Commits

- [Conventional Commits](https://www.conventionalcommits.org/): `type(scope): subject`, for
  example `fix(sliders): keep the value bubble inside the window`.
- The subject is imperative and at most 71 characters. The body says why, when the subject does
  not make it obvious.
- One logical change per commit. The history of `main` is linear: rebase onto `main` instead of
  merging it into your branch.

## License

Facet is under the [MIT license](LICENSE). By contributing you agree that your contribution is
licensed under the same terms.
