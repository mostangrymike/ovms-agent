#include <stdio.h>
#include <string.h>

#include "COMMAND_CHANGE_GATE.INC"

static int expect_input(const char *text, int expected)
{
    return m303_changeset_apply_input(text) == expected;
}

static int expect_policy(const char *name, int expected)
{
    return m303_change_policy_ok(name) == expected;
}

int main(void)
{
    int ok;

    ok =
        expect_input("CHANGESET APPLY", 1) &&
        expect_input("changeset apply", 1) &&
        expect_input("  CHANGESET   APPLY  ", 1) &&
        expect_input("CHANGESET SHOW", 0) &&
        expect_input("CHANGESET APPLY EXTRA", 0) &&
        expect_input("CHANGESET", 0) &&
        expect_input(NULL, 0) &&
        expect_policy("read-only", 0) &&
        expect_policy("workspace", 1) &&
        expect_policy("full", 1) &&
        expect_policy("autopilot", 1) &&
        expect_policy("dangerous", 0) &&
        expect_policy(NULL, 0);

    if (!ok) {
        (void)puts("M303 CHANGESET dispatch regression failed.");
        return 2;
    }

    (void)puts("M303 CHANGESET dispatch regression passed.");
    return 1;
}
