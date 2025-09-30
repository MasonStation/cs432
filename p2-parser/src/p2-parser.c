/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 */

#include "p2-parser.h"
#include "string.h"
ASTNode *parse_if(TokenQueue *input);
ASTNode *parse_block(TokenQueue *input);
ASTNode *parse_statement(TokenQueue *input);
ASTNode *parse_break(TokenQueue *input);
ASTNode *parse_continue(TokenQueue *input);
ASTNode *parse_return(TokenQueue *input);
ASTNode *parse_assignment(TokenQueue *input);
ASTNode *parse_location(TokenQueue *input);
ASTNode *parse_expression(TokenQueue *input);
ASTNode *parse_funcdecl(TokenQueue *input);
/*
 * helper functions
 */

/**
 * @brief Look up the source line of the next token in the queue.
 *
 * @param input Token queue to examine
 * @returns Source line
 */
int get_next_token_line(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input\n");
    }
    return TokenQueue_peek(input)->line;
}

/**
 * @brief Check next token for a particular type and text and discard it
 *
 * Throws an error if there are no more tokens or if the next token in the
 * queue does not match the given type or text.
 *
 * @param input Token queue to modify
 * @param type Expected type of next token
 * @param text Expected text of next token
 */
void match_and_discard_next_token(TokenQueue *input, TokenType type, const char *text)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected \'%s\')\n", text);
    }
    Token *token = TokenQueue_remove(input);
    if (token->type != type || !token_str_eq(token->text, text))
    {
        Error_throw_printf("Expected \'%s\' but found '%s' on line %d\n",
                           text, token->text, get_next_token_line(input));
    }
    Token_free(token);
}

/**
 * @brief Remove next token from the queue
 *
 * Throws an error if there are no more tokens.
 *
 * @param input Token queue to modify
 */
void discard_next_token(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input\n");
    }
    Token_free(TokenQueue_remove(input));
}

/**
 * @brief Look ahead at the type of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @returns True if the next token is of the expected type, false if not
 */
bool check_next_token_type(TokenQueue *input, TokenType type)
{
    if (TokenQueue_is_empty(input))
    {
        return false;
    }
    Token *token = TokenQueue_peek(input);
    return (token->type == type);
}

/**
 * @brief Look ahead at the type and text of the next token
 *
 * @param input Token queue to examine
 * @param type Expected type of next token
 * @param text Expected text of next token
 * @returns True if the next token is of the expected type and text, false if not
 */
bool check_next_token(TokenQueue *input, TokenType type, const char *text)
{
    if (TokenQueue_is_empty(input))
    {
        return false;
    }
    Token *token = TokenQueue_peek(input);
    return (token->type == type) && (token_str_eq(token->text, text));
}

/**
 * @brief Parse and return a Decaf type
 *
 * @param input Token queue to modify
 * @returns Parsed type (it is also removed from the queue)
 */
DecafType parse_type(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected type)\n");
    }
    Token *token = TokenQueue_remove(input);
    if (token->type != KEY)
    {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    DecafType t = VOID;
    if (token_str_eq("int", token->text))
    {
        t = INT;
    }
    else if (token_str_eq("bool", token->text))
    {
        t = BOOL;
    }
    else if (token_str_eq("void", token->text))
    {
        t = VOID;
    }
    else
    {
        Error_throw_printf("Invalid type '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    Token_free(token);
    return t;
}

/**
 * @brief Parse and return a Decaf identifier
 *
 * @param input Token queue to modify
 * @param buffer String buffer for parsed identifier (should be at least
 * @c MAX_TOKEN_LEN characters long)
 */
void parse_id(TokenQueue *input, char *buffer)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected identifier)\n");
    }
    Token *token = TokenQueue_remove(input);
    if (token->type != ID)
    {
        Error_throw_printf("Invalid ID '%s' on line %d\n", token->text, get_next_token_line(input));
    }
    snprintf(buffer, MAX_ID_LEN, "%s", token->text);
    Token_free(token);
}

ASTNode *parse_vardecl(TokenQueue *input)
{
    DecafType type = parse_type(input);
    char id[MAX_TOKEN_LEN];
    parse_id(input, id);
    match_and_discard_next_token(input, SYM, ";");
    // printf("type: %s\n", DecafType_to_string(type));
    return VarDeclNode_new(id, type, false, 1, 1);
}

/*
 * node-level parsing functions
 */
// booleans are literals
// void is a valid decaf type
ASTNode *parse_program(TokenQueue *input)
{
    NodeList *vars = NodeList_new();
    NodeList *funcs = NodeList_new();
    while (!TokenQueue_is_empty(input))
    {
        if (check_next_token(input, KEY, "def"))
        {
            NodeList_add(funcs, parse_funcdecl(input));
        }
        else
        {
            NodeList_add(vars, parse_vardecl(input));
        }
    }
    return ProgramNode_new(vars, funcs);
}

ASTNode *parse_funcdecl(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "def");
    DecafType ret_type = parse_type(input);
    char func[MAX_TOKEN_LEN];
    parse_id(input, func);
    match_and_discard_next_token(input, SYM, "(");

    // Parse parameter list
    ParameterList *params = ParameterList_new();
    if (!check_next_token(input, SYM, ")"))
    {
        while (1)
        {
            DecafType param_type = parse_type(input);
            char param_name[MAX_TOKEN_LEN];
            parse_id(input, param_name);
            ParameterList_add_new(params, param_name, param_type);

            if (check_next_token(input, SYM, ","))
            {
                match_and_discard_next_token(input, SYM, ",");
            }
            else
            {
                break;
            }
        }
    }

    match_and_discard_next_token(input, SYM, ")");
    ASTNode *body = parse_block(input);
    return FuncDeclNode_new(func, ret_type, params, body, line);
}

ASTNode *parse_while(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "while");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *condition = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode *body = parse_block(input);
    return WhileLoopNode_new(condition, body, line);
}
ASTNode *parse_block(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, SYM, "{");
    NodeList *vars = NodeList_new();
    NodeList *stmts = NodeList_new();
    while (!check_next_token(input, SYM, "}"))
    {
        if (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool"))
        {
            NodeList_add(vars, parse_vardecl(input));
        }
        else
        {
            NodeList_add(stmts, parse_statement(input));
        }
    }
    match_and_discard_next_token(input, SYM, "}");
    return BlockNode_new(vars, stmts, line);
}

ASTNode *parse_assignment(TokenQueue *input)
{
    int line = get_next_token_line(input);
    ASTNode *location = parse_location(input);
    match_and_discard_next_token(input, SYM, "=");
    ASTNode *expr = parse_expression(input);
    match_and_discard_next_token(input, SYM, ";");
    return AssignmentNode_new(location, expr, line);
}

ASTNode *parse_if(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected 'if')\n");
    }
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "if");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *condition = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");
    ASTNode *then = parse_block(input);

    ASTNode *else_block = NULL;

    if (!TokenQueue_is_empty(input) && check_next_token(input, KEY, "else"))
    {
        match_and_discard_next_token(input, KEY, "else");
        else_block = parse_block(input);
    }
    return ConditionalNode_new(condition, then, else_block, line);
}
// ASTNode* parse_primary(TokenQueue* input){
//     if(TokenQueue_is_empty(input)){
//         Error_throw_printf("Unexpected end of input (expected primary expression)\n");
//     }
//     int line = get_next_token_line(input);
//     if()
// }
ASTNode *parse_statement(TokenQueue *input)
{
    if (check_next_token_type(input, ID))
    {
        return parse_assignment(input);
    }
    if (check_next_token(input, KEY, "if"))
    {
        return parse_if(input);
    }
    if (check_next_token(input, KEY, "return"))
    {
        return parse_return(input);
    }
    if (check_next_token(input, KEY, "while"))
    {
        return parse_while(input);
    }
    if (check_next_token(input, KEY, "break"))
    {
        return parse_break(input);
    }
    if (check_next_token(input, KEY, "continue"))
    {
        return parse_continue(input);
    }
    if (check_next_token(input, SYM, ";"))
    {
        match_and_discard_next_token(input, SYM, ";");
        return NULL;
    }
    Error_throw_printf("Unknown statement on line %d\n", get_next_token_line(input));
    return NULL;
}
ASTNode *parse_break(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "break");
    match_and_discard_next_token(input, SYM, ";");
    return BreakNode_new(line);
}
ASTNode *parse_continue(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "continue");
    match_and_discard_next_token(input, SYM, ";");
    return ContinueNode_new(line);
}
ASTNode *parse_return(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "return");
    ASTNode *expr = NULL;
    if (!check_next_token(input, SYM, ";"))
    {
        expr = parse_expression(input);
    }
    match_and_discard_next_token(input, SYM, ";");
    return ReturnNode_new(expr, line);
}
ASTNode *parse_location(TokenQueue *input)
{
    int line = get_next_token_line(input);
    char id[MAX_TOKEN_LEN];
    parse_id(input, id);
    return LocationNode_new(id, NULL, line);
}

ASTNode *parse_primary(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected expression)\n");
    }
    int line = get_next_token_line(input);

    // Boolean literals
    if (check_next_token(input, KEY, "true"))
    {
        Token *token = TokenQueue_remove(input);
        ASTNode *node = LiteralNode_new_bool(true, line);
        Token_free(token);
        return node;
    }
    if (check_next_token(input, KEY, "false"))
    {
        Token *token = TokenQueue_remove(input);
        ASTNode *node = LiteralNode_new_bool(false, line);
        Token_free(token);
        return node;
    }
    // Parenthesized expressions
    if (check_next_token(input, SYM, "("))
    {
        match_and_discard_next_token(input, SYM, "(");
        ASTNode *expr = parse_expression(input);
        match_and_discard_next_token(input, SYM, ")");
        return expr;
    }
    // Integer literals (decimal/hex)
    if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT))
    {
        Token *token = TokenQueue_remove(input);
        int value = (int)strtol(token->text, NULL, 0);
        ASTNode *node = LiteralNode_new_int(value, line);
        Token_free(token);
        return node;
    }
    // String literals
    if (check_next_token_type(input, STRLIT))
    {
        Token *token = TokenQueue_remove(input);
        char buf[MAX_TOKEN_LEN];
        size_t len = strlen(token->text);
        if (len >= 2 && token->text[0] == '\"' && token->text[len - 1] == '\"')
        {
            size_t inner_len = len - 2;
            if (inner_len >= MAX_TOKEN_LEN)
                inner_len = MAX_TOKEN_LEN - 1;
            memcpy(buf, token->text + 1, inner_len);
            buf[inner_len] = '\0';
        }
        else
        {
            snprintf(buf, MAX_TOKEN_LEN, "%s", token->text);
        }
        ASTNode *node = LiteralNode_new_string(buf, line);
        Token_free(token);
        return node;
    }
    // Identifiers (variable references)
    if (check_next_token_type(input, ID))
    {
        Token *token = TokenQueue_remove(input);
        ASTNode *node = LocationNode_new(token->text, NULL, line);
        Token_free(token);
        return node;
    }

    Error_throw_printf("Expected expression on line %d\n", line);
    return NULL;
}

ASTNode *parse_unary(TokenQueue *input)
{
    if (check_next_token(input, SYM, "-"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "-");
        ASTNode *expr = parse_unary(input);
        return UnaryOpNode_new(NEGOP, expr, line);
    }
    return parse_primary(input);
}

ASTNode *parse_multiply(TokenQueue *input)
{
    ASTNode *left = parse_unary(input);
    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, SYM, "*") || check_next_token(input, SYM, "/") || check_next_token(input, SYM, "%")))
    {
        int line = get_next_token_line(input);
        BinaryOpType op;
        if (check_next_token(input, SYM, "*"))
        {
            op = MULOP;
            match_and_discard_next_token(input, SYM, "*");
        }
        else if (check_next_token(input, SYM, "/"))
        {
            op = DIVOP;
            match_and_discard_next_token(input, SYM, "/");
        }
        else
        {
            op = MODOP;
            match_and_discard_next_token(input, SYM, "%");
        }
        ASTNode *right = parse_unary(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    return left;
}

ASTNode *parse_add(TokenQueue *input)
{
    ASTNode *left = parse_multiply(input);
    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, SYM, "+") || check_next_token(input, SYM, "-")))
    {
        int line = get_next_token_line(input);
        BinaryOpType op;
        if (check_next_token(input, SYM, "+"))
        {
            op = ADDOP;
            match_and_discard_next_token(input, SYM, "+");
        }
        else
        {
            op = SUBOP;
            match_and_discard_next_token(input, SYM, "-");
        }
        ASTNode *right = parse_multiply(input);
        left = BinaryOpNode_new(op, left, right, line);
    }
    return left;
}

ASTNode *parse_expression(TokenQueue *input)
{
    return parse_add(input);
}

ASTNode *parse(TokenQueue *input)
{
    return parse_program(input);
}
