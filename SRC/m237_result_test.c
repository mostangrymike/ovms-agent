#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llm_internal.h"

int command_line_complete(const char *a,size_t b,int c)
{(void)a;(void)b;(void)c;return 0;}
int command_read_stream(FILE *a,char *b,size_t c)
{(void)a;(void)b;(void)c;return 0;}

int main(void)
{
    char *result;
    char last[8192];
    char large[7000];
    size_t i;

    result=openai_result_make(
        "read_file","ok","read",1,
        "{\"path\":\"SRC/OPENAI.C\"}","sample output");
    if(result==NULL ||
       strstr(result,"tool: read_file")==NULL ||
       strstr(result,"status: ok")==NULL ||
       strstr(result,"code: 1")==NULL ||
       strstr(result,"effect: read")==NULL ||
       strstr(result,"truncated: no")==NULL ||
       strstr(result,"sample output")==NULL)
    {free(result);puts("M237 failed: normalized result.");return EXIT_FAILURE;}
    free(result);

    for(i=0U;i+1U<sizeof(large);++i)large[i]='X';
    large[sizeof(large)-1U]='\0';

    result=openai_result_make(
        "run_build","failure","execute",2,"{}",large);
    if(result==NULL ||
       strstr(result,"truncated: yes")==NULL ||
       strstr(result,"code: 2")==NULL)
    {free(result);puts("M237 failed: bounded result.");return EXIT_FAILURE;}
    free(result);

    if(!openai_result_last_text(last,sizeof(last)) ||
       strstr(last,"tool: run_build")==NULL ||
       strstr(last,"status: failure")==NULL)
    {puts("M237 failed: last result.");return EXIT_FAILURE;}

    if(!openai_parity_text(last,sizeof(last)) ||
       strstr(last,"Normalized results:     available")==NULL ||
       strstr(last,"Result persistence:     available")==NULL)
    {puts("M237 failed: parity.");return EXIT_FAILURE;}

    puts("Normalized autonomous tool-result regression passed.");
    return EXIT_SUCCESS;
}
