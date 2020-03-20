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
#include "type.h"
#include "tree.h"
#include "util.h"
#include "array.h"
#include "common.h"
#include "object.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

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

/* ========================================================================= */
/* -- PRIVATE STRUCTURES --------------------------------------------------- */
/* ========================================================================= */

struct type {
  object_t object;
};

struct type_rd_ctx {
  tree_rd_ctx_t  tree_ctx;
  ident_rd_ctx_t ident_ctx;
  unsigned       n_types;
  type_t        *store;
  unsigned       store_sz;
};

/* ========================================================================= */
/* -- INTERNAL FUNCTION PROTOTYPES ----------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION PROTOTYPES ------------------------------------------- */
/* ========================================================================= */

static type_t type_make_universal(type_kind_t kind, const char *name,
  tree_t min, tree_t max);
static const char *type_minify_identity(const char *s);

/* ========================================================================= */
/* -- PRIVATE DATA --------------------------------------------------------- */
/* ========================================================================= */

static const imask_t has_map[T_LAST_TYPE_KIND] = {
  // T_UNRESOLVED
  (I_IDENT | I_RESOLUTION),

  // T_SUBTYPE
  (I_IDENT | I_BASE | I_RESOLUTION | I_CONSTR),

  // T_INTEGER
  (I_IDENT | I_DIMS),

  // T_REAL
  (I_IDENT | I_DIMS),

  // T_ENUM
  (I_IDENT | I_LITERALS | I_DIMS),

  // T_PHYSICAL
  (I_IDENT | I_UNITS | I_DIMS),

  // T_CARRAY
  (I_IDENT | I_ELEM | I_DIMS),

  // T_UARRAY
  (I_IDENT | I_INDEXCON | I_ELEM),

  // T_RECORD
  (I_IDENT | I_FIELDS),

  // T_FILE
  (I_IDENT | I_FILE),

  // T_ACCESS
  (I_IDENT | I_ACCESS),

  // T_FUNC
  (I_IDENT | I_PTYPES | I_RESULT | I_TEXT_BUF),

  // T_INCOMPLETE
  (I_IDENT),

  // T_PROC
  (I_IDENT | I_PTYPES | I_TEXT_BUF),

  // T_NONE
  (I_IDENT),

  // T_PROTECTED
  (I_IDENT | I_DECLS | I_REF)
};

static const char *kind_text_map[T_LAST_TYPE_KIND] = {
  "T_UNRESOLVED", "T_SUBTYPE",  "T_INTEGER", "T_REAL",
  "T_ENUM",       "T_PHYSICAL", "T_CARRAY",  "T_UARRAY",
  "T_RECORD",     "T_FILE",     "T_ACCESS",  "T_FUNC",
  "T_INCOMPLETE", "T_PROC",     "T_NONE",    "T_PROTECTED"
};

static const change_allowed_t change_allowed[] = {
  { T_INCOMPLETE, T_INTEGER  },
  { T_INCOMPLETE, T_REAL     },
  { T_INCOMPLETE, T_PHYSICAL },
  { T_INCOMPLETE, T_UARRAY   },
  { T_INCOMPLETE, T_RECORD   },
  { T_INCOMPLETE, T_ACCESS   },
  { T_INTEGER,    T_REAL     },
  { T_REAL,       T_INTEGER  },
  {           -1,         -1 }
};

/* ========================================================================= */
/* -- EXPORTED DATA -------------------------------------------------------- */
/* ========================================================================= */

object_class_t type_object = {
  .name           = "type",
  .change_allowed = change_allowed,
  .has_map        = has_map,
  .kind_text_map  = kind_text_map,
  .tag            = OBJECT_TAG_TYPE,
  .last_kind      = T_LAST_TYPE_KIND
};

/* ========================================================================= */
/* -- STATIC ASSERTIONS ---------------------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- EXPORTED FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

type_t
type_access (
  type_t t
) {
  if (t->object.kind == T_SUBTYPE) {
    return (type_access(type_base(t)));
  } else {
    return (lookup_item(&type_object, t, I_ACCESS)->type);
  }
} /* type_access() */

/* ------------------------------------------------------------------------- */

void
type_add_decl (
  type_t t,
  tree_t p
) {
  tree_array_add(&(lookup_item(&type_object, t, I_DECLS)->tree_array), p);
} /* type_add_decl() */

/* ------------------------------------------------------------------------- */

void
type_add_dim (
  type_t  t,
  range_t r
) {
  range_array_add(&(lookup_item(&type_object, t, I_DIMS)->range_array), r);
} /* type_add_dim() */

/* ------------------------------------------------------------------------- */

void
type_add_field (
  type_t t,
  tree_t p
) {
  assert(tree_kind(p) == T_FIELD_DECL);
  tree_array_add(&(lookup_item(&type_object, t, I_FIELDS)->tree_array), p);
} /* type_add_field() */

/* ------------------------------------------------------------------------- */

void
type_add_index_constr (
  type_t t,
  type_t c
) {
  type_array_add(&(lookup_item(&type_object, t, I_INDEXCON)->type_array), c);
} /* type_add_index_constr() */

/* ------------------------------------------------------------------------- */

void
type_add_param (
  type_t t,
  type_t p
) {
  type_array_add(&(lookup_item(&type_object, t, I_PTYPES)->type_array), p);
} /* type_add_param() */

/* ------------------------------------------------------------------------- */

void
type_add_unit (
  type_t t,
  tree_t u
) {
  tree_array_add(&(lookup_item(&type_object, t, I_UNITS)->tree_array), u);
} /* type_add_unit() */

/* ------------------------------------------------------------------------- */

type_t
type_base (
  type_t t
) {
  item_t *item = lookup_item(&type_object, t, I_BASE);

  assert(item->type != NULL);
  return (item->type);
} /* type_base() */

/* ------------------------------------------------------------------------- */

type_kind_t
type_base_kind (
  type_t t
) {
  assert(t != NULL);
  if (t->object.kind == T_SUBTYPE) {
    return (type_base_kind(type_base(t)));
  } else {
    return (t->object.kind);
  }
} /* type_base_kind() */

/* ------------------------------------------------------------------------- */

type_t
type_base_recur (
  type_t t
) {
  assert(t != NULL);
  while (t->object.kind == T_SUBTYPE) {
    t = type_base(t);
  }
  return (t);
} /* type_base_recur() */

/* ------------------------------------------------------------------------- */

tree_t
type_body (
  type_t t
) {
  assert(t->object.kind == T_PROTECTED);
  item_t *item = lookup_item(&type_object, t, I_REF);
  assert(item->tree);
  return (item->tree);
} /* type_body() */

/* ------------------------------------------------------------------------- */

void
type_change_dim (
  type_t   t,
  unsigned n,
  range_t  r
) {
  item_t *item = lookup_item(&type_object, t, I_DIMS);

  assert(n < item->range_array.count);
  item->range_array.items[n] = r;
} /* type_change_dim() */

/* ------------------------------------------------------------------------- */

void
type_change_index_constr (
  type_t   t,
  unsigned n,
  type_t   c
) {
  type_array_t *a = &(lookup_item(&type_object, t, I_INDEXCON)->type_array);

  assert(n < a->count);
  a->items[n] = c;
} /* type_change_index_constr() */

/* ------------------------------------------------------------------------- */

void
type_change_kind (
  type_t      t,
  type_kind_t kind
) {
  object_change_kind(&type_object, &(t->object), kind);
} /* type_change_kind() */

/* ------------------------------------------------------------------------- */

void
type_change_param (
  type_t   t,
  unsigned n,
  type_t   p
) {
  type_array_t *a = &(lookup_item(&type_object, t, I_PTYPES)->type_array);

  assert(n < a->count);
  a->items[n] = p;
} /* type_change_param() */

/* ------------------------------------------------------------------------- */

tree_t
type_constraint (
  type_t t
) {
  item_t *item = lookup_item(&type_object, t, I_CONSTR);

  assert(item->tree != NULL);
  return (item->tree);
} /* type_constraint() */

/* ------------------------------------------------------------------------- */

tree_t
type_decl (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_DECLS);

  return (tree_array_nth(&(item->tree_array), n));
} /* type_decl() */

/* ------------------------------------------------------------------------- */

unsigned
type_decls (
  type_t t
) {
  return (lookup_item(&type_object, t, I_DECLS)->tree_array.count);
} /* type_decls() */

/* ------------------------------------------------------------------------- */

range_t
type_dim (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_DIMS);

  return (range_array_nth(&(item->range_array), n));
} /* type_dim() */

/* ------------------------------------------------------------------------- */

unsigned
type_dims (
  type_t t
) {
  return (lookup_item(&type_object, t, I_DIMS)->range_array.count);
} /* type_dims() */

/* ------------------------------------------------------------------------- */

type_t
type_elem (
  type_t t
) {
  assert(t != NULL);

  if (t->object.kind == T_SUBTYPE) {
    return (type_elem(type_base(t)));
  } else {
    item_t *item = lookup_item(&type_object, t, I_ELEM);
    assert(item->type != NULL);
    return (item->type);
  }
} /* type_elem() */

/* ------------------------------------------------------------------------- */

void
type_enum_add_literal (
  type_t t,
  tree_t lit
) {
  assert(tree_kind(lit) == T_ENUM_LIT);
  tree_array_add(&(lookup_item(&type_object, t, I_LITERALS)->tree_array), lit);
} /* type_enum_add_literal() */

/* ------------------------------------------------------------------------- */

tree_t
type_enum_literal (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_LITERALS);

  return (tree_array_nth(&(item->tree_array), n));
} /* type_enum_literal() */

/* ------------------------------------------------------------------------- */

unsigned
type_enum_literals (
  type_t t
) {
  return (lookup_item(&type_object, t, I_LITERALS)->tree_array.count);
} /* type_enum_literals() */

/* ------------------------------------------------------------------------- */

bool
type_eq (
  type_t a,
  type_t b
) {
  assert(a != NULL);
  assert(b != NULL);

  if (a == b) {
    return (true);
  }

  type_kind_t kind_a = a->object.kind;
  type_kind_t kind_b = b->object.kind;

  if ((kind_a == T_UNRESOLVED) || (kind_b == T_UNRESOLVED)) {
    return (false);
  }

  // Subtypes are convertible to the base type
  while ((kind_a = a->object.kind) == T_SUBTYPE) {
    a = type_base(a);
  }
  while ((kind_b = b->object.kind) == T_SUBTYPE) {
    b = type_base(b);
  }

  const bool compare_c_u_arrays =
    (kind_a == T_CARRAY && kind_b == T_UARRAY) ||
    (kind_a == T_UARRAY && kind_b == T_CARRAY);

  if ((kind_a != kind_b) && !compare_c_u_arrays) {
    return (false);
  }

  // Universal integer type is equal to any other integer type
  type_t universal_int = type_universal_int();
  ident_t uint_i = type_ident(universal_int);
  if ((kind_a == T_INTEGER) &&
    ((type_ident(a) == uint_i) || (type_ident(b) == uint_i))) {
    return (true);
  }

  // Universal real type is equal to any other real type
  type_t universal_real = type_universal_real();
  ident_t ureal_i = type_ident(universal_real);
  if ((kind_a == T_REAL) &&
    ((type_ident(a) == ureal_i) || (type_ident(b) == ureal_i))) {
    return (true);
  }

  // XXX: this is not quite right as structurally equivalent types
  // may be declared in different scopes with the same name but
  // shouldn't compare equal

  if (type_has_ident(a) && type_has_ident(b)) {
    if (type_ident(a) != type_ident(b)) {
      return (false);
    }
  }

  // Access types are equal if the pointed to type is the same
  if (kind_a == T_ACCESS) {
    return (type_eq(type_access(a), type_access(b)));
  }

  if (compare_c_u_arrays) {
    return (type_eq(type_elem(a), type_elem(b)));
  }

  const imask_t has = has_map[a->object.kind];

  if ((has & I_DIMS) && (type_dims(a) != type_dims(b))) {
    return (false);
  }

  if (type_kind(a) == T_FUNC) {
    if (!type_eq(type_result(a), type_result(b))) {
      return (false);
    }
  }

  if (has & I_PTYPES) {
    if (type_params(a) != type_params(b)) {
      return (false);
    }

    const int nparams = type_params(a);
    for (int i = 0; i < nparams; i++) {
      if (!type_eq(type_param(a, i), type_param(b, i))) {
        return (false);
      }
    }
  }

  return (true);
} /* type_eq() */

/* ------------------------------------------------------------------------- */

tree_t
type_field (
  type_t   t,
  unsigned n
) {
  if (t->object.kind == T_SUBTYPE) {
    return (type_field(type_base(t), n));
  } else {
    item_t *item = lookup_item(&type_object, t, I_FIELDS);
    return (tree_array_nth(&(item->tree_array), n));
  }
} /* type_field() */

/* ------------------------------------------------------------------------- */

unsigned
type_fields (
  type_t t
) {
  if (t->object.kind == T_SUBTYPE) {
    return (type_fields(type_base(t)));
  } else {
    return (lookup_item(&type_object, t, I_FIELDS)->tree_array.count);
  }
} /* type_fields() */

/* ------------------------------------------------------------------------- */

type_t
type_file (
  type_t t
) {
  return (lookup_item(&type_object, t, I_FILE)->type);
} /* type_file() */

/* ------------------------------------------------------------------------- */

bool
type_has_body (
  type_t t
) {
  assert(t->object.kind == T_PROTECTED);
  item_t *item = lookup_item(&type_object, t, I_REF);
  return (item->tree != NULL);
} /* type_has_body() */

/* ------------------------------------------------------------------------- */

bool
type_has_constraint (
  type_t t
) {
  return (lookup_item(&type_object, t, I_CONSTR)->tree != NULL);
} /* type_has_constraint() */

/* ------------------------------------------------------------------------- */

bool
type_has_ident (
  type_t t
) {
  assert(t != NULL);
  return (lookup_item(&type_object, t, I_IDENT)->ident != NULL);
} /* type_has_ident() */

/* ------------------------------------------------------------------------- */

bool
type_has_resolution (
  type_t t
) {
  return (lookup_item(&type_object, t, I_RESOLUTION)->tree != NULL);
} /* type_has_resolution() */

/* ------------------------------------------------------------------------- */

ident_t
type_ident (
  type_t t
) {
  assert(t != NULL);

  item_t *item = lookup_item(&type_object, t, I_IDENT);
  if (item->ident == NULL) {
    switch (t->object.kind)
    {
      case T_SUBTYPE:
        {
          return (type_ident(type_base(t)));
        }

      case T_NONE:
        {
          return (ident_new("none"));
        }

      default:
        assert(false);
    }
  } else {
    return (item->ident);
  }
} /* type_ident() */

/* ------------------------------------------------------------------------- */

type_t
type_index_constr (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_INDEXCON);

  return (type_array_nth(&(item->type_array), n));
} /* type_index_constr() */

/* ------------------------------------------------------------------------- */

unsigned
type_index_constrs (
  type_t t
) {
  return (lookup_item(&type_object, t, I_INDEXCON)->type_array.count);
} /* type_index_constrs() */

/* ------------------------------------------------------------------------- */

bool
type_is_access (
  type_t t
) {
  return (type_base_kind(t) == T_ACCESS);
} /* type_is_access() */

/* ------------------------------------------------------------------------- */

bool
type_is_array (
  type_t t
) {
  const type_kind_t base = type_base_kind(t);

  return (base == T_CARRAY || base == T_UARRAY);
} /* type_is_array() */

/* ------------------------------------------------------------------------- */

bool
type_is_discrete (
  type_t t
) {
  const type_kind_t base = type_base_kind(t);

  return (base == T_INTEGER || base == T_ENUM);
} /* type_is_discrete() */

/* ------------------------------------------------------------------------- */

bool
type_is_enum (
  type_t t
) {
  return (type_base_kind(t) == T_ENUM);
} /* type_is_enum() */

/* ------------------------------------------------------------------------- */

bool
type_is_file (
  type_t t
) {
  return (type_base_kind(t) == T_FILE);
} /* type_is_file() */

/* ------------------------------------------------------------------------- */

bool
type_is_integer (
  type_t t
) {
  return (type_base_kind(t) == T_INTEGER);
} /* type_is_integer() */

/* ------------------------------------------------------------------------- */

bool
type_is_physical (
  type_t t
) {
  return (type_base_kind(t) == T_PHYSICAL);
} /* type_is_physical() */

/* ------------------------------------------------------------------------- */

bool
type_is_protected (
  type_t t
) {
  return (type_base_kind(t) == T_PROTECTED);
} /* type_is_protected() */

/* ------------------------------------------------------------------------- */

bool
type_is_real (
  type_t t
) {
  return (type_base_kind(t) == T_REAL);
} /* type_is_real() */

/* ------------------------------------------------------------------------- */

bool
type_is_record (
  type_t t
) {
  return (type_base_kind(t) == T_RECORD);
} /* type_is_record() */

/* ------------------------------------------------------------------------- */

bool
type_is_scalar (
  type_t t
) {
  const type_kind_t base = type_base_kind(t);

  return (base == T_INTEGER || base == T_REAL ||
         base == T_ENUM || base == T_PHYSICAL);
} /* type_is_scalar() */

/* ------------------------------------------------------------------------- */

bool
type_is_subprogram (
  type_t t
) {
  return (t->object.kind == T_FUNC || t->object.kind == T_PROC);
} /* type_is_subprogram() */

/* ------------------------------------------------------------------------- */

bool
type_is_unconstrained (
  type_t t
) {
  assert(t != NULL);
  if (t->object.kind == T_SUBTYPE) {
    if (!type_has_constraint(t)) {
      return (type_is_unconstrained(type_base(t)));
    } else {
      return (false);
    }
  } else {
    return (t->object.kind == T_UARRAY);
  }
} /* type_is_unconstrained() */

/* ------------------------------------------------------------------------- */

bool
type_is_universal (
  type_t t
) {
  assert(t != NULL);

  item_t *item = lookup_item(&type_object, t, I_IDENT);
  switch (t->object.kind)
  {
    case T_INTEGER:
      {
        return (item->ident == type_ident(type_universal_int()));
      }

    case T_REAL:
      {
        return (item->ident == type_ident(type_universal_real()));
      }

    default:
      return (false);
  }
} /* type_is_universal() */

/* ------------------------------------------------------------------------- */

type_kind_t
type_kind (
  type_t t
) {
  assert(t != NULL);

  return (t->object.kind);
} /* type_kind() */

/* ------------------------------------------------------------------------- */

const char *
type_kind_str (
  type_kind_t t
) {
  return (kind_text_map[t]);
} /* type_kind_str() */

/* ------------------------------------------------------------------------- */

bool
type_known_width (
  type_t type
) {
  if (!type_is_array(type)) {
    return (true);
  }

  if (type_is_unconstrained(type)) {
    return (false);
  }

  if (!type_known_width(type_elem(type))) {
    return (false);
  }

  const int ndims = array_dimension(type);
  for (int i = 0; i < ndims; i++) {
    int64_t low, high;
    if (!folded_bounds(range_of(type, i), &low, &high)) {
      return (false);
    }
  }

  return (true);
} /* type_known_width() */

/* ------------------------------------------------------------------------- */

type_t
type_new (
  type_kind_t kind
) {
  return ((type_t) object_new(&type_object, kind));
} /* type_new() */

/* ------------------------------------------------------------------------- */

type_t
type_param (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_PTYPES);

  return (type_array_nth(&(item->type_array), n));
} /* type_param() */

/* ------------------------------------------------------------------------- */

unsigned
type_params (
  type_t t
) {
  return (lookup_item(&type_object, t, I_PTYPES)->type_array.count);
} /* type_params() */

/* ------------------------------------------------------------------------- */

const char *
type_pp (
  type_t t
) {
  return (type_pp_minify(t, type_minify_identity));
} /* type_pp() */

/* ------------------------------------------------------------------------- */

const char *
type_pp_minify (
  type_t      t,
  minify_fn_t fn
) {
  assert(t != NULL);

  switch (type_kind(t))
  {
    case T_FUNC:
    case T_PROC:
      {
        item_t *tbi = lookup_item(&type_object, t, I_TEXT_BUF);
        if (tbi->text_buf == NULL) {
          tbi->text_buf = tb_new();
        } else {
          tb_rewind(tbi->text_buf);
        }

        if (type_has_ident(t)) {
          const char *fname = (*fn)(istr(type_ident(t)));
          tb_printf(tbi->text_buf, "%s ", fname);
        }
        tb_printf(tbi->text_buf, "[");
        const int nparams = type_params(t);
        for (int i = 0; i < nparams; i++) {
          tb_printf(tbi->text_buf, "%s%s",
            (i == 0 ? "" : ", "),
            (*fn)(istr(type_ident(type_param(t, i)))));
        }
        if (type_kind(t) == T_FUNC) {
          tb_printf(tbi->text_buf, "%sreturn %s",
            nparams > 0 ? " " : "",
            (*fn)(istr(type_ident(type_result(t)))));
        }
        tb_printf(tbi->text_buf, "]");

        return (tb_get(tbi->text_buf));
      }

    default:
      return ((*fn)(istr(type_ident(t))));
  }
} /* type_pp_minify() */

/* ------------------------------------------------------------------------- */

void
type_replace (
  type_t t,
  type_t a
) {
  assert(t != NULL);
  assert(t->object.kind == T_INCOMPLETE);

  object_replace(&(t->object), &(a->object));
} /* type_replace() */

/* ------------------------------------------------------------------------- */

tree_t
type_resolution (
  type_t t
) {
  item_t *item = lookup_item(&type_object, t, I_RESOLUTION);

  assert(item->tree != NULL);
  return (item->tree);
} /* type_resolution() */

/* ------------------------------------------------------------------------- */

type_t
type_result (
  type_t t
) {
  item_t *item = lookup_item(&type_object, t, I_RESULT);

  assert(item->type != NULL);
  return (item->type);
} /* type_result() */

/* ------------------------------------------------------------------------- */

void
type_set_access (
  type_t t,
  type_t a
) {
  lookup_item(&type_object, t, I_ACCESS)->type = a;
} /* type_set_access() */

/* ------------------------------------------------------------------------- */

void
type_set_base (
  type_t t,
  type_t b
) {
  lookup_item(&type_object, t, I_BASE)->type = b;
} /* type_set_base() */

/* ------------------------------------------------------------------------- */

void
type_set_body (
  type_t t,
  tree_t b
) {
  assert(t->object.kind == T_PROTECTED);
  item_t *item = lookup_item(&type_object, t, I_REF);
  item->tree = b;
} /* type_set_body() */

/* ------------------------------------------------------------------------- */

void
type_set_constraint (
  type_t t,
  tree_t c
) {
  lookup_item(&type_object, t, I_CONSTR)->tree = c;
} /* type_set_constraint() */

/* ------------------------------------------------------------------------- */

void
type_set_elem (
  type_t t,
  type_t e
) {
  lookup_item(&type_object, t, I_ELEM)->type = e;
} /* type_set_elem() */

/* ------------------------------------------------------------------------- */

void
type_set_file (
  type_t t,
  type_t f
) {
  lookup_item(&type_object, t, I_FILE)->type = f;
} /* type_set_file() */

/* ------------------------------------------------------------------------- */

void
type_set_ident (
  type_t  t,
  ident_t id
) {
  assert(t != NULL);
  lookup_item(&type_object, t, I_IDENT)->ident = id;
} /* type_set_ident() */

/* ------------------------------------------------------------------------- */

void
type_set_resolution (
  type_t t,
  tree_t r
) {
  lookup_item(&type_object, t, I_RESOLUTION)->tree = r;
} /* type_set_resolution() */

/* ------------------------------------------------------------------------- */

void
type_set_result (
  type_t t,
  type_t r
) {
  lookup_item(&type_object, t, I_RESULT)->type = r;
} /* type_set_result() */

/* ------------------------------------------------------------------------- */

bool
type_strict_eq (
  type_t a,
  type_t b
) {
  assert(a != NULL);
  assert(b != NULL);

  if (a == b) {
    return (true);
  }

  type_kind_t kind_a = a->object.kind;
  type_kind_t kind_b = b->object.kind;

  if ((kind_a == T_UNRESOLVED) || (kind_b == T_UNRESOLVED)) {
    return (false);
  }

  if (kind_a != kind_b) {
    return (false);
  }

  if (type_has_ident(a) && type_has_ident(b)) {
    if (type_ident(a) != type_ident(b)) {
      return (false);
    }
  }

  // Access types are equal if the pointed to type is the same
  if (kind_a == T_ACCESS) {
    return (type_eq(type_access(a), type_access(b)));
  }

  const imask_t has = has_map[a->object.kind];

  if (has & I_ELEM) {
    return (type_strict_eq(type_elem(a), type_elem(b)));
  }

  if ((has & I_DIMS) && (type_dims(a) != type_dims(b))) {
    return (false);
  }

  if (kind_a == T_FUNC) {
    if (!type_strict_eq(type_result(a), type_result(b))) {
      return (false);
    }
  }

  if (has & I_PTYPES) {
    if (type_params(a) != type_params(b)) {
      return (false);
    }

    const int nparams = type_params(a);
    for (int i = 0; i < nparams; i++) {
      if (!type_strict_eq(type_param(a, i), type_param(b, i))) {
        return (false);
      }
    }
  }

  return (true);
} /* type_strict_eq() */

/* ------------------------------------------------------------------------- */

tree_t
type_unit (
  type_t   t,
  unsigned n
) {
  item_t *item = lookup_item(&type_object, t, I_UNITS);

  return (tree_array_nth(&(item->tree_array), n));
} /* type_unit() */

/* ------------------------------------------------------------------------- */

unsigned
type_units (
  type_t t
) {
  return (lookup_item(&type_object, t, I_UNITS)->tree_array.count);
} /* type_units() */

/* ------------------------------------------------------------------------- */

type_t
type_universal_int (
  void
) {
  static type_t t = NULL;

  if (t == NULL) {
    tree_t min = tree_new(T_LITERAL);
    tree_set_subkind(min, L_INT);
    tree_set_ival(min, INT_MIN);

    tree_t max = tree_new(T_LITERAL);
    tree_set_subkind(max, L_INT);
    tree_set_ival(max, INT_MAX);

    t = type_make_universal(T_INTEGER, "universal integer", min, max);
  }

  return (t);
} /* type_universal_int() */

/* ------------------------------------------------------------------------- */

type_t
type_universal_real (
  void
) {
  static type_t t = NULL;

  if (t == NULL) {
    tree_t min = tree_new(T_LITERAL);
    tree_set_subkind(min, L_REAL);
    tree_set_dval(min, DBL_MIN);

    tree_t max = tree_new(T_LITERAL);
    tree_set_subkind(max, L_REAL);
    tree_set_dval(max, DBL_MAX);

    t = type_make_universal(T_REAL, "universal real", min, max);
  }

  return (t);
} /* type_universal_real() */

/* ------------------------------------------------------------------------- */

unsigned
type_width (
  type_t type
) {
  if (type_is_array(type)) {
    const unsigned elem_w = type_width(type_elem(type));
    unsigned w = 1;
    const int ndims = array_dimension(type);
    for (int i = 0; i < ndims; i++) {
      int64_t low, high;
      range_bounds(range_of(type, i), &low, &high);
      w *= MAX(high - low + 1, 0);
    }
    return (w * elem_w);
  } else if (type_is_record(type)) {
    type_t base = type_base_recur(type);
    unsigned w = 0;
    const int nfields = type_fields(base);
    for (int i = 0; i < nfields; i++) {
      w += type_width(tree_type(type_field(base, i)));
    }
    return (w);
  } else {
    return (1);
  }
} /* type_width() */

/* ========================================================================= */
/* -- INTERNAL FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION DEFINITIONS ------------------------------------------ */
/* ========================================================================= */

static type_t
type_make_universal (
  type_kind_t kind,
  const char *name,
  tree_t      min,
  tree_t      max
) {
  type_t t = type_new(kind);

  type_set_ident(t, ident_new(name));

  range_t r =
  {
    .kind  = RANGE_TO,
    .left  = min,
    .right = max
  };
  type_add_dim(t, r);

  tree_set_type(min, t);
  tree_set_type(max, t);

  return (t);
} /* type_make_universal() */

/* ------------------------------------------------------------------------- */

static const char *
type_minify_identity (
  const char *s
) {
  return (s);
} /* type_minify_identity() */

/* :vi set ts=2 et sw=2: */

