// Copyright (c) 2020, ellie/@ell1e & Horse64 Team (see AUTHORS.md),
// also see LICENSE.md file.
// SPDX-License-Identifier: BSD-2-Clause

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "debugsymbols.h"
#include "corelib/errors.h"
#include "corelib/moduleless.h"
#include "gcvalue.h"
#include "hash.h"
#include "uri.h"


static char _name_itype_invalid[] = "invalid_instruction";
static char _name_itype_setconst[] = "setconst";
static char _name_itype_setglobal[] = "setglobal";
static char _name_itype_getglobal[] = "getglobal";
static char _name_itype_getfunc[] = "getfunc";
static char _name_itype_getclass[] = "getclass";
static char _name_itype_valuecopy[] = "valuecopy";
static char _name_itype_binop[] = "binop";
static char _name_itype_unop[] = "unop";
static char _name_itype_call[] = "call";
static char _name_itype_settop[] = "settop";
static char _name_itype_returnvalue[] = "returnvalue";
static char _name_itype_jumptarget[] = "jumptarget";
static char _name_itype_condjump[] = "condjump";
static char _name_itype_jump[] = "jump";
static char _name_itype_newiterator[] = "newiterator";
static char _name_itype_iterate[] = "iterate";
static char _name_itype_pushcatchframe[] = "pushcatchframe";
static char _name_itype_addcatchtypebyref[] = "addcatchtyperef";
static char _name_itype_addcatchtype[] = "addcatchtype";
static char _name_itype_popcatchframe[] = "popcatchframe";
static char _name_itype_getmember[] = "getmember";
static char _name_itype_jumptofinally[] = "jumptofinally";
static char _name_itype_newlist[] = "newlist";
static char _name_itype_addtolist[] = "addtolist";
static char _name_itype_newset[] = "newset";
static char _name_itype_addtoset[] = "addtoset";
static char _name_itype_newvector[] = "newvector";
static char _name_itype_putvector[] = "putvector";
static char _name_itype_newmap[] = "newmap";
static char _name_itype_putmap[] = "putmap";


const char *bytecode_InstructionTypeToStr(instructiontype itype) {
    switch (itype) {
    case H64INST_INVALID:
        return _name_itype_invalid;
    case H64INST_SETCONST:
        return _name_itype_setconst;
    case H64INST_SETGLOBAL:
        return _name_itype_setglobal;
    case H64INST_GETGLOBAL:
        return _name_itype_getglobal;
    case H64INST_GETFUNC:
        return _name_itype_getfunc;
    case H64INST_GETCLASS:
        return _name_itype_getclass;
    case H64INST_VALUECOPY:
        return _name_itype_valuecopy;
    case H64INST_BINOP:
        return _name_itype_binop;
    case H64INST_UNOP:
        return _name_itype_unop;
    case H64INST_CALL:
        return _name_itype_call;
    case H64INST_SETTOP:
        return _name_itype_settop;
    case H64INST_RETURNVALUE:
        return _name_itype_returnvalue;
    case H64INST_JUMPTARGET:
        return _name_itype_jumptarget;
    case H64INST_CONDJUMP:
        return _name_itype_condjump;
    case H64INST_JUMP:
        return _name_itype_jump;
    case H64INST_NEWITERATOR:
        return _name_itype_newiterator;
    case H64INST_ITERATE:
        return _name_itype_iterate;
    case H64INST_PUSHCATCHFRAME:
        return _name_itype_pushcatchframe;
    case H64INST_ADDCATCHTYPEBYREF:
        return _name_itype_addcatchtypebyref;
    case H64INST_ADDCATCHTYPE:
        return _name_itype_addcatchtype;
    case H64INST_POPCATCHFRAME:
        return _name_itype_popcatchframe;
    case H64INST_GETMEMBER:
        return _name_itype_getmember;
    case H64INST_JUMPTOFINALLY:
        return _name_itype_jumptofinally;
    case H64INST_NEWLIST:
        return _name_itype_newlist;
    case H64INST_ADDTOLIST:
        return _name_itype_addtolist;
    case H64INST_NEWSET:
        return _name_itype_newset;
    case H64INST_ADDTOSET:
        return _name_itype_addtoset;
    case H64INST_NEWVECTOR:
        return _name_itype_newvector;
    case H64INST_PUTVECTOR:
        return _name_itype_putvector;
    case H64INST_NEWMAP:
        return _name_itype_newmap;
    case H64INST_PUTMAP:
        return _name_itype_putmap;
    default:
        fprintf(stderr, "bytecode_InstructionTypeToStr: called "
                "on invalid value %d\n", itype);
        return _name_itype_invalid;
    }
}

h64program *h64program_New() {
    h64program *p = malloc(sizeof(*p));
    if (!p)
        return NULL;
    memset(p, 0, sizeof(*p));

    p->main_func_index = -1;
    p->globalinit_func_index = -1;
    p->to_str_name_index = -1;
    p->length_name_index = -1;
    p->init_name_index = -1;
    p->destroy_name_index = -1;
    p->clone_name_index = -1;
    p->equals_name_index = -1;
    p->hash_name_index = -1;

    p->symbols = h64debugsymbols_New();
    if (!p->symbols) {
        h64program_Free(p);
        return NULL;
    }
    p->symbols->program = p;

    if (!corelib_RegisterErrorClasses(p)) {
        h64program_Free(p);
        return NULL;
    }
    if (!corelib_RegisterFuncs(p)) {
        h64program_Free(p);
        return NULL;
    }

    return p;
}

int h64program_RegisterClassMemberEx(
        h64program *p,
        int64_t class_id,
        const char *name,
        int64_t func_idx
        ) {
    if (!p || !p->symbols)
        return 0;

    int64_t nameid = h64debugsymbols_MemberNameToMemberNameId(
        p->symbols, name, 1
    );
    if (nameid < 0)
        return 0;

    // Allocate bucket slot:
    assert(class_id >= 0 && class_id < p->classes_count);
    assert(p->classes[class_id].
           global_name_to_member_hashmap != NULL);
    int bucketindex = (nameid % (int64_t)H64CLASS_HASH_SIZE);
    h64classmemberinfo *buckets =
        (p->classes[class_id].
            global_name_to_member_hashmap[bucketindex]);
    int buckets_count = 0;
    while (buckets[buckets_count].nameid >= 0) {
        if (buckets[buckets_count].nameid == nameid)
            return 0;
        buckets_count++;
    }
    h64classmemberinfo *new_buckets = realloc(
        buckets, sizeof(*new_buckets) * (buckets_count + 2)
    );
    if (!new_buckets)
        return 0;
    p->classes[class_id].global_name_to_member_hashmap[
        bucketindex
    ] = new_buckets;
    buckets = new_buckets;

    // Allocate new slot for either methods or vars:
    int entry_idx = -1;
    if (func_idx >= 0) {
        if (p->classes[class_id].methods_count >=
                H64CLASS_MAX_METHODS)
            return 0;
        int64_t *new_method_global_name_idx = realloc(
            p->classes[class_id].method_global_name_idx,
            sizeof(*p->classes[class_id].
                   method_global_name_idx) *
            (p->classes[class_id].methods_count + 1)
        );
        if (!new_method_global_name_idx)
            return 0;
        p->classes[class_id].method_global_name_idx = (
            new_method_global_name_idx
        );
        int64_t *new_method_func_idx = realloc(
            p->classes[class_id].method_func_idx,
            sizeof(*p->classes[class_id].
                   method_func_idx) *
            (p->classes[class_id].methods_count + 1)
        );
        if (!new_method_func_idx)
            return 0;
        p->classes[class_id].method_func_idx = (
            new_method_func_idx
        );
        new_method_global_name_idx[
            p->classes[class_id].methods_count
        ] = nameid;
        new_method_func_idx[
            p->classes[class_id].methods_count
        ] = func_idx;
        p->classes[class_id].methods_count++;
        entry_idx = p->classes[class_id].methods_count - 1;
    } else {
        int64_t *new_vars_global_name_idx = realloc(
            p->classes[class_id].vars_global_name_idx,
            sizeof(*p->classes[class_id].
                   vars_global_name_idx) *
            (p->classes[class_id].vars_count + 1)
        );
        if (!new_vars_global_name_idx)
            return 0;
        p->classes[class_id].vars_global_name_idx = (
            new_vars_global_name_idx
        );
        new_vars_global_name_idx[
            p->classes[class_id].vars_count
        ] = nameid;
        p->classes[class_id].vars_count++;
        entry_idx = p->classes[class_id].vars_count - 1;
    }

    // Add into buckets:
    buckets[buckets_count + 1].nameid = -1;
    buckets[buckets_count + 1].methodorvaridx = -1;
    buckets[buckets_count].nameid = nameid;
    buckets[buckets_count].methodorvaridx = (
        func_idx > 0 ?
        entry_idx : (H64CLASS_MAX_METHODS + entry_idx)
    );
    return 1;
}

void h64program_LookupClassMember(
        h64program *p, int64_t class_id, int64_t nameid,
        int *out_membervarid, int *out_memberfuncid
        ) {
    assert(p != NULL && p->symbols != NULL);
    int bucketindex = (nameid % (int64_t)H64CLASS_HASH_SIZE);
    h64classmemberinfo *buckets =
        (p->classes[class_id].
            global_name_to_member_hashmap[bucketindex]);
    int i = 0;
    while (buckets[i].nameid >= 0) {
        if (buckets[i].nameid == nameid) {
            int64_t result = buckets[i].methodorvaridx;
            if (result < H64CLASS_MAX_METHODS) {
                *out_memberfuncid = result;
                *out_membervarid = -1;
            } else {
                *out_memberfuncid = -1;
                *out_membervarid = (result - H64CLASS_MAX_METHODS);
            }
            return;
        }
        i++;
    }
    *out_memberfuncid = -1;
    *out_membervarid = -1;
}

void h64program_LookupClassMemberByname(
        h64program *p, int64_t class_id, const char *name,
        int *out_membervarid, int *out_memberfuncid
        ) {
    int64_t nameid = h64debugsymbols_MemberNameToMemberNameId(
        p->symbols, name, 0
    );
    if (nameid < 0) {
        *out_membervarid = -1;
        *out_memberfuncid = -1;
    }
    return h64program_LookupClassMember(
        p, class_id, nameid, out_membervarid, out_memberfuncid
    );
}

void h64program_PrintBytecodeStats(h64program *p) {
    char _prefix[] = "horsec: info:";
    printf("%s bytecode func count: %" PRId64 "\n",
           _prefix, (int64_t)p->func_count);
    printf("%s bytecode global vars count: %" PRId64 "\n",
           _prefix, (int64_t)p->globals_count);
    printf("%s bytecode class count: %" PRId64 "\n",
           _prefix, (int64_t)p->classes_count);
    int i = 0;
    while (i < p->func_count) {
        const char _noname[] = "(unnamed)";
        const char *name = "(no symbols)";
        if (p->symbols) {
            h64funcsymbol *fsymbol = h64debugsymbols_GetFuncSymbolById(
                p->symbols, i
            );
            assert(fsymbol != NULL);
            name = fsymbol->name;
            if (!name) name = _noname;
        }
        char associatedclass[64] = "";
        if (p->func[i].associated_class_index >= 0) {
            snprintf(
                associatedclass, sizeof(associatedclass) - 1,
                " (CLASS: %d)", p->func[i].associated_class_index
            );
        }
        char instructioninfo[64] = "";
        if (!p->func[i].iscfunc && p->func[i].instructions_bytes > 0) {
            snprintf(instructioninfo, sizeof(instructioninfo),
                " code: %" PRId64 "B",
                (int64_t)p->func[i].instructions_bytes);
        }
        printf(
            "%s bytecode func id=%" PRId64 " "
            "name: \"%s\" cfunction: %d%s%s%s\n",
            _prefix, (int64_t)i, name, p->func[i].iscfunc,
            instructioninfo,
            (i == p->main_func_index ? " (PROGRAM START)" : ""),
            associatedclass
        );
        i++;
    }
    i = 0;
    while (i < p->classes_count) {
        const char *name = "(no symbols)";
        if (p->symbols) {
            h64classsymbol *csymbol = h64debugsymbols_GetClassSymbolById(
                p->symbols, i
            );
            assert(csymbol != NULL && csymbol->name != NULL);
            name = csymbol->name;
        }
        printf(
            "%s bytecode class id=%" PRId64 " "
            "name: \"%s\"\n",
            _prefix, (int64_t)i, name
        );
        i++;
    }
}

void valuecontent_Free(valuecontent *content) {
    if (!content)
        return;
    if (content->type == H64VALTYPE_CONSTPREALLOCSTR)
        free(content->constpreallocstr_value);
}

void h64program_FreeInstructions(
        char *instructionbytes,
        int instructionbytes_len
        ) {
    char *p = instructionbytes;
    int len = instructionbytes_len;
    while (len > 0) {
        assert(p != NULL);
        size_t nextelement = h64program_PtrToInstructionSize(p);
        h64instructionany *inst = (h64instructionany*)p;
        if (inst->type == H64INST_SETCONST) {
            h64instruction_setconst *instsetconst = (void *)p;
            valuecontent_Free(&instsetconst->content);
        }
        len -= (int)nextelement;
        p += (ptrdiff_t)nextelement;
    }
    free(instructionbytes);
}

size_t h64program_PtrToInstructionSize(char *ptr) {
    if (!ptr)
        return 0;
    h64instructionany *inst = (h64instructionany *)ptr;
    switch (inst->type) {
    case H64INST_SETCONST:
        return sizeof(h64instruction_setconst);
    case H64INST_SETGLOBAL:
        return sizeof(h64instruction_setglobal);
    case H64INST_GETGLOBAL:
        return sizeof(h64instruction_getglobal);
    case H64INST_GETFUNC:
        return sizeof(h64instruction_getfunc);
    case H64INST_GETCLASS:
        return sizeof(h64instruction_getclass);
    case H64INST_VALUECOPY:
        return sizeof(h64instruction_valuecopy);
    case H64INST_BINOP:
        return sizeof(h64instruction_binop);
    case H64INST_UNOP:
        return sizeof(h64instruction_unop);
    case H64INST_CALL:
        return sizeof(h64instruction_call);
    case H64INST_SETTOP:
        return sizeof(h64instruction_settop);
    case H64INST_RETURNVALUE:
        return sizeof(h64instruction_returnvalue);
    case H64INST_JUMPTARGET:
        return sizeof(h64instruction_jumptarget);
    case H64INST_CONDJUMP:
        return sizeof(h64instruction_condjump);
    case H64INST_JUMP:
        return sizeof(h64instruction_jump);
    case H64INST_NEWITERATOR:
        return sizeof(h64instruction_newiterator);
    case H64INST_ITERATE:
        return sizeof(h64instruction_iterate);
    case H64INST_PUSHCATCHFRAME:
        return sizeof(h64instruction_pushcatchframe);
    case H64INST_ADDCATCHTYPEBYREF:
        return sizeof(h64instruction_addcatchtypebyref);
    case H64INST_ADDCATCHTYPE:
        return sizeof(h64instruction_addcatchtype);
    case H64INST_POPCATCHFRAME:
        return sizeof(h64instruction_popcatchframe);
    case H64INST_GETMEMBER:
        return sizeof(h64instruction_getmember);
    case H64INST_JUMPTOFINALLY:
        return sizeof(h64instruction_jumptofinally);
    case H64INST_NEWLIST:
        return sizeof(h64instruction_newlist);
    case H64INST_ADDTOLIST:
        return sizeof(h64instruction_addtolist);
    case H64INST_NEWSET:
        return sizeof(h64instruction_newset);
    case H64INST_ADDTOSET:
        return sizeof(h64instruction_addtoset);
    case H64INST_NEWVECTOR:
        return sizeof(h64instruction_newvector);
    case H64INST_PUTVECTOR:
        return sizeof(h64instruction_putvector);
    case H64INST_NEWMAP:
        return sizeof(h64instruction_newmap);
    case H64INST_PUTMAP:
        return sizeof(h64instruction_putmap);
    default:
        fprintf(
            stderr, "Invalid inst type for "
            "h64program_PtrToInstructionSize: %d\n",
            (int)inst->type
        );
        assert(0);
    }
    return 0;
}

void h64program_Free(h64program *p) {
    if (!p)
        return;

    if (p->symbols)
        h64debugsymbols_Free(p->symbols);
    if (p->classes) {
        int i = 0;
        while (i < p->classes_count) {
            if (p->classes[i].global_name_to_member_hashmap) {
                int k = 0;
                while (k < H64CLASS_HASH_SIZE) {
                    free(p->classes[i].
                         global_name_to_member_hashmap[k]);
                    k++;
                }
                free(p->classes[i].global_name_to_member_hashmap);
            }
            free(p->classes[i].method_func_idx);
            free(p->classes[i].method_global_name_idx);
            free(p->classes[i].vars_global_name_idx);
            i++;
        }
    }
    free(p->classes);
    if (p->func) {
        int i = 0;
        while (i < p->func_count) {
            free(p->func[i].cfunclookup);
            if (!p->func[i].iscfunc) {
                assert(p->func[i].instructions ||
                       p->func[i].instructions_bytes == 0);
                h64program_FreeInstructions(
                    p->func[i].instructions,
                    p->func[i].instructions_bytes
                );
            }
            i++;
        }
    }
    free(p->func);
    int i = 0;
    while (i < p->globalvar_count) {
        h64program_ClearValueContent(
            &p->globalvar[i].content, 0
        );
        i++;
    }
    free(p->globalvar);

    free(p);
}

int bytecode_fileuriindex(h64program *p, const char *fileuri) {
    char *normalized_uri = uri_Normalize(fileuri, 1);
    if (!normalized_uri)
        return -1;
    int fileuriindex = -1;
    int foundindex = 0;
    int k = 0;
    while (k > p->symbols->fileuri_count) {
        if (strcmp(p->symbols->fileuri[k], normalized_uri) == 0) {
            fileuriindex = k;
            break;
        }
        k++;
    }
    if (fileuriindex < 0) {
        char **new_fileuri = realloc(
            p->symbols->fileuri, sizeof(*new_fileuri) *
            (p->symbols->fileuri_count + 1)
        );
        if (!new_fileuri) {
            free(normalized_uri);
            return -1;
        }
        p->symbols->fileuri = new_fileuri;
        p->symbols->fileuri[p->symbols->fileuri_count] =
            normalized_uri;
        fileuriindex = p->symbols->fileuri_count;
        p->symbols->fileuri_count++;
        normalized_uri = NULL;
    }
    return fileuriindex;
}

int h64program_AddGlobalvar(
        h64program *p,
        const char *name,
        int is_const,
        const char *fileuri,
        const char *module_path,
        const char *library_name
        ) {
    assert(p != NULL && p->symbols != NULL && name != NULL);
    h64globalvar *new_globalvar = realloc(
        p->globalvar, sizeof(*p->globalvar) * (p->globalvar_count + 1)
    );
    if (!new_globalvar)
        return -1;
    p->globalvar = new_globalvar;
    memset(&p->globalvar[p->globalvar_count], 0, sizeof(*p->globalvar));

    int fileuriindex = -1;
    if (fileuri) {
        fileuriindex = bytecode_fileuriindex(p, fileuri);
        if (fileuriindex < 0)
            return -1;
    }

    h64modulesymbols *msymbols = NULL;
    if (module_path) {
        msymbols = h64debugsymbols_GetModule(
            p->symbols, module_path, library_name, 1
        );
        if (!msymbols)
            return -1;
    } else {
        assert(library_name == NULL);
        msymbols = h64debugsymbols_GetBuiltinModule(p->symbols);
        assert(msymbols != NULL);
    }

    // Add to the globalvar symbols table:
    h64globalvarsymbol *new_globalvar_symbols = realloc(
        msymbols->globalvar_symbols,
        sizeof(*msymbols->globalvar_symbols) * (
            msymbols->globalvar_count + 1
        ));
    if (!new_globalvar_symbols)
        return -1;
    msymbols->globalvar_symbols = new_globalvar_symbols;
    memset(&msymbols->globalvar_symbols[msymbols->globalvar_count],
        0, sizeof(*msymbols->globalvar_symbols));
    msymbols->globalvar_symbols[msymbols->globalvar_count].name = (
        strdup(name)
    );
    if (!msymbols->globalvar_symbols[msymbols->globalvar_count].name) {
        globalvarsymboloom:
        h64debugsymbols_ClearGlobalvarSymbol(
            &msymbols->globalvar_symbols[msymbols->globalvar_count]
        );
        return -1;
    }
    msymbols->globalvar_symbols[msymbols->globalvar_count].
        fileuri_index = fileuriindex;
    msymbols->globalvar_symbols[msymbols->globalvar_count].
        is_const = is_const;

    // Add globals to lookup-by-name hash table:
    uint64_t setno = msymbols->globalvar_count;
    assert(msymbols->globalvar_name_to_entry != NULL);
    if (!hash_StringMapSet(
            msymbols->globalvar_name_to_entry,
            name, setno)) {
        goto globalvarsymboloom;
    }

    // Add actual globalvar entry:
    memset(
        &p->globalvar[p->globalvar_count], 0,
        sizeof(p->globalvar[p->globalvar_count])
    );

    p->globalvar_count++;
    msymbols->globalvar_count++;

    return p->globalvar_count - 1;
}

int h64program_RegisterCFunction(
        h64program *p,
        const char *name,
        int (*func)(h64vmthread *vmthread),
        const char *fileuri,
        int arg_count,
        char **arg_kwarg_name,
        int last_is_multiarg,
        const char *module_path,
        const char *library_name,
        int is_threadable,
        int associated_class_index
        ) {
    assert(p != NULL && p->symbols != NULL);
    assert(name != NULL || associated_class_index < 0);
    h64func *new_func = realloc(
        p->func, sizeof(*p->func) * (p->func_count + 1)
    );
    if (!new_func)
        return -1;
    p->func = new_func;
    memset(&p->func[p->func_count], 0, sizeof(*p->func));

    int fileuriindex = -1;
    if (fileuri) {
        int fileuriindex = bytecode_fileuriindex(p, fileuri);
        if (fileuriindex < 0)
            return -1;
    }

    char *cfunclookup = NULL;
    if (func != NULL) {
        assert(name != NULL);
        cfunclookup = malloc(
            (module_path ? strlen(module_path) : strlen("$$builtin")) +
            1 + strlen(name) +
            strlen("@lib:") +
            (library_name ? strlen(library_name) : 0) + 1
        );
        if (!cfunclookup)
            return -1;
        cfunclookup[0] = '\0';
        char _builtinpath[] = "$$builtin";
        const char *writemodpath = module_path;
        if (writemodpath == NULL || strlen(writemodpath) == 0)
            writemodpath = _builtinpath;
        memcpy(cfunclookup, writemodpath, strlen(writemodpath));
        cfunclookup[strlen(writemodpath)] = '.';
        cfunclookup[strlen(writemodpath) + 1] = '\0';
        memcpy(cfunclookup + strlen(cfunclookup), name, strlen(name) + 1);
        if (library_name && strlen(library_name) > 0) {
            memcpy(cfunclookup + strlen(cfunclookup),
                   "@lib:", strlen("@lib:") + 1);
            memcpy(cfunclookup + strlen(cfunclookup), library_name,
                   strlen(library_name) + 1);
        }
    }

    h64modulesymbols *msymbols = NULL;
    if (module_path) {
        msymbols = h64debugsymbols_GetModule(
            p->symbols, module_path, library_name, 1
        );
        if (!msymbols)
            return -1;
    } else {
        assert(library_name == NULL);
        msymbols = h64debugsymbols_GetBuiltinModule(p->symbols);
        assert(msymbols != NULL);
    }

    // Add to the func symbols table:
    h64funcsymbol *new_func_symbols = realloc(
        msymbols->func_symbols,
        sizeof(*msymbols->func_symbols) * (
            msymbols->func_count + 1
        ));
    if (!new_func_symbols)
        return -1;
    msymbols->func_symbols = new_func_symbols;
    memset(&msymbols->func_symbols[msymbols->func_count],
        0, sizeof(*msymbols->func_symbols));
    if (name) {
        msymbols->func_symbols[msymbols->func_count].name = (
            strdup(name)
        );
    }
    msymbols->func_symbols[msymbols->func_count].
        fileuri_index = fileuriindex;
    if (name && !msymbols->func_symbols[msymbols->func_count].name) {
        funcsymboloom:
        if (cfunclookup)
            free(cfunclookup);
        if (name)
            hash_StringMapUnset(
                msymbols->func_name_to_entry, name
            );
        if (p->symbols) {
            hash_IntMapUnset(
                p->symbols->func_id_to_module_symbols_index,
                p->func_count
            );
            hash_IntMapUnset(
                p->symbols->func_id_to_module_symbols_func_subindex,
                p->func_count
            );
        }
        h64debugsymbols_ClearFuncSymbol(
            &msymbols->func_symbols[msymbols->func_count]
        );
        return -1;
    }
    msymbols->func_symbols[msymbols->func_count].has_self_arg = (
        associated_class_index >= 0
    );
    msymbols->func_symbols[msymbols->func_count].arg_count = arg_count;
    if (arg_count > 0) {
        msymbols->func_symbols[msymbols->func_count].
                arg_kwarg_name = (
            malloc(sizeof(*msymbols->func_symbols[msymbols->func_count].
                arg_kwarg_name) * arg_count));
        if (!msymbols->func_symbols[msymbols->func_count].
                arg_kwarg_name)
            goto funcsymboloom;
        memset(
            msymbols->func_symbols[msymbols->func_count].
            arg_kwarg_name, 0,
            sizeof(*msymbols->func_symbols[msymbols->func_count].
                arg_kwarg_name) * arg_count);
        int i = 0;
        while (i < arg_count) {
            msymbols->func_symbols[msymbols->func_count].
                arg_kwarg_name[i] = (
                (arg_kwarg_name && arg_kwarg_name[i]) ?
                 strdup(arg_kwarg_name[i]) : NULL
                );
            if (arg_kwarg_name && arg_kwarg_name[i] &&
                    msymbols->func_symbols[msymbols->func_count].
                    arg_kwarg_name[i] == NULL)
                goto funcsymboloom;
            i++;
        }
    }

    // Add function to lookup-by-name hash table:
    uint64_t setno = msymbols->func_count;
    if (name && !hash_StringMapSet(
            msymbols->func_name_to_entry,
            name, setno)) {
        goto funcsymboloom;
    }

    // Add it to lookups from func id to debug symbols:
    if (p->symbols && !hash_IntMapSet(
            p->symbols->func_id_to_module_symbols_index, p->func_count,
            (uint64_t)msymbols->index)) {
        goto funcsymboloom;
    }
    if (p->symbols && !hash_IntMapSet(
            p->symbols->func_id_to_module_symbols_func_subindex,
            p->func_count,
            (uint64_t)msymbols->func_count)) {
        goto funcsymboloom;
    }

    // Register function as class method if it is one:
    if (associated_class_index >= 0) {
        if (!h64program_RegisterClassMemberEx(
                p, associated_class_index,
                name, p->func_count
                )) {
            goto funcsymboloom;
        }
    }

    // Add actual function entry:
    p->func[p->func_count].input_stack_size = (
        arg_count + (associated_class_index >= 0 ? 1 : 0)
    );
    p->func[p->func_count].is_threadable = is_threadable;
    p->func[p->func_count].iscfunc = 1;
    p->func[p->func_count].associated_class_index = (
        associated_class_index
    );
    p->func[p->func_count].cfunclookup = cfunclookup;
    p->func[p->func_count].cfunc_ptr = func;
    msymbols->func_symbols[msymbols->func_count].global_id = p->func_count;
    msymbols->func_symbols[msymbols->func_count].arg_count = arg_count;
    msymbols->func_symbols[msymbols->func_count].
        last_arg_is_multiarg = last_is_multiarg;

    p->func_count++;
    msymbols->func_count++;

    return p->func_count - 1;
}

int h64program_RegisterHorse64Function(
        h64program *p,
        const char *name,
        const char *fileuri,
        int arg_count,
        char **arg_kwarg_name,
        int last_is_multiarg,
        const char *module_path,
        const char *library_name,
        int associated_class_idx
        ) {
    int idx = h64program_RegisterCFunction(
        p, name, NULL, fileuri, arg_count, arg_kwarg_name,
        last_is_multiarg, module_path,
        library_name, -1, associated_class_idx
    );
    if (idx >= 0) {
        p->func[idx].iscfunc = 0;
    }
    return idx;
}

int h64program_AddClass(
        h64program *p,
        const char *name,
        const char *fileuri,
        const char *module_path,
        const char *library_name
        ) {
    assert(p != NULL && p->symbols != NULL);
    h64class *new_classes = realloc(
        p->classes, sizeof(*p->classes) * (p->classes_count + 1)
    );
    if (!new_classes)
        return -1;
    p->classes = new_classes;
    memset(&p->classes[p->classes_count], 0, sizeof(*p->classes));
    p->classes[p->classes_count].base_class_global_id = -1;

    int fileuriindex = -1;
    if (fileuri) {
        int fileuriindex = bytecode_fileuriindex(p, fileuri);
        if (fileuriindex < 0)
            return -1;
    }

    h64modulesymbols *msymbols = NULL;
    if (module_path) {
        msymbols = h64debugsymbols_GetModule(
            p->symbols, module_path, library_name, 1
        );
        if (!msymbols)
            return -1;
    } else {
        assert(library_name == NULL);
        msymbols = h64debugsymbols_GetBuiltinModule(p->symbols);
        assert(msymbols != NULL);
    }

    // Add to the class symbols table:
    h64classsymbol *new_classes_symbols = realloc(
        msymbols->classes_symbols,
        sizeof(*msymbols->classes_symbols) * (
            msymbols->classes_count + 1
        ));
    if (!new_classes_symbols)
        return -1;
    msymbols->classes_symbols = new_classes_symbols;
    memset(&msymbols->classes_symbols[msymbols->classes_count],
        0, sizeof(*msymbols->classes_symbols));
    msymbols->classes_symbols[msymbols->classes_count].name = (
        strdup(name)
    );
    if (!msymbols->classes_symbols[msymbols->classes_count].name) {
        classsymboloom:
        if (name)
            hash_StringMapUnset(
                msymbols->class_name_to_entry, name
            );
        if (p->symbols) {
            hash_IntMapUnset(
                p->symbols->class_id_to_module_symbols_index,
                p->func_count
            );
            hash_IntMapUnset(
                p->symbols->class_id_to_module_symbols_class_subindex,
                p->func_count
            );
        }
        if (p->classes[p->classes_count].global_name_to_member_hashmap) {
            int i = 0;
            while (i < H64CLASS_HASH_SIZE) {
                free(p->classes[p->classes_count].
                     global_name_to_member_hashmap[i]);
                i++;
            }
            free(p->classes[p->classes_count].
                 global_name_to_member_hashmap);
        }
        h64debugsymbols_ClearClassSymbol(
            &msymbols->classes_symbols[msymbols->classes_count]
        );
        return -1;
    }
    msymbols->classes_symbols[msymbols->classes_count].
        fileuri_index = fileuriindex;

    // Add class to lookup-by-name hash table:
    uint64_t setno = msymbols->classes_count;
    if (!hash_StringMapSet(
            msymbols->class_name_to_entry,
            name, setno)) {
        goto classsymboloom;
    }

    // Add it to lookups from class id to debug symbols:
    if (p->symbols && !hash_IntMapSet(
            p->symbols->class_id_to_module_symbols_index, p->classes_count,
            (uint64_t)msymbols->index)) {
        goto classsymboloom;
    }
    if (p->symbols && !hash_IntMapSet(
            p->symbols->class_id_to_module_symbols_class_subindex,
            p->classes_count,
            (uint64_t)msymbols->classes_count)) {
        goto classsymboloom;
    }

    // Add actual class entry:
    p->classes[p->classes_count].global_name_to_member_hashmap = malloc(
        H64CLASS_HASH_SIZE * sizeof(
            *p->classes[p->classes_count].global_name_to_member_hashmap
        )
    );
    if (!p->classes[p->classes_count].global_name_to_member_hashmap)
        goto classsymboloom;
    memset(
        p->classes[p->classes_count].global_name_to_member_hashmap,
        0, H64CLASS_HASH_SIZE * sizeof(
            *p->classes[p->classes_count].global_name_to_member_hashmap
        )
    );
    int i = 0;
    while (i < H64CLASS_HASH_SIZE) {
        p->classes[p->classes_count].global_name_to_member_hashmap[i] =
            malloc(
                sizeof(**(p->classes[p->classes_count].
                global_name_to_member_hashmap))
            );
        if (!p->classes[p->classes_count].
                global_name_to_member_hashmap[i]) {
            goto classsymboloom;
        }
        p->classes[p->classes_count].
            global_name_to_member_hashmap[i][0].nameid = -1;
        p->classes[p->classes_count].
            global_name_to_member_hashmap[i][0].methodorvaridx = -1;
        i++;
    }

    p->classes_count++;
    msymbols->classes_count++;

    return p->classes_count - 1;
}

int h64program_RegisterClassVariable(
        h64program *p,
        int64_t class_id,
        const char *name
        ) {
    return h64program_RegisterClassMemberEx(
        p, class_id, name, -1
    );
}
