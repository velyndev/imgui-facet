// The scope stack every Facet Begin / End pair reports to (src/internal/scope_stack.h).

#include <cstring>

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/session.h"

using namespace facet::internal;

TEST_CASE("Balanced scopes nest and close without a report")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int errors = -1;
    int depthInside = 0;
    const char* innermost = nullptr;
    session.SetUi([&] {
        const int start = UsageErrorCount();
        PushScope("BeginCard");
        PushScope("BeginShell");
        depthInside = OpenScopeCount();
        innermost = InnermostScope();
        CHECK(PopScope("BeginShell"));
        CHECK(PopScope("BeginCard"));
        errors = UsageErrorCount() - start;
    });
    const int before = UsageErrorCount();
    session.Frame();
    CHECK(depthInside == 2);
    REQUIRE(innermost != nullptr);
    CHECK(std::strcmp(innermost, "BeginShell") == 0);
    CHECK(errors == 0);
    CHECK(UsageErrorCount() == before);
    CHECK(OpenScopeCount() == 0);
}

TEST_CASE("An End without its Begin is reported and changes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool popped = true;
    int depthAfter = -1;
    session.SetUi([&] {
        PushScope("BeginCard");
        popped = PopScope("BeginModal");
        depthAfter = OpenScopeCount();
        PopScope("BeginCard");
    });
    const int before = UsageErrorCount();
    session.Frame();
    CHECK_FALSE(popped);
    CHECK(depthAfter == 1);
    CHECK(UsageErrorCount() == before + 1);
}

TEST_CASE("An End closes the scopes left open inside it and reports each of them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        PushScope("BeginCard");
        PushScope("BeginSettingsRow");
        PushScope("BeginStack");
        CHECK(PopScope("BeginCard"));
        CHECK(OpenScopeCount() == 0);
    });
    const int before = UsageErrorCount();
    session.Frame();
    CHECK(UsageErrorCount() == before + 2);
}

TEST_CASE("EndFrame names every scope left open and starts the next frame empty")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        PushScope("BeginCard");
        PushScope("BeginShell");
    });
    const int before = UsageErrorCount();
    session.Frame();
    CHECK(UsageErrorCount() == before + 2);
    CHECK(OpenScopeCount() == 0);
    CHECK(InnermostScope() == nullptr);
}

TEST_CASE("Scopes deeper than the stack are counted and still balance")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int depthErrors = 0;
    session.SetUi([&] {
        const int start = UsageErrorCount();
        for (int i = 0; i < kScopeStackDepth + 4; ++i)
            PushScope("BeginStack");
        depthErrors = UsageErrorCount() - start;
        CHECK(OpenScopeCount() == kScopeStackDepth + 4);
        for (int i = 0; i < kScopeStackDepth + 4; ++i)
            CHECK(PopScope("BeginStack"));
    });
    const int before = UsageErrorCount();
    session.Frame();
    CHECK(depthErrors == 1);
    CHECK(UsageErrorCount() == before + 1);
    CHECK(OpenScopeCount() == 0);
}
