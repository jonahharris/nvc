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
#include "util.h"
#include "tree.h"
#include "phase.h"
#include "common.h"
#include "rt/netdb.h"

#include <assert.h>
#include <stdlib.h>

/* System Inclusions */

/* Project Inclusions */

/* ========================================================================= */
/* -- PRIVATE DEFINITIONS -------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE MACROS ------------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- PRIVATE TYPEDEFS ----------------------------------------------------- */
/* ========================================================================= */

typedef struct {
  group_t  *groups;
  group_t  *free_list;
  groupid_t next_gid;
  group_t **lookup;
  int       nnets;
} group_nets_ctx_t;

/* ========================================================================= */
/* -- PRIVATE STRUCTURES --------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- INTERNAL FUNCTION PROTOTYPES ----------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION PROTOTYPES ------------------------------------------- */
/* ========================================================================= */

static groupid_t group_add(group_nets_ctx_t *ctx, netid_t first, int length);
static groupid_t group_alloc(group_nets_ctx_t *ctx, netid_t first,
  unsigned length);
static bool group_contains_record(type_t type);
static void group_decl(tree_t decl, group_nets_ctx_t *ctx, int start, int n);
static void group_free_list(group_t *list);
static void group_init_context(group_nets_ctx_t *ctx, int nnets);
static bool group_name(tree_t target, group_nets_ctx_t *ctx, int start,
  int n);
static int group_net_to_field(type_t type, netid_t nid);
static void group_nets_visit_fn(tree_t t, void *_ctx);
static void group_ref(tree_t target, group_nets_ctx_t *ctx, int start, int n);
static void group_reuse(group_nets_ctx_t *ctx, group_t *group);
static void group_target(tree_t t, group_nets_ctx_t *ctx);
static void group_unlink(group_nets_ctx_t *ctx, group_t *where);
static void group_write_netdb(tree_t top, group_nets_ctx_t *ctx);
static void ungroup_name(tree_t name, group_nets_ctx_t *ctx);
static void ungroup_proc_params(tree_t t, group_nets_ctx_t *ctx);
static void ungroup_ref(tree_t target, group_nets_ctx_t *ctx);

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
group_nets (
  tree_t top
) {
  const int nnets = tree_attr_int(top, nnets_i, 0);

  group_nets_ctx_t ctx;

  group_init_context(&ctx, nnets);
  tree_visit(top, group_nets_visit_fn, &ctx);

  group_write_netdb(top, &ctx);

  if (opt_get_int("verbose")) {
    int ngroups = 0;
    for (group_t *it = ctx.groups; it != NULL; it = it->next) {
      ngroups++;
    }

    notef("%d nets, %d groups", nnets, ngroups);
    notef("nets:groups ratio %.3f", (float) nnets / (float) ngroups);
  }

  group_free_list(ctx.groups);
  group_free_list(ctx.free_list);
  free(ctx.lookup);
} /* group_nets() */

/* ========================================================================= */
/* -- INTERNAL FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION DEFINITIONS ------------------------------------------ */
/* ========================================================================= */

static groupid_t
group_add (
  group_nets_ctx_t *ctx,
  netid_t           first,
  int               length
) {
  assert(length > 0);
  assert(first < ctx->nnets);
  assert(first + length <= ctx->nnets);

  for (netid_t i = first; i < first + length; i++) {
    group_t *it = ctx->lookup[i];
    if ((it == NULL) || (it->gid == GROUPID_INVALID)) {
      continue;
    }

    if ((it->first == first) && (it->length == length)) {
      // Exactly matches
      return (it->gid);
    } else if (it->first >= first + length) {
      // Disjoint, to the right
    } else if (first >= it->first + it->length) {
      // Disjoint, to the left
    } else if ((first == it->first) && (length > it->length)) {
      // Overlaps on left
      group_add(ctx, first + it->length, length - it->length);
      return (GROUPID_INVALID);
    } else if ((first > it->first) &&
      (first + length == it->first + it->length)) {
      // Overlaps on right
      group_unlink(ctx, it);
      group_add(ctx, it->first, first - it->first);
      group_reuse(ctx, it);
      return (group_alloc(ctx, first, length));
    } else if ((first > it->first) &&
      (first + length < it->first + it->length)) {
      // Overlaps completely
      group_unlink(ctx, it);
      group_add(ctx, it->first, first - it->first);
      group_add(ctx, first + length,
        it->first + it->length - first - length);
      group_reuse(ctx, it);
      return (group_alloc(ctx, first, length));
    } else if ((first < it->first) &&
      (first + length > it->first + it->length)) {
      // Contains in middle
      group_add(ctx, first, it->first - first);
      group_add(ctx, it->first + it->length,
        first + length - it->first - it->length);
      return (GROUPID_INVALID);
    } else if ((first == it->first) &&
      (first + length < it->first + it->length)) {
      // Contains on left
      group_unlink(ctx, it);
      group_add(ctx, first + length, it->length - length);
      group_reuse(ctx, it);
      return (group_alloc(ctx, first, length));
    } else if ((first < it->first) &&
      (first + length == it->first + it->length)) {
      // Contains on right
      group_add(ctx, first, it->first - first);
      return (GROUPID_INVALID);
    } else if ((first < it->first) && (first + length > it->first)) {
      // Split left
      group_unlink(ctx, it);
      group_add(ctx, first, it->first - first);
      group_add(ctx, it->first, first + length - it->first);
      group_add(ctx, first + length,
        it->first + it->length - first - length);
      group_reuse(ctx, it);
      return (GROUPID_INVALID);
    } else if ((first > it->first) && (it->first + it->length > first)) {
      // Split right
      group_unlink(ctx, it);
      group_add(ctx, it->first, first - it->first);
      group_add(ctx, first, it->first + it->length - first);
      group_add(ctx, it->first + it->length,
        first + length - it->first - it->length);
      group_reuse(ctx, it);
      return (GROUPID_INVALID);
    } else {
      fatal("unhandled case in group_add: first=%d length=%d "
        "it->first=%d it->length=%d", first, length,
        it->first, it->length);
    }
  }

  return (group_alloc(ctx, first, length));
} /* group_add() */

/* ------------------------------------------------------------------------- */

static groupid_t
group_alloc (
  group_nets_ctx_t *ctx,
  netid_t           first,
  unsigned          length
) {
  group_t *g;

  if (ctx->free_list != NULL) {
    g = ctx->free_list;
    ctx->free_list = g->next;
  } else {
    g = xmalloc(sizeof(group_t));
  }

  g->next = ctx->groups;
  g->gid = ctx->next_gid++;
  g->first = first;
  g->length = length;

  ctx->groups = g;

  for (netid_t i = first; i < first + length; i++) {
    ctx->lookup[i] = g;
  }

  return (g->gid);
} /* group_alloc() */

/* ------------------------------------------------------------------------- */

static bool
group_contains_record (
  type_t type
) {
  if (type_is_record(type)) {
    return (true);
  } else if (type_is_array(type)) {
    return (group_contains_record(type_elem(type)));
  } else {
    return (false);
  }
} /* group_contains_record() */

/* ------------------------------------------------------------------------- */

static void
group_decl (
  tree_t            decl,
  group_nets_ctx_t *ctx,
  int               start,
  int               n
) {
  netid_t first = NETID_INVALID;
  unsigned len = 0;
  type_t type = tree_type(decl);
  const int nnets = tree_nets(decl);
  const bool record = group_contains_record(type);
  int ffield = -1;

  assert((n == -1) | (start + n <= nnets));
  for (int i = start; i < (n == -1 ? nnets : start + n); i++) {
    netid_t nid = tree_net(decl, i);
    if (first == NETID_INVALID) {
      first = nid;
      len = 1;
      ffield = record ? group_net_to_field(type, i) : -1;
    } else if ((nid == first + len) &&
      (!record || (group_net_to_field(type, i) == ffield))) {
      ++len;
    } else {
      group_add(ctx, first, len);
      first = nid;
      len = 1;
      ffield = record ? group_net_to_field(type, i) : -1;
    }
  }

  if (first != NETID_INVALID) {
    group_add(ctx, first, len);
  } else {
    // Array signal with null range
    tree_add_attr_int(decl, null_range_i, 1);
  }
} /* group_decl() */

/* ------------------------------------------------------------------------- */

static void
group_free_list (
  group_t *list
) {
  while (list != NULL) {
    group_t *tmp = list->next;
    free(list);
    list = tmp;
  }
} /* group_free_list() */

/* ------------------------------------------------------------------------- */

static void
group_init_context (
  group_nets_ctx_t *ctx,
  int               nnets
) {
  ctx->groups = NULL;
  ctx->next_gid = 0;
  ctx->lookup = xcalloc(nnets * sizeof(group_t *));
  ctx->nnets = nnets;
  ctx->free_list = NULL;
} /* group_init_context() */

/* ------------------------------------------------------------------------- */

static bool
group_name (
  tree_t            target,
  group_nets_ctx_t *ctx,
  int               start,
  int               n
) {
  switch (tree_kind(target))
  {
    case T_REF:
      {
        group_ref(target, ctx, start, n);
        return (true);
      }

    case T_ARRAY_REF:
      {
        tree_t value = tree_value(target);

        type_t type = tree_type(value);
        if (type_is_unconstrained(type)) {
          return (false);
        }

        int offset = 0;
        const int nparams = tree_params(target);
        for (int i = 0; i < nparams; i++) {
          tree_t index = tree_value(tree_param(target, i));
          const int stride = type_width(type_elem(type));

          if (tree_kind(index) != T_LITERAL) {
            if (i > 0) {
              return (false);
            }

            const int twidth = type_width(type);
            for (int j = 0; j < twidth; j += stride) {
              group_name(value, ctx, start + j, n);
            }
            return (true);
          } else {
            if (i > 0) {
              range_t type_r = range_of(type, i);
              int64_t low, high;
              range_bounds(type_r, &low, &high);
              offset *= high - low + 1;
            }

            offset += stride * rebase_index(type, i, assume_int(index));
          }
        }

        return (group_name(value, ctx, start + offset, n));
      }

    case T_ARRAY_SLICE:
      {
        tree_t value = tree_value(target);
        type_t type = tree_type(value);

        if (type_is_unconstrained(type)) {
          return (false);    // Only in procedure
        }
        range_t slice = tree_range(target, 0);

        if ((tree_kind(slice.left) != T_LITERAL) ||
          (tree_kind(slice.right) != T_LITERAL)) {
          return (false);
        }

        int64_t low, high;
        range_bounds(slice, &low, &high);

        const int64_t low0 = rebase_index(type, 0, assume_int(slice.left));
        const int stride = type_width(type_elem(type));

        return (group_name(value, ctx, start + low0 * stride, n));
      }

    case T_RECORD_REF:
      {
        tree_t value = tree_value(target);
        type_t rec = tree_type(value);
        const int offset = record_field_to_net(rec, tree_ident(target));

        return (group_name(value, ctx, start + offset, n));
      }

    case T_AGGREGATE:
    case T_LITERAL:
      {
        // This can appear due to assignments to open ports with a
        // default value
        return (true);
      }

    default:
      fatal_at(tree_loc(target), "tree kind %s not yet supported for offset "
        "calculation", tree_kind_str(tree_kind(target)));
  }
} /* group_name() */

/* ------------------------------------------------------------------------- */

static int
group_net_to_field (
  type_t  type,
  netid_t nid
) {
  int count = 0;

  if (type_is_record(type)) {
    const int nfields = type_fields(type);
    netid_t first = 0;
    for (int i = 0; i < nfields; i++) {
      tree_t field = type_field(type, i);
      type_t ftype = tree_type(field);
      const netid_t next = first + type_width(tree_type(field));
      if ((nid >= first) && (nid < next)) {
        if (type_is_array(ftype) || type_is_record(ftype)) {
          return (count + group_net_to_field(ftype, nid - first));
        } else {
          return (count);
        }
      }
      first = next;
      count += type_width(ftype);
    }
    fatal_trace("group_net_to_field failed to find field for nid=%d type=%s",
      nid, type_pp(type));
  } else if (type_is_array(type)) {
    type_t elem = type_elem(type);
    const int width = type_width(elem);
    if (type_is_record(elem)) {
      return ((nid / width) * width + group_net_to_field(elem, nid % width));
    } else {
      return (group_net_to_field(elem, nid % width));
    }
  } else {
    return (0);
  }
} /* group_net_to_field() */

/* ------------------------------------------------------------------------- */

static void
group_nets_visit_fn (
  tree_t t,
  void  *_ctx
) {
  group_nets_ctx_t *ctx = _ctx;

  switch (tree_kind(t))
  {
    case T_SIGNAL_ASSIGN:
      {
        group_target(tree_target(t), ctx);
      }
      break;

    case T_WAIT:
      {
        const int ntriggers = tree_triggers(t);
        for (int i = 0; i < ntriggers; i++) {
          group_target(tree_trigger(t, i), ctx);
        }
      }
      break;

    case T_PCALL:
      {
        ungroup_proc_params(t, ctx);
      }
      break;

    case T_SIGNAL_DECL:
      {
        // Ensure that no group is larger than a signal declaration
        group_decl(t, ctx, 0, -1);
      }
      break;

    default:
      {
      }
      break;
  }
} /* group_nets_visit_fn() */

/* ------------------------------------------------------------------------- */

static void
group_ref (
  tree_t            target,
  group_nets_ctx_t *ctx,
  int               start,
  int               n
) {
  assert(tree_kind(target) == T_REF);

  tree_t decl = tree_ref(target);
  switch (tree_kind(decl))
  {
    case T_SIGNAL_DECL:
      {
        group_decl(decl, ctx, start, n);
      }
      break;

    case T_ALIAS:
      {
        group_target(tree_value(decl), ctx);
      }
      break;

    default:
      {
      }
      break;
  }
} /* group_ref() */

/* ------------------------------------------------------------------------- */

static void
group_reuse (
  group_nets_ctx_t *ctx,
  group_t          *group
) {
  group->next = ctx->free_list;
  ctx->free_list = group;
} /* group_reuse() */

/* ------------------------------------------------------------------------- */

static void
group_target (
  tree_t            t,
  group_nets_ctx_t *ctx
) {
  switch (tree_kind(t))
  {
    case T_REF:
      {
        group_ref(t, ctx, 0, -1);
      }
      break;

    case T_ARRAY_REF:
    case T_ARRAY_SLICE:
    case T_RECORD_REF:
      {
        type_t type = tree_type(t);
        if (!type_known_width(type)) {
          ungroup_name(t, ctx);
        } else if (!group_name(t, ctx, 0, type_width(type))) {
          ungroup_name(t, ctx);
        }
      }
      break;

    case T_LITERAL:
    case T_OPEN:
      {
        // Constant folding can cause this to appear
      }
      break;

    case T_AGGREGATE:
      {
        const int nassocs = tree_assocs(t);
        for (int i = 0; i < nassocs; i++) {
          group_target(tree_value(tree_assoc(t, i)), ctx);
        }
      }
      break;

    default:
      fmt_loc(stdout, tree_loc(t));
      fatal_trace("Cannot handle tree kind %s in group_target",
        tree_kind_str(tree_kind(t)));
  }
} /* group_target() */

/* ------------------------------------------------------------------------- */

static void
group_unlink (
  group_nets_ctx_t *ctx,
  group_t          *where
) {
  where->gid = GROUPID_INVALID;

  if (where == ctx->groups) {
    ctx->groups = where->next;
  } else {
    for (group_t *it = ctx->groups; it != NULL; it = it->next) {
      if (it->next == where) {
        it->next = where->next;
        return;
      }
    }
    fatal_trace("unlink group not in list");
  }
} /* group_unlink() */

/* ------------------------------------------------------------------------- */

static void
group_write_netdb (
  tree_t            top,
  group_nets_ctx_t *ctx
) {
  char *name = xasprintf("_%s.netdb", istr(tree_ident(top)));

  fbuf_t *f = lib_fbuf_open(lib_work(), name, FBUF_OUT);

  if (f == NULL) {
    fatal("failed to create net database file %s", name);
  }

  free(name);

  for (group_t *it = ctx->groups; it != NULL; it = it->next) {
    write_u32(it->gid, f);
    write_u32(it->first, f);
    write_u32(it->length, f);
  }
  write_u32(GROUPID_INVALID, f);

  fbuf_close(f);
} /* group_write_netdb() */

/* ------------------------------------------------------------------------- */

static void
ungroup_name (
  tree_t            name,
  group_nets_ctx_t *ctx
) {
  switch (tree_kind(name))
  {
    case T_ARRAY_REF:
    case T_ARRAY_SLICE:
    case T_RECORD_REF:
      {
        ungroup_name(tree_value(name), ctx);
      }
      break;

    case T_REF:
      {
        ungroup_ref(name, ctx);
      }
      break;

    default:
      fatal_trace("cannot handle tree type %s in ungroup_name",
        tree_kind_str(tree_kind(name)));
  }
} /* ungroup_name() */

/* ------------------------------------------------------------------------- */

static void
ungroup_proc_params (
  tree_t            t,
  group_nets_ctx_t *ctx
) {
  // Ungroup any signal that is passed to a procedure as in general we
  // cannot guarantee anything about the procedure's behaviour

  const int nparams = tree_params(t);

  for (int i = 0; i < nparams; i++) {
    tree_t value = tree_value(tree_param(t, i));

    tree_kind_t kind = tree_kind(value);
    while (kind == T_ARRAY_REF || kind == T_ARRAY_SLICE) {
      value = tree_value(value);
      kind = tree_kind(value);
    }

    if (kind != T_REF) {
      continue;
    }

    tree_t decl = tree_ref(value);

    if (tree_kind(decl) != T_SIGNAL_DECL) {
      continue;
    }

    const int nnets = tree_nets(decl);
    for (int i = 0; i < nnets; i++) {
      group_add(ctx, tree_net(decl, i), 1);
    }
  }
} /* ungroup_proc_params() */

/* ------------------------------------------------------------------------- */

static void
ungroup_ref (
  tree_t            target,
  group_nets_ctx_t *ctx
) {
  tree_t decl = tree_ref(target);

  if (tree_kind(decl) == T_SIGNAL_DECL) {
    const int nnets = tree_nets(decl);
    for (int i = 0; i < nnets; i++) {
      group_add(ctx, tree_net(decl, i), 1);
    }
  }
} /* ungroup_ref() */

/* :vi set ts=2 et sw=2: */

