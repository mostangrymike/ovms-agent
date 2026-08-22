#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"
#include "LLM_PATCH.H"

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
    char out[2048]; char *t;
    while(remove("M235_PATCH_TARGET.TMP")==0){}
    while(remove("M235_PATCH_SPEC.TMP")==0){}
    while(remove("M235_BAD_PATCH.TMP")==0){}

    if(!wt("M235_PATCH_TARGET.TMP","alpha\nmiddle\nomega\n") ||
       !wt("M235_PATCH_SPEC.TMP",
          "TARGET M235_PATCH_TARGET.TMP\n"
          "@@OLD\nalpha\n@@NEW\nALPHA\n@@END\n"
          "@@OLD\nomega\n@@NEW\nOMEGA\n@@END\n"))
    { puts("M235 failed: setup."); return EXIT_FAILURE; }

    if(!llm_patch_validate("M235_PATCH_SPEC.TMP",out,sizeof(out)) ||
       strstr(out,"2 hunks")==NULL)
    { puts("M235 failed: validate."); return EXIT_FAILURE; }

    t=rt("M235_PATCH_TARGET.TMP");
    if(t==NULL||strcmp(t,"alpha\nmiddle\nomega\n")!=0)
    { free(t);puts("M235 failed: validate modified file.");return EXIT_FAILURE;}
    free(t);

    if(!llm_patch_dry("M235_PATCH_SPEC.TMP",out,sizeof(out)))
    { puts("M235 failed: dry."); return EXIT_FAILURE; }

    (void)putenv("OVMS_AGENT_WRITE_ENABLED=YES");
    (void)putenv("OVMS_AGENT_APPROVAL_POLICY=WORKSPACE");
    if(!llm_patch_apply("M235_PATCH_SPEC.TMP",out,sizeof(out)))
    { puts("M235 failed: apply."); return EXIT_FAILURE; }

    t=rt("M235_PATCH_TARGET.TMP");
    if(t==NULL||strcmp(t,"ALPHA\nmiddle\nOMEGA\n")!=0)
    { free(t);puts("M235 failed: content.");return EXIT_FAILURE;}
    free(t);

    if(!wt("M235_BAD_PATCH.TMP",
          "TARGET M235_PATCH_TARGET.TMP\n"
          "@@OLD\nmissing\n@@NEW\nx\n@@END\n"))
    { puts("M235 failed: bad spec setup."); return EXIT_FAILURE; }

    if(llm_patch_validate("M235_BAD_PATCH.TMP",out,sizeof(out)) ||
       strstr(out,"not found")==NULL)
    { puts("M235 failed: stale rejection."); return EXIT_FAILURE; }

    if(!llm_parity_text(out,sizeof(out)) ||
       strstr(out,"Multi-hunk patching:   available")==NULL ||
       strstr(out,"Patch prevalidation:   available")==NULL)
    { puts("M235 failed: parity."); return EXIT_FAILURE; }

    while(remove("M235_PATCH_TARGET.TMP")==0){}
    while(remove("M235_PATCH_SPEC.TMP")==0){}
    while(remove("M235_BAD_PATCH.TMP")==0){}
    puts("Structured multi-hunk patch regression passed.");
    return EXIT_SUCCESS;
}
