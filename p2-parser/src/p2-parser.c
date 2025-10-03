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

    // VarDecl*
    while (check_next_token(input, KEY, "int") || check_next_token(input, KEY, "bool"))
    {
        NodeList_add(vars, parse_vardecl(input));
    }

    // If immediately '}', empty block
    while (!check_next_token(input, SYM, "}"))
    {
        ASTNode *stmt = parse_statement(input);
        if (stmt != NULL) // ignore empty ';' statements
            NodeList_add(stmts, stmt);
    }

    match_and_discard_next_token(input, SYM, "}");
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
    if (check_next_token(input, SYM, "{"))
    {
        return parse_block(input); // delegate here
    }
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
/**
 * Parses a break statement. (e.g., break;)
 * @param input Token queue to modify
 */
ASTNode *parse_break(TokenQueue *input)
{
    int line = get_next_token_line(input);
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
    if (check_next_token(input, SYM, "["))
    {
        match_and_discard_next_token(input, SYM, "[");
        index = parse_expression(input);
        match_and_discard_next_token(input, SYM, "]");
    }
    return LocationNode_new(id, index, line);
}

/**
 * Parses a primary expression (e.g., literals, parenthesized expressions).
 * @param input Token queue to modify.
 * @return The parsed primary AST Node.
 */
static void unescape_basic(const char *src, char *dst, size_t cap)
{
    if (!src || !dst || cap == 0)
        return;
    size_t i = 0, di = 0, len = strlen(src);

    // strip surrounding quotes if present
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
            { // unknown escape: keep as-is literally
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
// ...existing code...

// ...existing code...

ASTNode *parse_expression(TokenQueue *input)
{
    // multiplicative handled in parse_binexpr; build additive layer here
    ASTNode *left = parse_binexpr(input);

    while (!TokenQueue_is_empty(input) &&
           (check_next_token(input, SYM, "+") ||
            check_next_token(input, SYM, "-")))
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

    // logical AND (lower precedence than + -)
    while (!TokenQueue_is_empty(input) && check_next_token(input, SYM, "&&"))
    {
        int line = get_next_token_line(input);
        match_and_discard_next_token(input, SYM, "&&");

        // parse right side (additive level again)
        ASTNode *right = parse_binexpr(input);
        while (!TokenQueue_is_empty(input) &&
               (check_next_token(input, SYM, "+") ||
                check_next_token(input, SYM, "-")))
        {
            int line2 = get_next_token_line(input);
            BinaryOpType op2;
            if (check_next_token(input, SYM, "+"))
            {
                op2 = ADDOP;
                match_and_discard_next_token(input, SYM, "+");
            }
            else
            {
                op2 = SUBOP;
                match_and_discard_next_token(input, SYM, "-");
            }
            ASTNode *r2 = parse_binexpr(input);
            right = BinaryOpNode_new(op2, right, r2, line2);
        }

        left = BinaryOpNode_new(ANDOP, left, right, line);
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
    if (input == NULL)
    {
        Error_throw_printf("No input provided to parser.\n");
    }
    return parse_program(input);
}
