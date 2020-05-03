#ifndef HORSE64_SCOPE_H_
#define HORSE64_SCOPE_H_


typedef struct h64expression h64expression;
typedef struct h64scope h64scope;


typedef struct jsonvalue jsonvalue;

typedef struct hashmap hashmap;

typedef struct h64scopedef {
    h64expression *declarationexpr;
    const char *identifier;
    int everused;
} h64scopedef;

typedef struct h64scope {
    int definitionref_count;
    h64scopedef *definitionref;

    h64scope *parentscope;

    char hashkey[16];
    hashmap *name_to_declaration_map;
} h64scope;


int scope_Init(h64scope *scope, char hashkey[16]);

void scope_FreeData(h64scope *scope);

jsonvalue *scope_ScopeToJSON(
    h64scope *scope
);

#endif  // HORSE64_SCOPE_H_