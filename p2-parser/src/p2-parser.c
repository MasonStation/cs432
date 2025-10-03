/**
 * @file p2-parser.c
 * @brief Compiler phase 2: parser
 * Authors: Mason Scofield and Matthew Galbreith
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
ASTNode *parse_lit(TokenQueue *input);
ASTNode *parse_base(TokenQueue *input);

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
/**
 * Parsed variable declaration.
 * @param input Token queue to modify.
 * @return The parsed variable declaration AST Node.
 */
ASTNode *parse_vardecl(TokenQueue *input)
{
    int line = get_next_token_line(input);

    DecafType type = parse_type(input);

    if (type == VOID)
    {
        Error_throw_printf("Line %d: variables must be int or bool.\n");
    }

    char id[MAX_TOKEN_LEN];
    parse_id(input, id);

    bool is_array = false;
    int arr_len = 1;

    if (check_next_token(input, SYM, "["))
    {
        match_and_discard_next_token(input, SYM, "[");
        if (!check_next_token_type(input, DECLIT))
        {
            Error_throw_printf("Line %d: array length must be a decimal literal.\n", line);
        }
        Token *n = TokenQueue_remove(input);
        arr_len = (int)strtol(n->text, NULL, 10);
        Token_free(n);

        if (arr_len <= 0)
        {
            Error_throw_printf("Line %d: array length must be > 0.\n", line);
        }

        match_and_discard_next_token(input, SYM, "]");
        is_array = true;
    }

    match_and_discard_next_token(input, SYM, ";");
    return VarDeclNode_new(id, type, is_array, arr_len, line);
}

/**
 * Parsing a program.
 * @param input Token queue to modify.
 * @return The parsed program AST Node.
 */
ASTNode *parse_program(TokenQueue *input)
{
    NodeList *vars = NodeList_new();
    NodeList *funcs = NodeList_new();

    // Phase 1: global vars (int|bool)*
    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool")))
    {
        NodeList_add(vars, parse_vardecl(input));
    }

    // Phase 2: functions (def)*
    while (!TokenQueue_is_empty(input))
    {
        if (!check_next_token(input, KEY, "def"))
        {
            int line = get_next_token_line(input);
            Error_throw_printf("Unexpected token at top level on line %d (expected 'def' or end of file)\n", line);
        }
        NodeList_add(funcs, parse_funcdecl(input));
    }

    return ProgramNode_new(vars, funcs);
}

/**
 * Parses a function declaration (e.g., def int foo( ) { } ).
 * @param input Token queue to modify.
 * @return The parsed function declaration AST Node.
 */
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
/**
 * Parses a while loop. (e.g., while( ) { } ).
 * @param input Token queue to modify
 * @return The parsed while AST Node.
 */
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
/**
 * Parsed a block of code. (e.g., code inside the "{ }").
 * @param input Token queue to modify.
 * @return The parsed block AST Node.
 */
ASTNode *parse_block(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, SYM, "{");

    NodeList *vars = NodeList_new();
    NodeList *stmts = NodeList_new();

    while (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool")) {
        NodeList_add(vars, parse_vardecl(input));
    }

    // Stop when next token's *text* is "}"
    while (!TokenQueue_is_empty(input)) {
        Token *t = TokenQueue_peek(input);
        if (token_str_eq(t->text, "}")) break;

        ASTNode *stmt = parse_statement(input);
        if (stmt) NodeList_add(stmts, stmt);
    }

    // Consume '}' by text (robust to token type)
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Line %d: expected '}' to close block.\n", line);
    }
    Token *rb = TokenQueue_remove(input);
    if (!token_str_eq(rb->text, "}")) {
        Error_throw_printf("Expected '}' but found '%s' on line %d\n", rb->text, rb->line);
    }
    Token_free(rb);

    return BlockNode_new(vars, stmts, line);
}


/**
 * Parses an assignment statement (e.g., the "=" operator in the x = 5; statement);
 * @param input Token queue to modify.
 * @return The parsed assignment AST Node.
 */
ASTNode *parse_assignment(TokenQueue *input)
{
    int line = get_next_token_line(input);
    ASTNode *location = parse_location(input);
    match_and_discard_next_token(input, SYM, "=");
    ASTNode *expr = parse_expression(input);
    match_and_discard_next_token(input, SYM, ";");
    return AssignmentNode_new(location, expr, line);
}
/**
 * Parses an if statement, and an else if needed. (e.g., if( ) { } else { }).
 * @param input Token queue to modify
 * @return The parsed if AST Node.
 */
ASTNode *parse_if(TokenQueue *input)
{
    if (TokenQueue_is_empty(input)) {
        Error_throw_printf("Unexpected end of input (expected 'if')\n");
    }
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "if");
    match_and_discard_next_token(input, SYM, "(");
    ASTNode *condition = parse_expression(input);
    match_and_discard_next_token(input, SYM, ")");

    // then-part: block or single statement wrapped as a block
    ASTNode *then_block;
    if (check_next_token(input, SYM, "{")) {
        then_block = parse_block(input);
    } else {
        ASTNode *stmt = parse_statement(input);
        NodeList *vars = NodeList_new();
        NodeList *stmts = NodeList_new();
        if (stmt) NodeList_add(stmts, stmt);
        then_block = BlockNode_new(vars, stmts, line);
    }

    ASTNode *else_block = NULL;
    if (!TokenQueue_is_empty(input) && check_next_token(input, KEY, "else")) {
        match_and_discard_next_token(input, KEY, "else");

        if (check_next_token(input, KEY, "if")) {
            // else-if: parse another if, then wrap it in a block for your AST
            ASTNode *nested_if = parse_if(input);
            NodeList *vars = NodeList_new();
            NodeList *stmts = NodeList_new();
            NodeList_add(stmts, nested_if);
            else_block = BlockNode_new(vars, stmts, line);
        } else if (check_next_token(input, SYM, "{")) {
            else_block = parse_block(input);
        } else {
            // single statement else; wrap it in a block
            ASTNode *stmt = parse_statement(input);
            NodeList *vars = NodeList_new();
            NodeList *stmts = NodeList_new();
            if (stmt) NodeList_add(stmts, stmt);
            else_block = BlockNode_new(vars, stmts, line);
        }
    }

    return ConditionalNode_new(condition, then_block, else_block, line);
}

/**
 * Checks and determines which statement needed to be parsed based on the next token.
 * @param input Token queue to modify.
 * @return The parsed statement AST Node.
 */
ASTNode *parse_statement(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected statement)\n");
    }
    // Checks the next token to determine which statement to parse.
    if (check_next_token(input, SYM, "{"))
    {
        return parse_block(input);
    }
    // Checks if the next token is an identifier, if it is, it parses an assignment statement.
    if (check_next_token_type(input, ID))
    {
        // Parse once: could be Location (with optional [expr]) OR a FuncCall.
        ASTNode *lhs_or_call = parse_base(input);

        if (check_next_token(input, SYM, "="))
        {
            int line = get_next_token_line(input);
            match_and_discard_next_token(input, SYM, "=");
            ASTNode *rhs = parse_expression(input);
            match_and_discard_next_token(input, SYM, ";");
            return AssignmentNode_new(lhs_or_call, rhs, line);
        }
        else
        {
            // Expression statement (e.g., a function call like init();)
            match_and_discard_next_token(input, SYM, ";");
            return lhs_or_call; // if your AST needs a CallStmt wrapper, add later in semantics
        }
    }

    // Checks if the next token is an if keyword, if it is, it parses an if statement.
    if (check_next_token(input, KEY, "if"))
    {
        return parse_if(input);
    }
    // Checks if the next token is a return keyword, if it is, it parses a return statement.
    if (check_next_token(input, KEY, "return"))
    {
        return parse_return(input);
    }
    // Checks if the next token is a while keyword, if it is, it parses a while statement.
    if (check_next_token(input, KEY, "while"))
    {
        return parse_while(input);
    }
    // Checks if the next token is a break keyword, if it is, it parses a break statement.
    if (check_next_token(input, KEY, "break"))
    {
        return parse_break(input);
    }
    // Checks if the next token is a continue keyword, if it is, it parses a continue statement.
    if (check_next_token(input, KEY, "continue"))
    {
        return parse_continue(input);
    }
    // Checks if the next token is a semicolon, if it is, it discards it and returns NULL.
    if (check_next_token(input, SYM, ";"))
    {
        match_and_discard_next_token(input, SYM, ";");
        return NULL;
    }

    Error_throw_printf("Unknown statement on line %d\n", get_next_token_line(input));
    return NULL;
}
/**
 * Parses a break statement. (e.g., break;)
 * @param input Token queue to modify
 */
ASTNode *parse_break(TokenQueue *input)
{
    int line = get_next_token_line(input);
    // checks for the break keyword and the semicolon after it.
    match_and_discard_next_token(input, KEY, "break");
    match_and_discard_next_token(input, SYM, ";");
    return BreakNode_new(line);
}
/**
 * Parses a continue statement. (e.g., continue;)
 * @param input Token queue to modify
 * @return The parsed continue AST Node.
 */
ASTNode *parse_continue(TokenQueue *input)
{
    int line = get_next_token_line(input);
    match_and_discard_next_token(input, KEY, "continue");
    match_and_discard_next_token(input, SYM, ";");
    return ContinueNode_new(line);
}
/**
 * Parses a return statement.(e.g., return;)
 * @param input Token queue to modify
 * @return The parsed returned AST Node.
 */
ASTNode *parse_return(TokenQueue *input)
{
    int line = get_next_token_line(input);
    // checks for the return keyword, then checks if there is an expression after it.
    match_and_discard_next_token(input, KEY, "return");
    ASTNode *expr = NULL;
    if (!check_next_token(input, SYM, ";"))
    {
        expr = parse_expression(input);
    }
    match_and_discard_next_token(input, SYM, ";");
    return ReturnNode_new(expr, line);
}
/**
 * Parses a location (e.g., a location of a variable reference).
 * @param input Token queue to modify.
 * @return The parsed location AST Node.
 */
ASTNode *parse_location(TokenQueue *input)
{
    int line = get_next_token_line(input);
    char id[MAX_TOKEN_LEN];
    parse_id(input, id);

    ASTNode *index = NULL;
    // checks if the token is an array bracket, and if it is, parses the expression inside the brackets for the location.
    if (check_next_token(input, SYM, "["))
    {
        match_and_discard_next_token(input, SYM, "[");
        index = parse_expression(input);
        match_and_discard_next_token(input, SYM, "]");
    }
    return LocationNode_new(id, index, line);
}

/**
 * Unescapes a basic string literal (e.g., "hello\nworld").
 * @param src Source string to unescape.
 * @param dst Destination buffer for unescaped string.
 * @param cap Capacity of destination buffer (including null terminator).
 */
static void unescape_basic(const char *src, char *dst, size_t cap)
{
    if (!src || !dst || cap == 0)
        return;
    size_t i = 0, di = 0, len = strlen(src);

    size_t start = 0, end = len;
    if (len >= 2 && src[0] == '"' && src[len - 1] == '"')
    {
        start = 1;
        end = len - 1;
    }

    for (i = start; i < end && di + 1 < cap;)
    {
        char c = src[i++];
        if (c == '\\' && i < end)
        {
            char e = src[i++];
            if (e == 'n')
                c = '\n';
            else if (e == '"')
                c = '"';
            else if (e == '\\')
                c = '\\';
            else
            {
                if (di + 2 < cap)
                {
                    dst[di++] = '\\';
                    dst[di++] = e;
                }
                else if (di + 1 < cap)
                {
                    dst[di++] = '\\';
                }
                continue;
            }
        }
        dst[di++] = c;
    }
    dst[di] = '\0';
}
/**
 * Parses a base expression (e.g., literals, parenthesized expressions, variable references, function calls).
 * @param input Token queue to modify.
 * @return The parsed base AST Node.
 */
ASTNode *parse_base(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
        Error_throw_printf("Unexpected end of input (expected expression)\n");

    int line = get_next_token_line(input);

    if (check_next_token(input, SYM, "("))
    {
        match_and_discard_next_token(input, SYM, "(");
        ASTNode *e = parse_expression(input);
        match_and_discard_next_token(input, SYM, ")");
        return e;
    }

    // Identifier: could be FuncCall or Loc (with optional index)
    if (check_next_token_type(input, ID))
    {
        Token *idTok = TokenQueue_remove(input);
        char name[MAX_TOKEN_LEN];
        snprintf(name, sizeof(name), "%s", idTok->text);
        line = idTok->line;

        if (check_next_token(input, SYM, "("))
        {
            match_and_discard_next_token(input, SYM, "(");
            NodeList *args = NodeList_new();
            if (!check_next_token(input, SYM, ")"))
            {
                while (1)
                {
                    NodeList_add(args, parse_expression(input));
                    if (!check_next_token(input, SYM, ","))
                        break;
                    match_and_discard_next_token(input, SYM, ",");
                }
            }
            match_and_discard_next_token(input, SYM, ")");
            Token_free(idTok);
            return FuncCallNode_new(name, args, line);
        }

        ASTNode *index = NULL;
        if (check_next_token(input, SYM, "["))
        {
            match_and_discard_next_token(input, SYM, "[");
            index = parse_expression(input);
            match_and_discard_next_token(input, SYM, "]");
        }
        Token_free(idTok);
        return LocationNode_new(name, index, line);
    }

    if (check_next_token(input, KEY, "true"))
    {
        Token *t = TokenQueue_remove(input);
        ASTNode *n = LiteralNode_new_bool(true, line);
        Token_free(t);
        return n;
    }
    if (check_next_token(input, KEY, "false"))
    {
        Token *t = TokenQueue_remove(input);
        ASTNode *n = LiteralNode_new_bool(false, line);
        Token_free(t);
        return n;
    }
    if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT))
    {
        Token *t = TokenQueue_remove(input);
        int v = (int)strtol(t->text, NULL, 0);
        ASTNode *n = LiteralNode_new_int(v, line);
        Token_free(t);
        return n;
    }
    if (check_next_token_type(input, STRLIT))
    {
        Token *t = TokenQueue_remove(input);
        int line = t->line;
        char buf[MAX_TOKEN_LEN];
        unescape_basic(t->text, buf, sizeof(buf));
        ASTNode *n = LiteralNode_new_string(buf, line);
        Token_free(t);
        return n;
    }

    Error_throw_printf("Expected expression on line %d\n", line);
    return NULL;
}

/**
 * Parses a literal (e.g., true, false, 123, "hello").
 * @param input Token queue to modify.
 * @return The parsed literal AST Node.
 */
ASTNode *parse_lit(TokenQueue *input)
{
    if (TokenQueue_is_empty(input))
    {
        Error_throw_printf("Unexpected end of input (expected expression)\n");
    }
    int line = get_next_token_line(input);

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
    if (check_next_token_type(input, DECLIT) || check_next_token_type(input, HEXLIT))
    {
        Token *token = TokenQueue_remove(input);
        int value = (int)strtol(token->text, NULL, 0);
        ASTNode *node = LiteralNode_new_int(value, line);
        Token_free(token);
        return node;
    }

    if (check_next_token_type(input, STRLIT))
    {
        Token *token = TokenQueue_remove(input);
        int line2 = token->line;
        char buf[MAX_TOKEN_LEN];
        unescape_basic(token->text, buf, sizeof(buf));
        ASTNode *node = LiteralNode_new_string(buf, line2);
        Token_free(token);
        return node;
    }

    Error_throw_printf("Expected expression on line %d\n", line);
    return NULL;
}

/**
 * Parses a unary expression (e.g., "+" or "-").
 * @param input Token queue to modify.
 * @return The parsed unary AST Node.
 */
ASTNode *parse_unary(TokenQueue *input)
{
    if (check_next_token(input, SYM, "-"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "-");
        ASTNode *expr = parse_unary(input);
        return UnaryOpNode_new(NEGOP, expr, line);
    }
    if (check_next_token(input, SYM, "!"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "!");
        return UnaryOpNode_new(NOTOP, parse_unary(input), line);
    }
    return parse_base(input);
}
/**
 *Parses the multiplication, division, and modulus token operators.
 * @param input Token queue to modify.
 * @return The parsed binary expression AST Node.
 */
ASTNode *parse_binexpr(TokenQueue *input)
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

/**
 * Parses binary expressions
 * @param input Token queue to modify.
 * @return The parsed binary expression AST Node.
 */
ASTNode *parse_expression(TokenQueue *input)
{
    // multiplicative: *, /, %
    ASTNode *left = parse_binexpr(input);

    // additive: +, -
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
        ASTNode *right = parse_binexpr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }

    // relational: <, <=, >, >=
    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, SYM, "<=") || check_next_token(input, SYM, ">=") ||
            check_next_token(input, SYM, "<") || check_next_token(input, SYM, ">")))
    {
        int line = get_next_token_line(input);
        BinaryOpType op;
        if (check_next_token(input, SYM, "<="))
        {
            op = LEOP;
            match_and_discard_next_token(input, SYM, "<=");
        }
        else if (check_next_token(input, SYM, ">="))
        {
            op = GEOP;
            match_and_discard_next_token(input, SYM, ">=");
        }
        else if (check_next_token(input, SYM, "<"))
        {
            op = LTOP;
            match_and_discard_next_token(input, SYM, "<");
        }
        else
        {
            op = GTOP;
            match_and_discard_next_token(input, SYM, ">");
        }
        ASTNode *right = parse_binexpr(input);
        // allow chained rels only as left-assoc parses; spec usually rejects semantically later
        left = BinaryOpNode_new(op, left, right, line);
    }

    // equality: ==, !=
    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, SYM, "==") || check_next_token(input, SYM, "!=")))
    {
        int line = get_next_token_line(input);
        BinaryOpType op;
        if (check_next_token(input, SYM, "=="))
        {
            op = EQOP;
            match_and_discard_next_token(input, SYM, "==");
        }
        else
        {
            op = NEQOP;
            match_and_discard_next_token(input, SYM, "!=");
        }
        ASTNode *right = parse_binexpr(input);
        left = BinaryOpNode_new(op, left, right, line);
    }

    // logical AND: &&
    while (!TokenQueue_is_empty(input) && check_next_token(input, SYM, "&&"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "&&");
        ASTNode *right = parse_binexpr(input);
        left = BinaryOpNode_new(ANDOP, left, right, line);
    }

    // logical OR: ||
    while (!TokenQueue_is_empty(input) && check_next_token(input, SYM, "||"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "||");
        ASTNode *right = parse_binexpr(input);
        left = BinaryOpNode_new(OROP /* or whatever your enum uses */, left, right, line);
    }

    return left;
}

/**
 * The whole program.
 * @param input Token queue to modify.
 * @return The parsed program AST Node.
 */
ASTNode *parse(TokenQueue *input)
{
    // Checks if the input TokenQueue is null, if it is, throws an error.
    if (input == NULL)
    {
        Error_throw_printf("No input provided to parser.\n");
    }
    return parse_program(input);
}
