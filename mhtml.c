#include "handle_heap/mm_handles.h"
#include "hstack.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/*Simple handle based HTML parser..........*/

typedef struct{
    mm_handle vector; //mm_handle -> HTMLNode*
    int cappacity; //default to 8
    int size;
}handle_vector;

typedef struct{
    mm_handle parent; //HTMLNode* (can be invalid)

    handle_vector children; //HTMLNode_vector
    int children_pos;

    mm_handle attributes; //attribute string, char* (can be invalid)
    mm_handle inner_text; //Node text, char* (can be invalid)

    mm_handle tag; //char* (can not be invalid)
}HTMLNode;

void handle_vector_init(handle_vector* vector){
    vector->cappacity = 8;
    vector->size = 0;
    vector->vector = mm_alloc(vector->cappacity * sizeof(vector->vector));

    assert(mm_lock(vector->vector)); //check that alloc is successful
    mm_unlock(vector->vector);

    mm_set_metadata(vector->vector,MM_HANDLE_SERIALIZE_FUNC,0xdeadbeef);
}

mm_handle handle_vector_get(handle_vector* vector, int pos){
    mm_handle ret = {0};
    if(vector->size > pos){
        mm_handle* hvector = mm_lock(vector->vector);
        ret = hvector[pos];
        mm_unlock(vector->vector);
    }
    return ret;
}

void handle_vector_set(handle_vector* vector, int pos, mm_handle handle){
    if(vector->cappacity <= pos){
        vector->cappacity += vector->cappacity / 2;
        vector->vector = mm_realloc(vector->vector,sizeof(mm_handle) * vector->cappacity);

        assert(mm_lock(vector->vector));
        mm_unlock(vector->vector);
    }

    mm_handle* hvector = mm_lock(vector->vector);
    hvector[pos] = handle;
    vector->size = pos + 1;
    mm_unlock(vector->vector);
}

void handle_vector_destroy(handle_vector* vector){
    mm_free(vector->vector);
}

int handle_vector_size(handle_vector* vector){
    return vector->size;
}

static char* void_nodes[] = { //list of all void nodes given by chatGPT
    "area",
    "base",
    "br",
    "col",
    "embed",
    "hr",
    "img",
    "input",
    "link",
    "meta",
    "param",
    "source",
    "track",
    "wbr"
};

//WARNING: TODO: fix large strings!!!!!!!!!!!!
mm_handle HTMLParse(char* html){
    hstack node_stack = {0};
    mm_handle root_obj = {0};

    char lex[768] = {0};
    uint8_t lexi = 0;

    for(; *html; html++){
        if(*html == '<'){
            html++;

            if(*html == '/'){
                html++;
                mm_handle pop_handle = hstack_pop(&node_stack);
                HTMLNode* node = mm_lock(pop_handle);
                if(node){
                    int error = 1;
                    char* tag = mm_lock(node->tag);
                    if(tag){
                        lexi = 0;
                        while(*html != '>' && *html){
                            lex[lexi++] = *(html++);
                        }
                        lex[lexi] = '\0';

                        if(strncmp(tag,lex,lexi) == 0){
                            error = 0;
                            fprintf(stdout,"%s: tag (%s) parsed successfully with inner_text %s  SUCCESS\n",__PRETTY_FUNCTION__,tag,(char*)mm_lock(node->inner_text));
                            mm_unlock(node->inner_text);
                        } else fprintf(stderr,"%s: tag (%s) mismatch %s. PROCEED\n",__PRETTY_FUNCTION__,tag,lex);

                        mm_unlock(node->tag);
                    } else fprintf(stderr,"%s: no tag was parsered previosly!. (at : %s node: %p)FATAL!\n",__PRETTY_FUNCTION__,html,node);
                    mm_unlock(pop_handle);
                    if(error) return root_obj;
                } else {fprintf(stderr, "%s: too much closings!. FATAL!\n",__PRETTY_FUNCTION__); return root_obj;}
            } else if(*html == '!'){

                while(*html && *html != '>')
                    html++;
                if(*html == '>') html--;
            }else {
                lexi = 0;
                while(*html != '>' && *html){
                    lex[lexi++] = *html;
                    html++;
                }
                lex[lexi] = '\0';

                mm_handle pop_handle = hstack_peek(&node_stack);
                mm_handle unlock_node = pop_handle;
                HTMLNode* node = mm_lock(pop_handle);
                if(node == NULL){
                    mm_handle push_handle = mm_alloc(sizeof(HTMLNode));
                    hstack_push(push_handle,&node_stack);
                    node = mm_lock(push_handle);
                    if(node == NULL){
                        fprintf(stderr, "%s: not enough memory. FATAL!\n",__PRETTY_FUNCTION__);
                        mm_unlock(unlock_node);
                        return root_obj;
                    }
                    memset(node,0,sizeof(*node));
                    handle_vector_init(&node->children);
                    if(memcmp(&root_obj,&(mm_handle){0},sizeof(root_obj)) != 0){
                        fprintf(stderr,"%s: multiple errors acummulated, multiple roots!. FATAL!\n",__PRETTY_FUNCTION__);
                        mm_unlock(unlock_node);
                        handle_vector_destroy(&node->children); //destroy thoose childrens!
                        mm_free(mm_unlock(push_handle));
                    }
                    unlock_node = push_handle;
                    root_obj = push_handle;
                } else {
                    mm_handle new_node = mm_alloc(sizeof(HTMLNode));
                    handle_vector_set(&node->children,node->children_pos++,new_node);

                    node = mm_lock(new_node); assert(node);
                    mm_set_metadata(new_node,MM_HANDLE_SERIALIZE_FUNC,0xc0ffe);
                    mm_unlock(pop_handle);
                    memset(node,0,sizeof(*node));
                    node->parent = pop_handle;
                    handle_vector_init(&node->children);
                    hstack_push(new_node,&node_stack);
                    unlock_node = new_node;
                }
                char* space = strchr(lex,' ');
                if(space){
                    *space = '\0';
                    char* attribute = ++space;

                    node->attributes = mm_alloc(strlen(attribute) + 1);
                    if(strcpy(mm_lock(node->attributes),attribute) == NULL){
                        fprintf(stderr,"%s: not enough memory. FATAL!",__PRETTY_FUNCTION__);
                        mm_unlock(unlock_node);
                        return root_obj;
                    }
                    mm_unlock(node->attributes);
                }
                node->tag = mm_alloc(strlen(lex) + 1);
                char* tag_str = mm_lock(node->tag);
                if(strncpy(tag_str,lex,lexi + 1) == NULL){
                    fprintf(stderr, "%s: not enough memory. FATAL!\n",__PRETTY_FUNCTION__);
                    mm_unlock(unlock_node);
                    return root_obj;
                }
                for(int i = 0; i < sizeof(void_nodes) / sizeof(void_nodes[0]); i++){
                    if(strcmp(tag_str,void_nodes[i]) == 0){
                        fprintf(stderr,"%s: void node %s!. PROCEED\n",__PRETTY_FUNCTION__,tag_str);
                        mm_unlock(hstack_pop(&node_stack));
                    }
                }
                lexi = 0;
                memset(lex,0,sizeof(lex));
                mm_unlock(node->tag);

                html++;
                while(*html != '<' && *html){
                    lex[lexi++] = *(html++);
                }
                if(*html == '<') html--; //dirty fix
                lex[lexi] = '\0';
                node->inner_text = mm_alloc(lexi);
                strncpy(mm_lock(node->inner_text),lex,lexi);
                mm_unlock(node->inner_text);
                mm_unlock(unlock_node);
            }
        } else {
            lex[lexi++] = *html;
        }
    }

    return root_obj;
}

void print_node(HTMLNode* node){
    static int padding = 0;

    char* tag = mm_lock(node->tag);
    printf("tag: %s\n",tag);
    mm_unlock(node->tag);

    char* attributes = mm_lock(node->attributes);
    printf("attributes: %s\n",attributes);
    mm_unlock(node->attributes);

    char* txt = mm_lock(node->inner_text);
    printf("txt: %s\n",txt);
    mm_unlock(node->inner_text);

    for(int i = 0; i < handle_vector_size(&node->children); i++){
        mm_handle child = handle_vector_get(&node->children,i);
        print_node(mm_lock(child));
        mm_unlock(child);
    }
    // handle_vector_destroy(&node->children);
}

int main(int argc, char* argv[]){
    assert(argc == 2);
    FILE* html = fopen(argv[1],"r");
    fseek(html, 0L, SEEK_END);
    int size = ftell(html);
    rewind(html);

    char* html_str = malloc(size); assert(html_str);
    fread(html_str,1,size,html);

    mm_handle unlock = HTMLParse(html_str);
    HTMLNode* node = mm_lock(unlock);
    assert(node);

    handle_vector_get(&node->children,0);
    print_node(node);

    mm_unlock(unlock);
}
