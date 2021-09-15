#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdint.h>
#include <curses.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    NORMAL =   (1 << 0),
    GIT_MENU = (1 << 1),
    DEBUG_NEIGHBOURS = (1 << 2)
} game_flags_t;

/* Our own commit representation for comfier displaying. The only info we really
   need is the commit OID. */

struct commit_node_t;
typedef struct commit_node_t commit_node_t;

struct commit_node_t {
    commit_node_t *next;
    char summary[32];
    char author[32];
    char email[32];
    char date_as_string[32];
    git_oid oid;         // SHA-1 hash of GIT_OID_RAWSZ (20) Bytes         
    git_oid tree_oid;    // the hash of the tree referenced by this particular commit
    char oid_as_string[41]; // 20 oid Bytes * 2 chars to represent each byte + terminating \0
};

typedef struct {
    int32_t input;
    int32_t height;
    int32_t width;
    game_flags_t flags;    
    uint8_t *board;
    uint8_t *aux_board;
    
    WINDOW *window;
    char *debuginfo;

    git_repository *repo;    
    commit_node_t *commit_list;
    git_oid platform_oid;
    git_oid game_oid;
    git_oid selected_oid;

    char inputfield[32];
    size_t inputn;    
} game_state_t;

git_oid empty_oid = { .id = {} };



#define GAME_UPDATE(funcname) void funcname(game_state_t *g)
typedef GAME_UPDATE(game_update_f);

#define GAME_RESET(funcname) void funcname(game_state_t *g)
typedef GAME_RESET(game_reset_f);

#define GAME_RENDER(funcname) void funcname(game_state_t *g)
typedef GAME_RENDER(game_render_f);



typedef struct {
    game_update_f *game_update;
    game_reset_f *game_reset;
    game_render_f *game_render;
} game_code_t;

game_code_t game_code;
game_state_t game_state;

#endif
