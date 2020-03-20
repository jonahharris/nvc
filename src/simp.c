/* ========================================================================= **
**                               ____ _   _______                            **
**                              / __ \ | / / ___/                            **
**                             / / / / |/ / /__                              **
**                            /_/ /_/|___/\___/                              **
**                                                                           **
** ========================================================================= **
**                         OPEN SOURCE VHDL COMPILER                         **
** ========================================================================= **
** This file is part of the NVC VHDL Compiler                                **
** Copyright (C) Nick Gasson <nick@nickg.me.uk>                              **
** All Rights Reserved.                                                      **
**                                                                           **
** Permission to use, copy, modify, and/or distribute this software for any  **
** purpose is subject to the terms specified in COPYING.                     **
** ========================================================================= */

/* ========================================================================= */
/* -- INCLUSIONS ----------------------------------------------------------- */
/* ========================================================================= */

/* Interface Inclusions */
#include "phase.h"
#include "util.h"
#include "common.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdlib.h>

/* System Inclusions */

/* Project Inclusions */

/* ========================================================================= */
/* -- PRIVATE DEFINITIONS -------------------------------------------------- */
/* ========================================================================= */

#define MAX_BUILTIN_ARGS    2

/* ========================================================================= */
/* -- PRIVATE MACROS ------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE TYPEDEFS ----------------------------------------------------- */
/* ========================================================================= */

typedef struct imp_signal imp_signal_t;

struct imp_signal {
  imp_signal_t *next;
  tree_t        signal;
  tree_t        process;
};

typedef struct {
  imp_signal_t *imp_signals;
  tree_t        top;
  ident_t       prefix;
  eval_flags_t  eval_flags;
} simp_ctx_t;

/* ========================================================================= */
/* -- PRIVATE STRUCTURES --------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- INTERNAL FUNCTION PROTOTYPES ----------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION PROTOTYPES ------------------------------------------- */
/* ========================================================================= */

static tree_t simp_array_ref(tree_t t);
static tree_t simp_array_slice(tree_t t);
static tree_t simp_assert(tree_t t);
static tree_t simp_assoc(tree_t t);
static tree_t simp_attr_delayed_transaction(tree_t t, predef_attr_t predef,
  simp_ctx_t *ctx);
static tree_t simp_attr_ref(tree_t t, simp_ctx_t *ctx);
static void simp_build_wait(tree_t wait, tree_t expr);
static tree_t simp_call_args(tree_t t);
static tree_t simp_case(tree_t t);
static tree_t simp_cassert(tree_t t);
static tree_t simp_cassign(tree_t t);
static tree_t simp_concat(tree_t t);
static tree_t simp_context_ref(tree_t t, simp_ctx_t *ctx);
static tree_t simp_cpcall(tree_t t);
static tree_t simp_extract_aggregate(tree_t agg, int64_t index, tree_t def);
static tree_t simp_extract_string_literal(tree_t literal, int64_t index,
  tree_t def);
static tree_t simp_fcall(tree_t t, simp_ctx_t *ctx);
static tree_t simp_if(tree_t t);
static tree_t simp_if_generate(tree_t t);
static tree_t simp_pcall(tree_t t);
static tree_t simp_process(tree_t t);
static tree_t simp_record_ref(tree_t t);
static tree_t simp_ref(tree_t t);
static tree_t simp_select(tree_t t);
static tree_t simp_signal_assign(tree_t t);
static tree_t simp_tree(tree_t t, void *_ctx);
static tree_t simp_type_conv(tree_t t, simp_ctx_t *ctx);
static tree_t simp_wait(tree_t t);
static tree_t simp_while(tree_t t);

/* ========================================================================= */
/* -- PRIVATE DATA --------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- EXPORTED DATA -------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC ASSERTIONS ---------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- EXPORTED FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

void
simplify (
  tree_t       top,
  eval_flags_t flags
) {
  simp_ctx_t ctx =
  {
    .imp_signals = NULL,
    .top         = top,
    .prefix      = ident_runtil(tree_ident(top),'-'),
    .eval_flags  = flags
  };

  tree_rewrite(top, simp_tree, &ctx);

  while (ctx.imp_signals != NULL) {
    tree_add_decl(top, ctx.imp_signals->signal);
    tree_add_stmt(top, ctx.imp_signals->process);

    imp_signal_t *tmp = ctx.imp_signals->next;
    free(ctx.imp_signals);
    ctx.imp_signals = tmp;
  }
} /* simplify() */

/* ========================================================================= */
/* -- INTERNAL FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION DEFINITIONS ------------------------------------------ */
/* ========================================================================= */

static tree_t
simp_array_ref (
  tree_t t
) {
  tree_t value = tree_value(t);

  if (tree_kind(value) == T_OPEN) {
    return (value);
  }

  const int nparams = tree_params(t);

  int64_t indexes[nparams];
  bool can_fold = true;
  for (int i = 0; i < nparams; i++) {
    tree_t p = tree_param(t, i);
    assert(tree_subkind(p) == P_POS);
    can_fold = can_fold && folded_int(tree_value(p), &indexes[i]);
  }

  if (!can_fold) {
    return (t);
  }

  if (!tree_has_type(value)) {
    return (t);
  }

  const tree_kind_t value_kind = tree_kind(value);
  if (value_kind == T_AGGREGATE) {
    return (simp_extract_aggregate(value, indexes[0], t));
  } else if (value_kind == T_LITERAL) {
    return (simp_extract_string_literal(value, indexes[0], t));
  } else if (value_kind != T_REF) {
    return (t);   // Cannot fold nested array references
  }
  tree_t decl = tree_ref(value);

  if (nparams > 1) {
    return (t);  // Cannot constant fold multi-dimensional arrays
  }
  assert(nparams == 1);

  switch (tree_kind(decl))
  {
    case T_CONST_DECL:
      {
        if (!tree_has_value(decl)) {
          return (t);
        }

        tree_t v = tree_value(decl);
        if (tree_kind(v) != T_AGGREGATE) {
          return (t);
        }

        return (simp_extract_aggregate(v, indexes[0], t));
      }

    default:
      return (t);
  }
} /* simp_array_ref() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_array_slice (
  tree_t t
) {
  tree_t value = tree_value(t);

  if (tree_kind(value) == T_OPEN) {
    return (value);
  }

  return (t);
} /* simp_array_slice() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_assert (
  tree_t t
) {
  bool value_b;

  if (!tree_has_value(t)) {
    return (t);
  } else if (folded_bool(tree_value(t), &value_b) && value_b) {
    // Assertion always passes
    return (NULL);
  } else {
    return (t);
  }
} /* simp_assert() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_assoc (
  tree_t t
) {
  if (!tree_has_value(t)) {
    return (NULL);   // Delete it
  }
  return (t);
} /* simp_assoc() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_attr_delayed_transaction (
  tree_t        t,
  predef_attr_t predef,
  simp_ctx_t   *ctx
) {
  tree_t name = tree_name(t);

  assert(tree_kind(name) == T_REF);

  tree_t decl = tree_ref(name);

  const tree_kind_t kind = tree_kind(decl);
  if ((kind != T_SIGNAL_DECL) && (kind != T_PORT_DECL)) {
    return (t);
  }

  char *sig_name LOCAL =
    xasprintf("%s_%s", (predef == ATTR_DELAYED) ? "delayed" : "transaction",
      istr(tree_ident(name)));

  tree_t s = tree_new(T_SIGNAL_DECL);
  tree_set_loc(s, tree_loc(t));
  tree_set_ident(s, ident_uniq(sig_name));
  tree_set_type(s, tree_type(t));

  tree_t p = tree_new(T_PROCESS);
  tree_set_loc(p, tree_loc(t));
  tree_set_ident(p, ident_prefix(tree_ident(s), ident_new("p"), '_'));

  tree_t r = make_ref(s);

  tree_t a = tree_new(T_SIGNAL_ASSIGN);
  tree_set_ident(a, ident_new("assign"));
  tree_set_target(a, r);

  switch (predef)
  {
    case ATTR_DELAYED:
      {
        if (tree_has_value(decl)) {
          tree_set_value(s, tree_value(decl));
        } else {
          tree_set_value(s, make_default_value(tree_type(t), tree_loc(t)));
        }

        tree_t delay = tree_value(tree_param(t, 0));

        tree_t wave = tree_new(T_WAVEFORM);
        tree_set_value(wave, name);
        tree_set_delay(wave, delay);

        tree_add_waveform(a, wave);
      }
      break;

    case ATTR_TRANSACTION:
      {
        tree_set_value(s, make_default_value(tree_type(s), tree_loc(s)));

        tree_t not = call_builtin("not", tree_type(r), r, NULL);

        tree_t wave = tree_new(T_WAVEFORM);
        tree_set_value(wave, not);

        tree_add_waveform(a, wave);
      }
      break;

    default:
      {
      }
      break;
  }

  tree_add_stmt(p, a);

  tree_t wait = tree_new(T_WAIT);
  tree_set_ident(wait, ident_new("wait"));
  tree_add_attr_int(wait, ident_new("static"), 1);
  tree_add_trigger(wait, name);

  tree_add_stmt(p, wait);

  imp_signal_t *imp = xmalloc(sizeof(imp_signal_t));
  imp->next = ctx->imp_signals;
  imp->signal = s;
  imp->process = p;

  ctx->imp_signals = imp;

  return (r);
} /* simp_attr_delayed_transaction() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_attr_ref (
  tree_t      t,
  simp_ctx_t *ctx
) {
  if (tree_has_value(t)) {
    return (tree_value(t));
  }

  const predef_attr_t predef = tree_attr_int(t, builtin_i, -1);
  switch (predef)
  {
    case ATTR_DELAYED:
    case ATTR_TRANSACTION:
      {
        return (simp_attr_delayed_transaction(t, predef, ctx));
      }

    case ATTR_LENGTH:
    case ATTR_LEFT:
    case ATTR_LOW:
    case ATTR_HIGH:
    case ATTR_RIGHT:
    case ATTR_ASCENDING:
      {
        tree_t name = tree_name(t);

        if (tree_kind(name) != T_REF) {
          return (t);   // Cannot fold this
        }
        type_t type = tree_type(name);
        int64_t dim_i = 1;

        if (type_kind(type) == T_ENUM) {
          // Enumeration subtypes are handled below
          const int nlits = type_enum_literals(type);

          switch (predef)
          {
            case ATTR_LEFT:
            case ATTR_LOW:
              {
                return (make_ref(type_enum_literal(type, 0)));
              }

            case ATTR_RIGHT:
            case ATTR_HIGH:
              {
                return (make_ref(type_enum_literal(type, nlits - 1)));
              }

            case ATTR_ASCENDING:
              {
                return (get_enum_lit(t, true));
              }

            default:
              fatal_trace("invalid enumeration attribute %d", predef);
          }
        }

        if (type_is_array(type)) {
          if (tree_params(t) > 0) {
            const bool f = folded_int(tree_value(tree_param(t, 0)), &dim_i);
            assert(f);
          }

          if (type_is_unconstrained(type)) {
            return (t);
          }

          if ((dim_i < 1) || (dim_i > array_dimension(type))) {
            return (t);
          }
        }

        range_t r = range_of(type, dim_i - 1);

        const bool known_dir =
          (r.kind == RANGE_TO) || (r.kind == RANGE_DOWNTO);

        if ((predef == ATTR_LENGTH) && known_dir) {
          if ((tree_kind(r.left) == T_LITERAL) &&
            (tree_kind(r.right) == T_LITERAL)) {
            int64_t low, high;
            range_bounds(r, &low, &high);
            return (get_int_lit(t, (high < low) ? 0 : high - low + 1));
          } else {
            return (t);
          }
        } else if ((predef == ATTR_LOW) && known_dir) {
          return ((r.kind == RANGE_TO) ? r.left : r.right);
        } else if ((predef == ATTR_HIGH) && known_dir) {
          return ((r.kind == RANGE_TO) ? r.right : r.left);
        } else if (predef == ATTR_LEFT) {
          return (r.left);
        } else if (predef == ATTR_RIGHT) {
          return (r.right);
        } else if ((predef == ATTR_ASCENDING) && known_dir) {
          return (get_enum_lit(t, (r.kind == RANGE_TO)));
        } else {
          return (t);
        }
      }

    default:
      {
      }
      break;
  }

  return (t);
} /* simp_attr_ref() */

/* ------------------------------------------------------------------------- */

static void
simp_build_wait (
  tree_t wait,
  tree_t expr
) {
  // Add each signal referenced in an expression to the trigger
  // list for a wait statement

  switch (tree_kind(expr))
  {
    case T_REF:
      {
        tree_t decl = tree_ref(expr);
        tree_kind_t kind = tree_kind(decl);
        if ((kind == T_SIGNAL_DECL) || (kind == T_PORT_DECL) ||
          (kind == T_ALIAS)) {
          // Check for duplicates
          const int ntriggers = tree_triggers(wait);
          for (int i = 0; i < ntriggers; i++) {
            if (tree_ref(tree_trigger(wait, i)) == decl) {
              return;
            }
          }

          tree_add_trigger(wait, expr);
        }
      }
      break;

    case T_ARRAY_SLICE:
      {
        range_t r = tree_range(expr, 0);
        if (r.left != NULL) {
          simp_build_wait(wait, r.left);
        }
        if (r.right != NULL) {
          simp_build_wait(wait, r.right);
        }

        simp_build_wait(wait, tree_value(expr));
      }
      break;

    case T_WAVEFORM:
    case T_RECORD_REF:
    case T_QUALIFIED:
      {
        simp_build_wait(wait, tree_value(expr));
      }
      break;

    case T_ARRAY_REF:
      {
        simp_build_wait(wait, tree_value(expr));
        // Fall-through
      }

    case T_CONCAT:
    case T_FCALL:
    case T_TYPE_CONV:
      {
        const int nparams = tree_params(expr);
        for (int i = 0; i < nparams; i++) {
          simp_build_wait(wait, tree_value(tree_param(expr, i)));
        }
      }
      break;

    case T_AGGREGATE:
      {
        const int nassocs = tree_assocs(expr);
        for (int i = 0; i < nassocs; i++) {
          simp_build_wait(wait, tree_value(tree_assoc(expr, i)));
        }
      }
      break;

    case T_ATTR_REF:
      {
        const predef_attr_t predef = tree_attr_int(expr, builtin_i, -1);
        if ((predef == ATTR_EVENT) || (predef == ATTR_ACTIVE)) {
          simp_build_wait(wait, tree_name(expr));
        }
      }
      break;

    case T_LITERAL:
      {
      }
      break;

    default:
      fatal_trace("Cannot handle tree kind %s in wait expression",
        tree_kind_str(tree_kind(expr)));
  }
} /* simp_build_wait() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_call_args (
  tree_t t
) {
  tree_t decl = tree_ref(t);

  const int nparams = tree_params(t);
  const int nports = tree_ports(decl);

  // Replace named arguments with positional ones

  int last_pos = -1;

  for (int i = 0; i < nparams; i++) {
    if (tree_subkind(tree_param(t, i)) == P_POS) {
      last_pos = i;
    }
  }

  if (last_pos < nparams - 1) {
    tree_t new = tree_new(tree_kind(t));
    tree_set_loc(new, tree_loc(t));
    tree_set_ident(new, tree_ident(t));
    tree_set_ref(new, tree_ref(t));

    tree_kind_t kind = tree_kind(t);
    if ((kind == T_FCALL) || (kind == T_ATTR_REF)) {
      tree_set_type(new, tree_type(t));
    } else if (kind == T_CPCALL) {
      tree_set_ident2(new, tree_ident2(t));
    }

    for (int i = 0; i <= last_pos; i++) {
      tree_t port = tree_port(decl, i);
      tree_t param = tree_param(t, i);
      tree_t value = tree_value(param);

      if (tree_kind(value) == T_OPEN) {
        value = tree_value(port);
      }

      add_param(new, value, P_POS, NULL);
    }

    for (int i = last_pos + 1; i < nports; i++) {
      tree_t port = tree_port(decl, i);
      ident_t name = tree_ident(port);

      bool found = false;
      for (int j = last_pos + 1; (j < nparams) && !found; j++) {
        tree_t p = tree_param(t, j);
        assert(tree_subkind(p) == P_NAMED);

        tree_t ref = tree_name(p);
        assert(tree_kind(ref) == T_REF);

        if (name == tree_ident(ref)) {
          tree_t value = tree_value(p);

          if (tree_kind(value) == T_OPEN) {
            value = tree_value(port);
          }

          add_param(new, value, P_POS, NULL);
          found = true;
        }
      }
      assert(found);
    }

    t = new;
  }

  return (t);
} /* simp_call_args() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_case (
  tree_t t
) {
  const int nassocs = tree_assocs(t);

  if (nassocs == 0) {
    return (NULL);    // All choices are unreachable
  }
  int64_t ival;
  if (folded_int(tree_value(t), &ival)) {
    for (int i = 0; i < nassocs; i++) {
      tree_t a = tree_assoc(t, i);
      switch ((assoc_kind_t) tree_subkind(a))
      {
        case A_NAMED:
          {
            int64_t aval;
            if (folded_int(tree_name(a), &aval) && (ival == aval)) {
              if (tree_has_value(a)) {
                return (tree_value(a));
              } else {
                return (NULL);
              }
            }
          }
          break;

        case A_RANGE:
          {
            continue;   // TODO
          }

        case A_OTHERS:
          {
            if (tree_has_value(a)) {
              return (tree_value(a));
            } else {
              return (NULL);
            }
          }

        case A_POS:
          {
          }
          break;
      }
    }
  }

  return (t);
} /* simp_case() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_cassert (
  tree_t t
) {
  tree_t process = tree_new(T_PROCESS);

  tree_set_ident(process, tree_ident(t));
  tree_set_loc(process, tree_loc(t));

  if (tree_flags(t) & TREE_F_POSTPONED) {
    tree_set_flag(process, TREE_F_POSTPONED);
  }

  tree_t wait = tree_new(T_WAIT);
  tree_set_ident(wait, ident_new("assert_wait"));
  tree_add_attr_int(wait, ident_new("static"), 1);

  tree_t a = tree_new(T_ASSERT);
  tree_set_ident(a, ident_new("assert_wrap"));
  tree_set_loc(a, tree_loc(t));
  tree_set_value(a, tree_value(t));
  tree_set_severity(a, tree_severity(t));
  if (tree_has_message(t)) {
    tree_set_message(a, tree_message(t));
  }

  simp_build_wait(wait, tree_value(t));

  tree_add_stmt(process, a);
  tree_add_stmt(process, wait);

  return (process);
} /* simp_cassert() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_cassign (
  tree_t t
) {
  // Replace concurrent assignments with a process

  tree_t p = tree_new(T_PROCESS);

  tree_set_ident(p, tree_ident(t));
  tree_set_loc(p, tree_loc(t));

  tree_t w = tree_new(T_WAIT);
  tree_set_ident(w, ident_new("cassign"));
  tree_add_attr_int(w, ident_new("static"), 1);

  tree_t container = p;   // Where to add new statements
  void (*add_stmt)(tree_t, tree_t) = tree_add_stmt;

  tree_t target = tree_target(t);

  const int nconds = tree_conds(t);
  for (int i = 0; i < nconds; i++) {
    tree_t c = tree_cond(t, i);

    if (tree_has_value(c)) {
      // Replace this with an if statement
      tree_t i = tree_new(T_IF);
      tree_set_value(i, tree_value(c));
      tree_set_ident(i, ident_uniq("cond"));

      simp_build_wait(w, tree_value(c));

      (*add_stmt)(container, i);

      container = i;
      add_stmt = tree_add_stmt;
    }

    tree_t s = tree_new(T_SIGNAL_ASSIGN);
    tree_set_loc(s, tree_loc(t));
    tree_set_target(s, target);
    tree_set_ident(s, tree_ident(t));
    if (tree_has_reject(c)) {
      tree_set_reject(s, tree_reject(c));
    }

    const int nwaves = tree_waveforms(c);
    for (int i = 0; i < nwaves; i++) {
      tree_t wave = tree_waveform(c, i);
      tree_add_waveform(s, wave);
      simp_build_wait(w, wave);
    }

    (*add_stmt)(container, s);

    if (tree_has_value(c)) {
      // Add subsequent statements to the else part
      add_stmt = tree_add_else_stmt;
    }
  }

  tree_add_stmt(p, w);
  return (p);
} /* simp_cassign() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_concat (
  tree_t t
) {
  // Flatten nested concatenations to make efficient code generation easier

  tree_t p0 = tree_value(tree_param(t, 0));

  if (tree_kind(p0) != T_CONCAT) {
    return (t);
  }

  tree_t flat = tree_new(T_CONCAT);
  tree_set_loc(flat, tree_loc(t));
  tree_set_type(flat, tree_type(t));

  assert(tree_params(t) == 2);

  const int np0 = tree_params(p0);
  for (int i = 0; i < np0; i++) {
    tree_add_param(flat, tree_param(p0, i));
  }

  tree_add_param(flat, tree_param(t, 1));

  return (flat);
} /* simp_concat() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_context_ref (
  tree_t      t,
  simp_ctx_t *ctx
) {
  tree_t decl = tree_ref(t);

  const int nctx = tree_contexts(decl);

  for (int i = 2; i < nctx; i++) {
    tree_add_context(ctx->top, tree_context(decl, i));
  }

  return (NULL);
} /* simp_context_ref() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_cpcall (
  tree_t t
) {
  t = simp_call_args(t);

  tree_t process = tree_new(T_PROCESS);
  tree_set_ident(process, tree_ident(t));
  tree_set_loc(process, tree_loc(t));

  tree_t wait = tree_new(T_WAIT);
  tree_set_ident(wait, ident_new("pcall_wait"));

  tree_t pcall = tree_new(T_PCALL);
  tree_set_ident(pcall, ident_new("pcall"));
  tree_set_ident2(pcall, tree_ident2(t));
  tree_set_loc(pcall, tree_loc(t));
  tree_set_ref(pcall, tree_ref(t));

  const int nparams = tree_params(t);
  for (int i = 0; i < nparams; i++) {
    tree_t p = tree_param(t, i);
    assert(tree_subkind(p) == P_POS);

    // Only add IN and INOUT parameters to sensitivity list
    tree_t port = tree_port(tree_ref(t), i);
    port_mode_t mode = tree_subkind(port);
    if ((mode == PORT_IN) || (mode == PORT_INOUT)) {
      simp_build_wait(wait, tree_value(p));
    }

    tree_add_param(pcall, p);
  }

  tree_add_stmt(process, pcall);
  tree_add_stmt(process, wait);

  return (process);
} /* simp_cpcall() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_extract_aggregate (
  tree_t  agg,
  int64_t index,
  tree_t  def
) {
  range_t bounds = range_of(tree_type(agg), 0);
  int64_t low, high;

  range_bounds(bounds, &low, &high);

  const bool to = (bounds.kind == RANGE_TO);

  const int nassocs = tree_assocs(agg);
  for (int i = 0; i < nassocs; i++) {
    tree_t a = tree_assoc(agg, i);
    switch (tree_subkind(a))
    {
      case A_POS:
        {
          const int pos = tree_pos(a);
          if ((to && (pos + low == index)) ||
            (!to && (high - pos == index))) {
            return (tree_value(a));
          }
        }
        break;

      case A_OTHERS:
        {
          return (tree_value(a));
        }

      case A_RANGE:
        {
          range_t r = tree_range(a, 0);
          const int64_t left = assume_int(r.left);
          const int64_t right = assume_int(r.right);

          if ((to && (index >= left) && (index <= right)) ||
            (!to && (index <= left) && (index >= right))) {
            return (tree_value(a));
          }
        }
        break;

      case A_NAMED:
        {
          if (assume_int(tree_name(a)) == index) {
            return (tree_value(a));
          }
        }
        break;
    }
  }

  return (def);
} /* simp_extract_aggregate() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_extract_string_literal (
  tree_t  literal,
  int64_t index,
  tree_t  def
) {
  type_t type = tree_type(literal);

  range_t bounds = range_of(type, 0);
  int64_t low, high;

  range_bounds(bounds, &low, &high);

  const bool to = (bounds.kind == RANGE_TO);

  const int pos = to ? (index + low) : (high - index);
  if ((pos < 0) || (pos > tree_chars(literal))) {
    return (def);
  }

  return (tree_char(literal, pos));
} /* simp_extract_string_literal() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_fcall (
  tree_t      t,
  simp_ctx_t *ctx
) {
  return (eval(simp_call_args(t),
         EVAL_FCALL | EVAL_FOLDING | ctx->eval_flags));
} /* simp_fcall() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_if (
  tree_t t
) {
  bool value_b;

  if (folded_bool(tree_value(t), &value_b)) {
    if (value_b) {
      // If statement always executes so replace with then part
      if (tree_stmts(t) == 1) {
        return (tree_stmt(t, 0));
      } else {
        tree_t b = tree_new(T_BLOCK);
        tree_set_ident(b, tree_ident(t));
        for (unsigned i = 0; i < tree_stmts(t); i++) {
          tree_add_stmt(b, tree_stmt(t, i));
        }
        return (b);
      }
    } else {
      // If statement never executes so replace with else part
      if (tree_else_stmts(t) == 1) {
        return (tree_else_stmt(t, 0));
      } else if (tree_else_stmts(t) == 0) {
        return (NULL);     // Delete it
      } else {
        tree_t b = tree_new(T_BLOCK);
        tree_set_ident(b, tree_ident(t));
        for (unsigned i = 0; i < tree_else_stmts(t); i++) {
          tree_add_stmt(b, tree_else_stmt(t, i));
        }
        return (b);
      }
    }
  } else {
    return (t);
  }
} /* simp_if() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_if_generate (
  tree_t t
) {
  bool value_b;

  if (!folded_bool(tree_value(t), &value_b)) {
    return (t);
  }

  if (value_b) {
    tree_t block = tree_new(T_BLOCK);
    tree_set_ident(block, tree_ident(t));
    tree_set_loc(block, tree_loc(t));

    const int ndecls = tree_decls(t);
    for (int i = 0; i < ndecls; i++) {
      tree_add_decl(block, tree_decl(t, i));
    }

    const int nstmts = tree_stmts(t);
    for (int i = 0; i < nstmts; i++) {
      tree_add_stmt(block, tree_stmt(t, i));
    }

    return (block);
  } else {
    return (NULL);
  }
} /* simp_if_generate() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_pcall (
  tree_t t
) {
  return (simp_call_args(t));
} /* simp_pcall() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_process (
  tree_t t
) {
  // Replace sensitivity list with a "wait on" statement
  const int ntriggers = tree_triggers(t);

  if (ntriggers > 0) {
    const int nstmts = tree_stmts(t);
    if (nstmts == 0) {
      return (NULL);    // Body was optimised away
    }
    tree_t p = tree_new(T_PROCESS);
    tree_set_ident(p, tree_ident(t));
    tree_set_loc(p, tree_loc(t));

    const int ndecls = tree_decls(t);
    for (int i = 0; i < ndecls; i++) {
      tree_add_decl(p, tree_decl(t, i));
    }

    for (int i = 0; i < nstmts; i++) {
      tree_add_stmt(p, tree_stmt(t, i));
    }

    tree_t w = tree_new(T_WAIT);
    tree_set_ident(w, tree_ident(p));
    tree_add_attr_int(w, ident_new("static"), 1);
    for (int i = 0; i < ntriggers; i++) {
      tree_add_trigger(w, tree_trigger(t, i));
    }
    tree_add_stmt(p, w);

    return (p);
  }

  // Delete processes that contain just a single wait statement
  if ((tree_stmts(t) == 1) && (tree_kind(tree_stmt(t, 0)) == T_WAIT)) {
    return (NULL);
  } else {
    return (t);
  }
} /* simp_process() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_record_ref (
  tree_t t
) {
  tree_t value = tree_value(t), agg = NULL;

  switch (tree_kind(value))
  {
    case T_AGGREGATE:
      {
        agg = value;
      }
      break;

    case T_REF:
      {
        tree_t decl = tree_ref(value);
        if (tree_kind(decl) != T_CONST_DECL) {
          return (t);
        }

        agg = tree_value(decl);
        if (tree_kind(agg) != T_AGGREGATE) {
          return (t);
        }
      }
      break;

    case T_OPEN:
      {
        return (value);
      }

    default:
      return (t);
  }

  ident_t field = tree_ident(t);
  type_t type = tree_type(agg);

  const int nassocs = tree_assocs(agg);
  for (int i = 0; i < nassocs; i++) {
    tree_t a = tree_assoc(agg, i);
    switch (tree_subkind(a))
    {
      case A_POS:
        {
          if (tree_ident(type_field(type, tree_pos(a))) == field) {
            return (tree_value(a));
          }
        }
        break;

      case A_NAMED:
        {
          if (tree_ident(tree_name(a)) == field) {
            return (tree_value(a));
          }
        }
        break;
    }
  }

  return (t);
} /* simp_record_ref() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_ref (
  tree_t t
) {
  tree_t decl = tree_ref(t);

  switch (tree_kind(decl))
  {
    case T_CONST_DECL:
      {
        if (type_is_array(tree_type(decl))) {
          return (t);
        } else {
          tree_t value = tree_value(decl);
          switch (tree_kind(value))
          {
            case T_LITERAL:
              {
                return (value);
              }

            case T_REF:
              {
                if (tree_kind(tree_ref(value)) == T_ENUM_LIT) {
                  return (value);
                }
                // Fall-through
              }

            default:
              return (t);
          }
        }
      }

    case T_UNIT_DECL:
      {
        return (tree_value(decl));
      }

    default:
      return (t);
  }
} /* simp_ref() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_select (
  tree_t t
) {
  // Replace a select statement with a case inside a process

  tree_t p = tree_new(T_PROCESS);

  tree_set_ident(p, tree_ident(t));

  tree_t w = tree_new(T_WAIT);
  tree_set_ident(w, ident_new("select_wait"));
  tree_add_attr_int(w, ident_new("static"), 1);

  tree_t c = tree_new(T_CASE);
  tree_set_ident(c, ident_new("select_case"));
  tree_set_loc(c, tree_loc(t));
  tree_set_value(c, tree_value(t));

  simp_build_wait(w, tree_value(t));

  const int nassocs = tree_assocs(t);
  for (int i = 0; i < nassocs; i++) {
    tree_t a = tree_assoc(t, i);
    tree_add_assoc(c, a);

    if (tree_subkind(a) == A_NAMED) {
      simp_build_wait(w, tree_name(a));
    }

    tree_t value = tree_value(a);

    const int nwaveforms = tree_waveforms(value);
    for (int j = 0; j < nwaveforms; j++) {
      simp_build_wait(w, tree_waveform(value, j));
    }
  }

  tree_add_stmt(p, c);
  tree_add_stmt(p, w);
  return (p);
} /* simp_select() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_signal_assign (
  tree_t t
) {
  tree_t target = tree_target(t);

  if (tree_kind(target) == T_OPEN) {
    return (NULL);    // Delete it
  }
  return (t);
} /* simp_signal_assign() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_tree (
  tree_t t,
  void  *_ctx
) {
  simp_ctx_t *ctx = _ctx;

  switch (tree_kind(t))
  {
    case T_PROCESS:
      {
        return (simp_process(t));
      }

    case T_ARRAY_REF:
      {
        return (simp_array_ref(t));
      }

    case T_ARRAY_SLICE:
      {
        return (simp_array_slice(t));
      }

    case T_ATTR_REF:
      {
        return (simp_attr_ref(t, ctx));
      }

    case T_FCALL:
      {
        return (simp_fcall(t, ctx));
      }

    case T_PCALL:
      {
        return (simp_pcall(t));
      }

    case T_REF:
      {
        return (simp_ref(t));
      }

    case T_IF:
      {
        return (simp_if(t));
      }

    case T_CASE:
      {
        return (simp_case(t));
      }

    case T_WHILE:
      {
        return (simp_while(t));
      }

    case T_CASSIGN:
      {
        return (simp_cassign(t));
      }

    case T_SELECT:
      {
        return (simp_select(t));
      }

    case T_WAIT:
      {
        return (simp_wait(t));
      }

    case T_NULL:
      {
        return (NULL); // Delete it
      }

    case T_CPCALL:
      {
        return (simp_cpcall(t));
      }

    case T_CASSERT:
      {
        return (simp_cassert(t));
      }

    case T_CONCAT:
      {
        return (simp_concat(t));
      }

    case T_RECORD_REF:
      {
        return (simp_record_ref(t));
      }

    case T_CTXREF:
      {
        return (simp_context_ref(t, ctx));
      }

    case T_ASSERT:
      {
        return (simp_assert(t));
      }

    case T_IF_GENERATE:
      {
        return (simp_if_generate(t));
      }

    case T_SIGNAL_ASSIGN:
      {
        return (simp_signal_assign(t));
      }

    case T_ASSOC:
      {
        return (simp_assoc(t));
      }

    case T_TYPE_CONV:
      {
        return (simp_type_conv(t, ctx));
      }

    default:
      return (t);
  }
} /* simp_tree() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_type_conv (
  tree_t      t,
  simp_ctx_t *ctx
) {
  return (eval(t, EVAL_FOLDING | ctx->eval_flags));
} /* simp_type_conv() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_wait (
  tree_t t
) {
  // LRM 93 section 8.1
  // If there is no sensitivity list supplied generate one from the
  // condition clause

  if (tree_has_value(t) && (tree_triggers(t) == 0)) {
    simp_build_wait(t, tree_value(t));
  }

  return (t);
} /* simp_wait() */

/* ------------------------------------------------------------------------- */

static tree_t
simp_while (
  tree_t t
) {
  bool value_b;

  if (!tree_has_value(t)) {
    return (t);
  } else if (folded_bool(tree_value(t), &value_b) && !value_b) {
    // Condition is false so loop never executes
    return (NULL);
  } else {
    return (t);
  }
} /* simp_while() */

/* :vi set ts=2 et sw=2: */

