#ifndef OVMS_AGENT_SYMBOL_INDEX_H
#define OVMS_AGENT_SYMBOL_INDEX_H

#include "agent.h"

#define SYMBOL_INDEX_FILE "OVMS_AGENT_SYMBOLS.DAT"

void symbol_reindex(agent_state *state);
void symbol_index_status(agent_state *state);
void symbol_index_check(agent_state *state);
void symbol_where(agent_state *state, const char *symbol);
void symbol_callers(agent_state *state, const char *symbol);
void symbol_show(agent_state *state, const char *symbol);
void symbol_show_function(agent_state *state,
                          const char *symbol);

/*
 * COMMAND_SYMBOL.C includes command_internal.h before this header.  Keep the
 * large legacy index translation unit untouched, but route the command-side
 * SHOW/STRUCT call through the stricter M279 classifier.
 */
#ifdef OVMS_AGENT_COMMAND_INTERNAL_H
#include "SYMBOL_SHOW_M279.INC"
#define symbol_show_struct m279_show_struct
#else
void symbol_show_struct(agent_state *state,
                        const char *symbol);
#endif

void symbol_show_module(agent_state *state,
                        const char *module);
void symbol_show_dependencies(agent_state *state,
                              const char *module);
void symbol_who_uses(agent_state *state,
                     const char *symbol);
void symbol_module_graph(agent_state *state,
                         const char *module);
void symbol_module_graph_all(agent_state *state);
void symbol_impact(agent_state *state,
                   const char *symbol);
void symbol_module_reverse(agent_state *state,
                           const char *module);
void symbol_path_find(agent_state *state,
                      const char *source,
                      const char *target);
void symbol_cluster(agent_state *state);
void symbol_cycle_all(agent_state *state);
void symbol_cycle_module(agent_state *state,
                         const char *module);
void symbol_architecture(agent_state *state);
void symbol_unused(agent_state *state,
                   const char *symbol);
void symbol_unused_detail(agent_state *state,
                          const char *symbol);
void symbol_rename_preview(agent_state *state,
                           const char *old_name,
                           const char *new_name);
int symbol_rename_apply(agent_state *state,
                        const char *old_name,
                        const char *new_name);
int symbol_rename_verify(agent_state *state,
                         const char *old_name,
                         const char *new_name);
void symbol_extract_function_preview(agent_state *state,
                                     const char *module,
                                     unsigned long start_line,
                                     unsigned long end_line,
                                     const char *new_name);
int symbol_extract_function_apply(agent_state *state,
                                  const char *module,
                                  unsigned long start_line,
                                  unsigned long end_line,
                                  const char *new_name);
int symbol_extract_function_verify(agent_state *state,
                                   const char *module,
                                   unsigned long start_line,
                                   unsigned long end_line,
                                   const char *new_name);

void symbol_inline_function_preview(agent_state *state,
                                    const char *symbol);
int symbol_inline_function_apply(agent_state *state,
                                 const char *symbol);

void symbol_delete_function_preview(agent_state *state,
                                    const char *symbol);
int symbol_delete_function_apply(agent_state *state,
                                 const char *symbol);

void symbol_move_function_preview(agent_state *state,
                                  const char *symbol,
                                  const char *destination);
int symbol_move_function_apply(agent_state *state,
                               const char *symbol,
                               const char *destination);

void symbol_declaration_order_check(agent_state *state,
                                    const char *module);

int symbol_declaration_order_fix(agent_state *state,
                                 const char *module);

int symbol_decl_fix_limited(agent_state *state,
                            const char *module,
                            unsigned int limit);

int symbol_decl_fix_auto(agent_state *state,
                         const char *module,
                         unsigned int batch_size);

void symbol_decl_fix_diagnose(agent_state *state,
                              const char *module,
                              unsigned int limit);

/*
 * Refactor verification in SYMBOL_INDEX.C uses system() for controlled build
 * and object invalidation.  The M279 wrapper delegates every command except
 * the legacy [.BUILD] object-delete form, which gains flat-root awareness.
 */
#include "SYMBOL_SYSTEM_M279.INC"

/*
 * RENAME/APPLY and RENAME/VERIFY historically wrote C source directly with
 * stdio and rolled back by assuming a ; -1 version existed.  Route only the
 * command-side entry points through an RMS/version-safe M279 transaction.
 * The legacy symbol translation unit remains untouched.
 */
#ifdef OVMS_AGENT_COMMAND_INTERNAL_H
#include "SYMBOL_RENAME_M279.INC"
#define symbol_rename_apply m279_rename_apply
#define symbol_rename_verify m279_rename_verify
#endif

#endif /* OVMS_AGENT_SYMBOL_INDEX_H */
