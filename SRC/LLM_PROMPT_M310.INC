static const char prompt_plan_m310_rules[] =
        " M310 saved-plan execution rules supersede two earlier serialization "
        "sentences where they conflict. A create_file path may be reused by "
        "one or more later type=replace_block operations in the same plan, "
        "provided create_file appears first and each later replacement matches "
        "the content produced by the preceding operation. Never emit a second "
        "create_file for that path, never place a replace_block before its "
        "create_file, and do not reuse a newly created path for delete, rename, "
        "or move in the same plan. M310 also supersedes the instruction that "
        "operation_count must be the first machine-readable line. For every "
        "executable plan, immediately before operation_count emit exactly one "
        "standalone line build=required or build=skip. Use build=skip only when "
        "the user explicitly requires that no build run or the requested "
        "validation contract explicitly excludes a build; otherwise emit "
        "build=required. Do not emit build= for advisory-only plans. Never put "
        "build= text in narrative sections or OLD/NEW payloads unless it is "
        "literal source content protected by the existing payload framing "
        "rules. A missing build= line is legacy-compatible and is interpreted "
        "as build=required by the executor.";

static const char *llm_m310_prompt_plan(void)
{
    static char combined[
        sizeof(prompt_plan) +
        sizeof(prompt_plan_m310_rules) + 1U];
    static int initialized = 0;

    if (!initialized) {
        (void)strcpy(combined, prompt_plan);
        (void)strcat(combined, prompt_plan_m310_rules);
        initialized = 1;
    }

    return combined;
}

#define prompt_plan llm_m310_prompt_plan()
