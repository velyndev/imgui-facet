# Security policy

## Supported versions

Facet is at version 0.1 and has no tagged release yet. Fixes go to the `main` branch.

## Reporting a vulnerability

Please do not open a public issue for a security problem.

Report it privately on GitHub: open the **Security** tab of this repository and choose
**Report a vulnerability**. If that button is not shown, open an issue that only says you have a
security report and leaves out every detail; you will be given a private way to send it.

A useful report has:

- the Facet commit and how Facet was built (fetched Dear ImGui or your own target);
- what an attacker has to control, for example an image file, an SVG, a font or the saved state;
- the smallest input or code that triggers the problem, and what happens.

The parts of Facet that read data from outside the program are the image and SVG loading
(`facet/core/image.h`, `facet/core/icons.h`), the fonts it loads from Windows, and the state it
reads back from Dear ImGui's ini file. The decoders it uses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Facet is maintained by one person, so there is no fixed response time. You get an answer once the
report has been read, and a fix is credited to you in the commit unless you ask otherwise.
