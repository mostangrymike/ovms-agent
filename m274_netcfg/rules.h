/*
 * M274 Phase 2 Rule Engine API (DEC C / OpenVMS C89)
 *
 * This header defines a first-pass rule engine interface supporting:
 *  - require-substring and forbid-substring rules
 *  - warning and error severities
 *  - bounded, fixed-capacity rule context
 *  - rule results collection
 *  - loading built-in defaults or an optional rules text file
 *  - checking a configuration file
 *  - releasing/resetting rule state
 *
 * Phase 3 features (e.g., scoring) are intentionally omitted.
 */

#ifndef RULES_H
#define RULES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Capacity and size limits (compile-time bounded context). */
#ifndef RULES_MAX_RULES
#define RULES_MAX_RULES           64   /* maximum number of rules */
#endif
#ifndef RULES_MAX_SUBSTR
#define RULES_MAX_SUBSTR         128   /* maximum substring length, incl. NUL */
#endif
#ifndef RULES_MAX_RESULTS
#define RULES_MAX_RESULTS        256   /* maximum number of results per check */
#endif
#ifndef RULES_MAX_PATH
#define RULES_MAX_PATH           255   /* max path length used internally */
#endif

/* Status codes (returned by functions). */
#define RULES_OK                    0
#define RULES_TRUNC                 1   /* operation succeeded but data was truncated */
#define RULES_ERR_PARAM           (-1)
#define RULES_ERR_FULL            (-2)
#define RULES_ERR_IO              (-3)
#define RULES_ERR_PARSE           (-4)
#define RULES_ERR_STATE           (-5)

/* Rule severity. */
enum rule_severity_e {
    RULE_SEV_WARN  = 1,
    RULE_SEV_ERROR = 2
};

/* Rule type. */
enum rule_type_e {
    RULE_TYPE_REQUIRE = 1,  /* require substring to appear */
    RULE_TYPE_FORBID  = 2   /* forbid substring from appearing */
};

/* One rule definition (bounded substring). */
struct rule_def {
    unsigned short in_use;             /* 0 unused, nonzero used */
    unsigned char  type;               /* enum rule_type_e */
    unsigned char  sev;                /* enum rule_severity_e */
    char           substr[RULES_MAX_SUBSTR]; /* NUL-terminated */
};

/* One rule match result (bounded match buffer). */
struct rule_result {
    unsigned short rule_index;         /* index into ctx->rules */
    unsigned char  type;               /* enum rule_type_e */
    unsigned char  sev;                /* enum rule_severity_e */
    unsigned long  line_no;            /* 1-based line number of match */
    unsigned long  col_no;             /* 1-based column number of match */
    char           match[RULES_MAX_SUBSTR]; /* matched substring or context */
};

/* Rule engine context (bounded, no dynamic allocation required). */
struct rule_ctx {
    unsigned int   rule_count;                 /* number of active rules */
    struct rule_def rules[RULES_MAX_RULES];    /* rule storage */
    unsigned long  flags;                      /* reserved for future use */
    char           last_error[128];            /* last error text (optional) */
};

/*
 * Initialize context and load built-in default rules.
 * Returns RULES_OK or error code.
 */
int rules_ctx_init_def(struct rule_ctx *ctx);

/*
 * Reset context to empty state (no rules, clears errors/flags).
 * Safe to call multiple times. No I/O performed.
 */
void rules_ctx_reset(struct rule_ctx *ctx);

/*
 * Free any state associated with the context.
 * For this bounded design, behaves like rules_ctx_reset.
 */
void rules_ctx_free(struct rule_ctx *ctx);

/*
 * Add one rule programmatically. The substring is copied and may be truncated
 * to RULES_MAX_SUBSTR-1. Returns RULES_OK, RULES_TRUNC, or an error code.
 */
int rules_add_rule(struct rule_ctx *ctx,
                   unsigned char type,
                   unsigned char sev,
                   const char *substr);

/*
 * Load rules from a text file at path. If replace_existing is nonzero, any
 * existing rules in ctx are cleared first; otherwise, rules are appended up to
 * RULES_MAX_RULES. Returns RULES_OK, RULES_TRUNC (if capacity reached or a
 * substring truncated), or an error code.
 *
 * Expected file format (first pass, line-oriented):
 *   require,warn,<substring>
 *   require,error,<substring>
 *   forbid,warn,<substring>
 *   forbid,error,<substring>
 * Lines beginning with '#' are treated as comments. Empty lines are ignored.
 */
int rules_load_text(struct rule_ctx *ctx,
                    const char *path,
                    int replace_existing);

/*
 * Check a configuration file at cfg_path against the loaded rules. The caller
 * supplies a results buffer 'resv' with capacity indicated by *resn on entry;
 * on successful return, *resn is set to the number of results written (which
 * may be zero). Returns RULES_OK, RULES_TRUNC (if result capacity was reached),
 * or an error code.
 */
int rules_check_config(struct rule_ctx *ctx,
                       const char *cfg_path,
                       struct rule_result *resv,
                       unsigned int *resn);

/* Retrieve last error message text (may be empty string). */
const char *rules_last_error(const struct rule_ctx *ctx);

#ifdef __cplusplus
}
#endif

 void report_risk_score(
    unsigned int error_count,
    unsigned int warn_count);
#endif /* RULES_H */
