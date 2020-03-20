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
#include "hash.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

struct hash {
  unsigned     size;
  unsigned     members;
  bool         replace;
  void       **values;
  const void **keys;
};

/* ========================================================================= */
/* -- INTERNAL FUNCTION PROTOTYPES ----------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION PROTOTYPES ------------------------------------------- */
/* ========================================================================= */

static inline int hash_slot(hash_t *h, const void *key);

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
hash_free (
  hash_t *h
) {
  free(h->values);
  free(h);
} /* hash_free() */

/* ------------------------------------------------------------------------- */

void *
hash_get (
  hash_t     *h,
  const void *key
) {
  int n = 0;

  return (hash_get_nth(h, key, &n));
} /* hash_get() */

/* ------------------------------------------------------------------------- */

void *
hash_get_nth (
  hash_t     *h,
  const void *key,
  int        *n
) {
  int slot = hash_slot(h, key);

  for ( ; ; slot = (slot + 1) & (h->size - 1)) {
    if (h->keys[slot] == key) {
      if (*n == 0) {
        return (h->values[slot]);
      } else {
        --(*n);
      }
    } else if (h->keys[slot] == NULL) {
      return (NULL);
    }
  }
} /* hash_get_nth() */

/* ------------------------------------------------------------------------- */

bool
hash_iter (
  hash_t      *h,
  hash_iter_t *now,
  const void **key,
  void       **value
) {
  assert(*now != HASH_END);

  while (*now < h->size) {
    const unsigned old = (*now)++;
    if (h->keys[old] != NULL) {
      *key = h->keys[old];
      *value = h->values[old];
      return (true);
    }
  }

  *now = HASH_END;
  return (false);
} /* hash_iter() */

/* ------------------------------------------------------------------------- */

unsigned
hash_members (
  hash_t *h
) {
  return (h->members);
} /* hash_members() */

/* ------------------------------------------------------------------------- */

hash_t *
hash_new (
  int  size,
  bool replace
) {
  struct hash *h = xmalloc(sizeof(struct hash));

  h->size = next_power_of_2(size);
  h->members = 0;
  h->replace = replace;

  char *mem = xcalloc(h->size * 2 * sizeof(void *));
  h->values = (void **) mem;
  h->keys = (const void **) (mem + (h->size * sizeof(void *)));

  return (h);
} /* hash_new() */

/* ------------------------------------------------------------------------- */

bool
hash_put (
  hash_t     *h,
  const void *key,
  void       *value
) {
  if (unlikely(h->members >= h->size / 2)) {
    // Rebuild the hash table with a larger size
    // This is expensive so a conservative initial size should be chosen

    const int old_size = h->size;
    h->size *= 2;

    const void **old_keys = h->keys;
    void **old_values = h->values;

    char *mem = xcalloc(h->size * 2 * sizeof(void *));
    h->values = (void **) mem;
    h->keys = (const void **) (mem + (h->size * sizeof(void *)));

    h->members = 0;

    for (int i = 0; i < old_size; i++) {
      if (old_keys[i] != NULL) {
        hash_put(h, old_keys[i], old_values[i]);
      }
    }

    free(old_values);
  }

  int slot = hash_slot(h, key);

  for ( ; ; slot = (slot + 1) & (h->size - 1)) {
    if ((h->keys[slot] == key) && h->replace) {
      h->values[slot] = value;
      return (true);
    } else if (h->keys[slot] == NULL) {
      h->values[slot] = value;
      h->keys[slot] = key;
      h->members++;
      break;
    }
  }

  return (false);
} /* hash_put() */

/* ------------------------------------------------------------------------- */

void
hash_replace (
  hash_t *h,
  void   *value,
  void   *with
) {
  for (int i = 0; i < h->size; i++) {
    if (h->values[i] == value) {
      h->values[i] = with;
    }
  }
} /* hash_replace() */

/* ========================================================================= */
/* -- INTERNAL FUNCTION DEFINITIONS ---------------------------------------- */
/* ========================================================================= */

/* ========================================================================= */
/* -- STATIC FUNCTION DEFINITIONS ------------------------------------------ */
/* ========================================================================= */

static inline int
hash_slot (
  hash_t     *h,
  const void *key
) {
  assert(key != NULL);

  uintptr_t uptr = (uintptr_t) key;

  // Bottom two bits will always be zero with 32-bit pointers
  uptr >>= 2;

  // Hash function from here:
  //   http://burtleburtle.net/bob/hash/integer.html

  uint32_t a = (uint32_t) uptr;
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * UINT32_C(0x27d4eb2d);
  a = a ^ (a >> 15);

  return (a & (h->size - 1));
} /* hash_slot() */

/* :vi set ts=2 et sw=2: */

