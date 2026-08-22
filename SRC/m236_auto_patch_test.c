#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"
#include "LLM_TOOL_SCHEMA.H"
#include "LLM_PATCH.H"
#include "LLM_AUTO.H"

int command_line_complete(const char *a,size_t b,int c)
{ (void)a;(void)b;(void)c;return 0; }
int command_read_stream(FILE *a,char *b,size_t c)
{ (void)a;(void)b;(void)c;return 0; }

static int wt(const char *p,const char *t)
{
    FILE *f=fopen(p,"w");
    if(f==NULL)return 0;
    if(fputs(t,f)==EOF){(void)fclose(f);return 0;}
    return fclose(f)==0;
}
static char *rt(const char *p)
{
    FILE *f=fopen(p,"r"); long n; char *t;
    if(f==NULL)return NULL;
    if(fseek(f,0L,SEEK_END)!=0||(n=ftell(f))<0L||fseek(f,0L,SEEK_SET)!=0)
    {(void)fclose(f);return NULL;}
    t=(char*)malloc((size_t)n+1U); if(t==NULL){(void)fclose(f);return NULL;}
    if(fread(t,1U,(size_t)n,f)!=(size_t)n){free(t);(void)fclose(f);return NULL;}
    t[n]='\0';(void)fclose(f);return t;
}

int main(void)
{
    char out[4096];
    char *text;
    FILE *schema_file;

    while(remove("M236_AUTO_TARGET.TMP")==0){}
    while(remove("M236_SCHEMA.TMP")==0){}

    schema_file=fopen("M236_SCHEMA.TMP","w");
    if(schema_file==NULL || !write_agent_tools_with_replace(schema_file) ||
       fclose(schema_file)!=0)
    { puts("M236 failed: write tool schema."); return EXIT_FAILURE; }

    text=rt("M236_SCHEMA.TMP");
    if(text==NULL || strstr(text,"structured_patch")==NULL ||
       strstr(text,"@@OLD")==NULL)
    { free(text);puts("M236 failed: structured tool schema.");return EXIT_FAILURE;}
    free(text);

    if(!wt("M236_AUTO_TARGET.TMP","one\nmiddle\nthree\n"))
    { puts("M236 failed: setup."); return EXIT_FAILURE; }

    (void)putenv("OVMS_AGENT_WRITE_ENABLED=YES");
    (void)putenv("OVMS_AGENT_APPROVAL_POLICY=WORKSPACE");

    if(!llm_patch_apply_json(
        "{\"path\":\"M236_AUTO_TARGET.TMP\","
        "\"patch\":\"@@OLD\\none\\n@@NEW\\nONE\\n@@END\\n"
        "@@OLD\\nthree\\n@@NEW\\nTHREE\\n@@END\\n\"}",
        out,sizeof(out)))
    { puts("M236 failed: autonomous structured patch."); return EXIT_FAILURE; }

    text=rt("M236_AUTO_TARGET.TMP");
    if(text==NULL || strcmp(text,"ONE\nmiddle\nTHREE\n")!=0)
    { free(text);puts("M236 failed: autonomous patch content.");return EXIT_FAILURE;}
    free(text);

    if(llm_patch_apply_json(
        "{\"path\":\"M236_AUTO_TARGET.TMP\","
        "\"patch\":\"@@OLD\\nmissing\\n@@NEW\\nX\\n@@END\\n\"}",
        out,sizeof(out)) ||
       strstr(out,"not found")==NULL)
    { puts("M236 failed: stale autonomous hunk."); return EXIT_FAILURE; }

    llm_auto_test_limits(12U,1U);
    llm_auto_begin(LLM_WORKFLOW_WRITE);
    if(!llm_auto_allow_write() || llm_auto_allow_write())
    { puts("M236 failed: one patch/write accounting."); return EXIT_FAILURE; }
    llm_auto_finish("test");
    llm_auto_test_limits(0U,0U);

    if(!llm_parity_text(out,sizeof(out)) ||
       strstr(out,"Autonomous multi-hunk:  available")==NULL)
    { puts("M236 failed: parity."); return EXIT_FAILURE; }

    while(remove("M236_AUTO_TARGET.TMP")==0){}
    while(remove("M236_SCHEMA.TMP")==0){}
    puts("Autonomous structured patch tool regression passed.");
    return EXIT_SUCCESS;
}
