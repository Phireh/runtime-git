#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "common.h"

// TODO: Maybe do this with ftw.h
void delete_temp_dirs()
{
    int pid = fork();
    if (!pid)
    {
        // Redirect output to /dev/null to avoid messing the screen
        int fd = open("/dev/null", O_WRONLY);
        dup2(fd, 1);
        dup2(fd, 2);
        
        if (execl("/usr/bin/make", "make", "clean", (char*) NULL) == -1)
        {
            fprintf(stderr, "%s\n", strerror(errno));
        }
    }
    // Wait for removal to finish
    wait(0);
}

// Buffer to allocate the returned string in the function below
char tempdirname[PATH_MAX];

/* Dumps the entire contents of a tree into a temporal folder using a template for its filename.
   Said template has to end with "XXXXXX".
   
   NOTE: This takes the commit OID, not the tree OID
   
   Returns the actual filename of the temp dir on success, NULL on error
 */
char *dump_git_tree(char *template, git_oid oid, git_repository *repo)
{

    // Extract the tree OID from the commit OID
    git_commit *commit;
    if (git_commit_lookup(&commit, repo, &oid))
        return NULL;

    git_oid tree_oid = *git_commit_tree_id(commit);
    git_commit_free(commit);


    // Work on the tree from now on
    git_tree *tree;
    if (git_tree_lookup(&tree, repo, &tree_oid))
        return NULL;

    /* Create a tempdir from the template provided as an argument.
       We're using a temp buffer just for this function and letting the user
       provide a normal const char as parameter, so we need to copy it first */
    strcpy(tempdirname, template);
    if (!mkdtemp(tempdirname))
    {
        git_tree_free(tree);        
        fprintf(stderr, "TMPDIR creation failed with errno %d: %s\n", errno, strerror(errno));
        return NULL;
    }

    int n = git_tree_entrycount(tree);

    for (int i = 0; i < n; ++i)
    {
        const git_tree_entry *entry; 
        git_object *object;
            
        entry = git_tree_entry_byindex(tree, i);
        if (git_tree_entry_to_object(&object, repo, entry))
        {
            git_tree_free(tree);
            return NULL;
        }
        else
        {
            // We only want to copy file-like blobs
            if (git_object_type(object) == GIT_OBJECT_BLOB)
            {
                /* TODO: Add code for GIT_OBJECT_OFS_DELTA and GIT_OBJECT_REF_DELTA
                   if we are dealing with objects in deltified representation
                */
                git_blob *blob = (git_blob *) object;

                // Construct the filename of the new file: tempdir/filename
                char filepath[PATH_MAX];
                strcpy(filepath, tempdirname);                    
                strcat(filepath, "/");
                strcat(filepath, git_tree_entry_name(entry));

                // Actually write the file contents to the temp folder
                FILE *fp = fopen(filepath, "w");                    
                fwrite(git_blob_rawcontent(blob), (size_t)git_blob_rawsize(blob), 1, fp);
                fclose(fp);
            }
            git_object_free(object);
        }
    }
    git_tree_free(tree);

    return tempdirname;
}

int read_git_repo(git_repository *repo, commit_node_t **list_head)
{    
    git_revwalk *walker = NULL;
    git_commit *commit = NULL;
    commit_node_t *commit_node = NULL;
    git_oid oid;

    // We use a revwalker starting from HEAD to retrieve commits one by one
    git_revwalk_new(&walker, repo);
    git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL);
    git_revwalk_push_head(walker);

    // Traverse commit log until we walked all commits
    while (git_revwalk_next(&oid, walker) != GIT_ITEROVER)
    {
        if (!git_commit_lookup(&commit, repo, &oid))
        {
            const git_signature *commit_signature = git_commit_author(commit);

            if (commit_node)
            {
                // If we're on the rest of the list, reserve space and advance
                commit_node->next = (commit_node_t *) calloc(1, sizeof(commit_node_t));
                commit_node = commit_node->next;
            }
            else
            {
                // If we are at head, reserve space for first commit
                *list_head = (commit_node_t *) calloc(1, sizeof(commit_node_t));
                commit_node = *list_head;
            }

            // Copy data to our user-defined struct for easier management
            snprintf(commit_node->summary, 32, "%s", git_commit_summary(commit));
            snprintf(commit_node->author, 32, "%s", commit_signature->name);
            snprintf(commit_node->email, 32, "%s", commit_signature->email);
            snprintf(commit_node->date_as_string, 32, "%s", ctime(&commit_signature->when.time));
            commit_node->oid = oid;
            commit_node->tree_oid = *git_commit_tree_id(commit);
            
            // Print OID as hex values
            git_oid_tostr(commit_node->oid_as_string, GIT_OID_HEXSZ + 1 /* 41 */, &oid);
            

            // free temp git struct
            git_commit_free(commit);
        }
        else
        {
            // TODO: Error handling
            return 0;
        }
    }
    git_revwalk_free(walker);
    return 1;
}

void clean_and_exit(int exit_code)
{
    // Restore terminal defaults on exit
    endwin();
    exit(exit_code);
}

void *load_functions(game_code_t *code, char *filename)
{
    void *library_handle;

    library_handle = dlopen(filename, RTLD_NOW);
    if (!library_handle)
    {
        fprintf(stderr, "Error loading library: %s\n", dlerror());
        return NULL;
    }
    else
    {
        game_update_f *game_update = (game_update_f *) dlsym(library_handle, "game_update");
        char *error_string = dlerror();
        if (error_string)
        {
            fprintf(stderr, "Error loading function game_update\n");
            return NULL;
        }
        else
        {
            code->game_update = game_update;
        }

        game_render_f *game_render = dlsym(library_handle, "game_render");
        error_string = dlerror();
        if (error_string)
        {
            fprintf(stderr, "Error loading function game_render\n");
            return NULL;
        }
        else
        {
            code->game_render = game_render;
        }

        game_reset_f *game_reset = dlsym(library_handle, "game_reset");
        error_string = dlerror();
        if (error_string)
        {
            fprintf(stderr, "Error loading function game_reset\n");
            return NULL;
        }
        else
        {
            code->game_reset = game_reset;
        }
        return library_handle;
    }
}

int main()
{
    /** Initialization **/

    // Initialize libgit2
    git_libgit2_init();
    
    // Open the git repo
    if (git_repository_open_ext(&game_state.repo, "./", GIT_REPOSITORY_OPEN_NO_SEARCH, NULL))
    {
        fprintf(stderr, "Error opening git repo\n");
        exit(1);
    }


    // Copy commit info from the local git repo
    read_git_repo(game_state.repo, &game_state.commit_list);
    
    // Remember commit OID of HEAD
    game_state.platform_oid = game_state.commit_list->oid;
    game_state.game_oid = game_state.commit_list->oid;

    // Inject platform-independent code
    void *game_handle = load_functions(&game_code, "./game.so");
    if (!game_handle)
        goto cleanup;

    // Initialize curses library proper
    initscr();

    // Disable usual terminal features for maximum control
    raw(); // character input buffering
    noecho(); // character echoing
    nonl();   // return key line feed


    /** Window creation **/

    int h, w;
    getmaxyx(stdscr, h, w);
    
    WINDOW *window_handler = newwin(h, w, 0, 0);
    game_state.window = window_handler;
    nodelay(window_handler, TRUE);

    // Activate special key capturing (backspace, delete, etc) 
    keypad(window_handler, TRUE);    

    // Reserve space for the game state
    game_state.board = (uint8_t*) malloc(h*w*sizeof(uint8_t));
    
    
    // Record dimensions
    game_state.height = h;
    game_state.width = w;

    // Initiate flags
    game_state.flags = NORMAL;

    char debuginfo[128];
    game_state.debuginfo = debuginfo;
    
    /** Main loop **/
    for (;;)
    {
        // Fill debug info
        snprintf(debuginfo, sizeof(debuginfo), "ADDRESS OF GAME_CODE: %p", game_code.game_render);
        
        // Check if user wants to load a different commit for the game runtime
        if (memcmp((const void *)&game_state.selected_oid, (const void *)&empty_oid, GIT_OID_RAWSZ))
        {
            // Copy entire commit tree to temp folder
            char *tempdir = dump_git_tree("tempXXXXXX", game_state.game_oid, game_state.repo);

            int pid = fork();

            if (!pid) // we are the child process
            {
                char command[PATH_MAX];
                strcpy(command, "--directory=");
                strcat(command, "./");
                strcat(command, tempdir);
                
                // Redirect output to /dev/null to avoid messing the screen
                int fd = open("/dev/null", O_WRONLY);
                dup2(fd, 1);
                dup2(fd, 2);
                
                if (execl("/usr/bin/make", "/usr/bin/make", "-s", command, "game", (char*) NULL) == -1)
                {
                    fprintf(stderr, "%s\n", strerror(errno));
                    return -1;
                }
            }
            // Wait for compilation to finish
            wait(0);
            
            // Record change of runtime
            game_state.game_oid = game_state.selected_oid;

            // Clear selection
            game_state.selected_oid = empty_oid;
            game_state.inputn = 0;
            for (size_t i = 0; i < sizeof(game_state.inputfield); ++i)
                game_state.inputfield[i] = 0;

            // Close the handle to the previous' game lib code
            if (dlclose(game_handle))
            {
                fprintf(stderr, "Error closing game code handle\n");
                goto cleanup;
            }

            
            // Actually do the code injection
            char libpath[PATH_MAX] = "./";
            strcat(libpath, tempdir);
            strcat(libpath, "/game.so");            
            
            game_handle = load_functions(&game_code, libpath);
            if (!game_handle)
                goto cleanup;


            // Re-render in case the rendering function has changed
            game_code.game_render(&game_state);
        }
        /* NOTE: wgetch does an implicit wrefresh() if the window was last modified since it was
           last called, so we can avoid calling wrefresh() at all by ourselves */

        int ch = wgetch(window_handler);
        switch (ch)
        {
        case ERR:
            usleep(1000*33);
            game_code.game_render(&game_state);
            break;
        case 'q':
            goto cleanup;
            break;
        case 'r':
            game_code.game_reset(&game_state);
            game_code.game_render(&game_state);
            break;
        default:
            game_state.input = ch;
            game_code.game_update(&game_state);
            game_code.game_render(&game_state);
            break;
        }
    }
    
    /** Cleanup **/
cleanup:
    // Restore terminal defaults on exit
    
    // Clean unneeded files
    delete_temp_dirs();
    endwin();
    return 0;
}
