int llm_m189_scope_prose(void)
{
    plan_scope_file files[LLM_PLAN_MAX_FILES];
    unsigned int count;

    if (!plan_collect_paths(
            "Goal\n"
            "Files to modify\n"
            "None in the current repository.\n"
            "The future sandbox path M189_NEVER/FILE.MD does not exist here.\n"
            "Files to create\n"
            "No files in the current repository.\n"
            "Ordered edits\n"
            "Inspect only.\n",
            files,
            &count)) {
        return 0;
    }

    return count == 0U;
}

int llm_m189_missing_modify(void)
{
    plan_scope_file files[LLM_PLAN_MAX_FILES];
    unsigned int count;

    return !plan_collect_paths(
        "Goal\n"
        "Files to modify\n"
        "M189_NEVER/MISSING.MD\n"
        "Files to create\n"
        "None.\n"
        "Ordered edits\n"
        "Inspect only.\n",
        files,
        &count);
}

int llm_m189_explicit_scope(void)
{
    plan_scope_file files[LLM_PLAN_MAX_FILES];
    unsigned int count;

    if (!plan_collect_paths(
            "Goal\n"
            "Files to modify\n"
            "- `BUILD.COM`\n"
            "Files to create\n"
            "- `M189_NEVER/NEW.MD`\n"
            "Ordered edits\n"
            "Apply bounded changes.\n",
            files,
            &count)) {
        return 0;
    }

    return count == 2U &&
           strcmp(files[0].path, "BUILD.COM") == 0 &&
           files[0].expect_missing == 0 &&
           strcmp(files[1].path, "M189_NEVER/NEW.MD") == 0 &&
           files[1].expect_missing == 1;
}
