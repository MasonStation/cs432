/**
 * @file p3-analysis.c
 * @brief Compiler phase 3: static analysis
 */
#include "p3-analysis.h"

/**
 * @brief State/data for static analysis visitor
 */
typedef struct AnalysisData
{
    /**
     * @brief List of errors detected
     */
    ErrorList* errors;

    /* BOILERPLATE: TODO: add any new desired state information (and clean it up in AnalysisData_free) */

} AnalysisData;

/**
 * @brief Allocate memory for analysis data
 * 
 * @returns Pointer to allocated structure
 */
AnalysisData* AnalysisData_new (void)
{
    AnalysisData* data = (AnalysisData*)calloc(1, sizeof(AnalysisData));
    CHECK_MALLOC_PTR(data);
    data->errors = ErrorList_new();
    return data;
}

/**
 * @brief Deallocate memory for analysis data
 * 
 * @param data Pointer to the structure to be deallocated
 */
void AnalysisData_free (AnalysisData* data)
{
    /* free everything in data that is allocated on the heap except the error
     * list; it needs to be returned after the analysis is complete */

    /* free "data" itself */
    free(data);
}

/**
 * @brief Macro for more convenient access to the data inside a @ref AnalysisVisitor
 * data structure
 */
#define DATA ((AnalysisData*)visitor->data)

/**
 * @brief Macro for more convenient access to the error list inside a
 * @ref AnalysisVisitor data structure
 */
#define ERROR_LIST (((AnalysisData*)visitor->data)->errors)

/**
 * @brief Wrapper for @ref lookup_symbol that reports an error if the symbol isn't found
 * 
 * @param visitor Visitor with the error list for reporting
 * @param node AST node to begin the search at
 * @param name Name of symbol to find
 * @returns The @ref Symbol if found, otherwise @c NULL
 */
Symbol* lookup_symbol_with_reporting(NodeVisitor* visitor, ASTNode* node, const char* name)
{
    Symbol* symbol = lookup_symbol(node, name);
    if (symbol == NULL) {
        ErrorList_printf(ERROR_LIST, "Symbol '%s' undefined on line %d", name, node->source_line);
    }
    return symbol;
}

/**
 * @brief Macro for shorter storing of the inferred @c type attribute
 */
#define SET_INFERRED_TYPE(T) ASTNode_set_printable_attribute(node, "type", (void*)(T), \
                                 type_attr_print, dummy_free)

/**
 * @brief Macro for shorter retrieval of the inferred @c type attribute
 */
#define GET_INFERRED_TYPE(N) (DecafType)(long)ASTNode_get_attribute(N, "type")

static void AnalysisVisitor_check_program_main(NodeVisitor* visitor, ASTNode* node)
{
    int main_count = 0;
    ASTNode* main_func = NULL;

    FOR_EACH(ASTNode*, func, node->program.functions) {
        if (strncmp(func->funcdecl.name, "main", MAX_ID_LEN) == 0) {
            main_count++;
            main_func = func;
        }
    }

    if (main_count != 1) {
        ErrorList_printf(ERROR_LIST,
            "Program must contain exactly one 'main' function (found %d).",
            main_count);
        return;
    }

    if (main_func->funcdecl.return_type != INT) {
        ErrorList_printf(ERROR_LIST,
            "Invalid main: return type must be int (found %s) on line %d",
            DecafType_to_string(main_func->funcdecl.return_type),
            main_func->source_line);
    }

    if (ParameterList_size(main_func->funcdecl.parameters) != 0) {
        ErrorList_printf(ERROR_LIST,
            "Invalid main: must not take parameters (found %d) on line %d",
            ParameterList_size(main_func->funcdecl.parameters),
            main_func->source_line);
    }
}

static void AnalysisVisitor_check_vardecl(NodeVisitor* visitor, ASTNode* node)
{
    //VOID check

    if (node->vardecl.type == VOID) {
        ErrorList_printf(ERROR_LIST,
            "Invalid declaration: variable '%s' declared with type void on line %d",
            node->vardecl.name, node->source_line);
    }
    
    // 
}


static void AnalysisVisitor_check_location(NodeVisitor* visitor, ASTNode* node)
{
    lookup_symbol_with_reporting(visitor, node, node->location.name);
}

ErrorList* analyze (ASTNode* tree)
{
    /* allocate analysis structures */
    NodeVisitor* v = NodeVisitor_new();
    v->data = (void*)AnalysisData_new();
    v->dtor = (Destructor)AnalysisData_free;
    v->previsit_vardecl   = AnalysisVisitor_check_vardecl;
    v->postvisit_location = AnalysisVisitor_check_location;
    v->postvisit_program  = AnalysisVisitor_check_program_main;

    /* BOILERPLATE: TODO: register analysis callbacks */

    /* perform analysis, save error list, clean up, and return errors */
    NodeVisitor_traverse(v, tree);
    ErrorList* errors = ((AnalysisData*)v->data)->errors;
    NodeVisitor_free(v);
    return errors;
}
