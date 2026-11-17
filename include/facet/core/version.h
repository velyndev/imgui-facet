#pragma once

// Library version and the deprecation marker.
//
// The three numbers below are the only place the version is written down: CMakeLists.txt
// reads them for project(VERSION), FACET_VERSION is built from them, and facet::Version()
// (context.h) returns FACET_VERSION.

#define FACET_VERSION_MAJOR 0
#define FACET_VERSION_MINOR 1
#define FACET_VERSION_PATCH 0

#define FACET_VERSION_STRINGIFY_INNER(x) #x
#define FACET_VERSION_STRINGIFY(x) FACET_VERSION_STRINGIFY_INNER(x)

// "major.minor.patch".
#define FACET_VERSION                                                                              \
    FACET_VERSION_STRINGIFY(FACET_VERSION_MAJOR)                                                   \
    "." FACET_VERSION_STRINGIFY(FACET_VERSION_MINOR) "." FACET_VERSION_STRINGIFY(FACET_VERSION_PATCH)

// major * 10000 + minor * 100 + patch, for #if checks.
#define FACET_VERSION_NUM (FACET_VERSION_MAJOR * 10000 + FACET_VERSION_MINOR * 100 + FACET_VERSION_PATCH)

// Marks a declaration that is kept for compatibility. It stays for at least one minor release
// and is removed in the next major one. Defining FACET_DISABLE_DEPRECATED before including
// Facet hides such declarations early: headers wrap them in #ifndef FACET_DISABLE_DEPRECATED.
#define FACET_DEPRECATED(msg) [[deprecated(msg)]]
