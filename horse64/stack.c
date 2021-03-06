// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#include "compileconfig.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "stack.h"


h64stack *stack_New() {
    h64stack *st = malloc(sizeof(*st));
    if (!st)
        return NULL;
    memset(st, 0, sizeof(*st));

    return st;
}

void stack_FreeEntry(h64stack *st, int slot) {
    assert(st->entry[slot].type != H64VALTYPE_CONSTPREALLOCSTR);
    valuecontent_Free(&st->entry[slot]);
}

void stack_Free(h64stack *st) {
    if (!st)
        return;
    int64_t k = 0;
    while (k < st->entry_count) {
        stack_FreeEntry(st, k);
        k++;
    }
    free(st->entry);
    free(st);
}

void stack_Shrink(h64stack *st, int64_t total_entries) {
    int64_t i = st->entry_count;
    while (i > total_entries) {
        stack_FreeEntry(st, i - 1);
        i--;
    }
    st->entry_count = i;
    if (st->alloc_count + ALLOC_MAXOVERSHOOT > st->entry_count) {
        int64_t new_alloc = st->entry_count + ALLOC_OVERSHOOT;
        valuecontent *new_entries = realloc(
            st->entry, sizeof(*new_entries) * new_alloc
        );
        if (!new_entries) {
            return;
        }
        st->entry = new_entries;
        st->alloc_count = new_alloc;
    }
}

void stack_PrintDebug(h64stack *st) {
    fprintf(stderr, "=== STACK %p ===\n", st);
    fprintf(
        stderr, "* Total entries: %" PRId64
        ", alloc entries: %" PRId64 "\n",
        st->entry_count,
        st->alloc_count
    );
    int64_t k = 0;
    while (k < st->entry_count) {
        valuecontent *vc = &st->entry[k];
        fprintf(stderr, "%" PRId64 ": ", k);
        if (vc->type == H64VALTYPE_INT64) {
            fprintf(stderr, "%" PRId64, vc->int_value);
        } else if (vc->type == H64VALTYPE_FLOAT64) {
            fprintf(stderr, "%f", vc->float_value);
        } else if (vc->type == H64VALTYPE_BOOL) {
            fprintf(stderr, "%s", (vc->int_value ? "true" : "false"));
        } else if (vc->type == H64VALTYPE_GCVAL) {
            fprintf(stderr, "gcval %p", vc->ptr_value);
        } else {
            fprintf(stderr, "<value type %d>", (int)vc->type);
        }
        fprintf(stderr, "\n");
        k++;
    }
}

int stack_ToSize(
        h64stack *st,
        int64_t total_entries,
        int can_use_emergency_margin
        ) {
    assert(total_entries >= 0);
    int alloc_needed_margin = (
        can_use_emergency_margin ? 0 :
        ALLOC_EMERGENCY_MARGIN
    );
    // Grow if needed:
    if (unlikely(st->alloc_count < total_entries + alloc_needed_margin)) {
        valuecontent *new_entries = realloc(
            st->entry, sizeof(*new_entries) *
                (total_entries + alloc_needed_margin + ALLOC_OVERSHOOT)
        );
        if (!new_entries) {
            if (!can_use_emergency_margin ||
                    total_entries > st->alloc_count)
                return 0;
        } else {
            st->entry = new_entries;
            st->alloc_count = (
                total_entries + alloc_needed_margin + ALLOC_OVERSHOOT
            );
        }
    }
    if (total_entries < st->entry_count) {
        // Shrink or be done:
        stack_Shrink(st, total_entries);
        return 1;
    }
    assert(st->alloc_count > total_entries);
    int64_t k = st->entry_count;
    while (k < total_entries) {
        memset(&st->entry[k], 0, sizeof(st->entry[k]));
        k++;
    }
    st->entry_count = total_entries;
    return 1;
}
