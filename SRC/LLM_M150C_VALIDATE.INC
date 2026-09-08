#include <stddef.h>
#include <string.h>

#include "LLM_PROMPT_M310.INC"

int llm_m150c_has_create(void)
{
    return
        strstr(prompt_plan, "type=create_file") != NULL &&
        strstr(prompt_plan, "complete new file text") != NULL &&
        strstr(prompt_plan, "must not contain BEGIN_OLD_TEXT") != NULL;
}

int llm_m150c_guards_target(void)
{
    return
        strstr(prompt_plan, "shown not to exist") != NULL &&
        strstr(
            prompt_plan,
            "Never use create_file for a path that exists") != NULL &&
        strstr(
            prompt_plan,
            "Do not reuse a create_file path") != NULL;
}

int llm_m150c_keeps_replace(void)
{
    return
        strstr(prompt_plan, "type=replace_block") != NULL &&
        strstr(prompt_plan, "BEGIN_OLD_TEXT") != NULL &&
        strstr(prompt_plan, "END_OLD_TEXT") != NULL &&
        strstr(prompt_plan, "BEGIN_NEW_TEXT") != NULL &&
        strstr(prompt_plan, "END_NEW_TEXT") != NULL;
}

int llm_m150c_blocks_legacy(void)
{
    return
        strstr(
            prompt_plan,
            "Do not generate legacy type=replace_text") != NULL;
}

int llm_m310_prompt_create_chain(void)
{
    return
        strstr(
            prompt_plan,
            "create_file path may be reused by one or more later "
            "type=replace_block") != NULL &&
        strstr(
            prompt_plan,
            "never place a replace_block before its create_file") != NULL;
}

int llm_m310_prompt_build_policy(void)
{
    return
        strstr(prompt_plan, "build=required") != NULL &&
        strstr(prompt_plan, "build=skip") != NULL &&
        strstr(
            prompt_plan,
            "missing build= line is legacy-compatible") != NULL;
}
