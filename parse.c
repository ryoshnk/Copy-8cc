#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "8cc.h"

#define MAX_ARGS 6

Ast *globals = NULL;
Ast *locals = NULL;
Ctype *ctype_int = &(Ctype){ CTYPE_INT, NULL };
Ctype *ctype_char = &(Ctype){ CTYPE_CHAR, NULL };

static int labelseq = 0;

static Ast *read_expr(int prec);
static Ctype* make_ptr_type(Ctype *ctype);
static Ctype* make_array_type(Ctype *ctype, int size);

static Ast *ast_uop(char type, Ctype *ctype, Ast *operand) {
  Ast *r = malloc(sizeof(Ast));
  r->type = type;
  r->ctype = ctype;
  r->operand = operand;
  return r;
}

static Ast *ast_binop(char type, Ctype *ctype, Ast *left, Ast *right) {
  Ast *r = malloc(sizeof(Ast));
  r->type = type;
  r->ctype = ctype;
  r->left = left;
  r->right = right;
  return r;
}

static Ast *ast_int(int val) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_LITERAL;
  r->ctype = ctype_int;
  r->ival = val;
  return r;
}

static Ast *ast_char(char c) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_LITERAL;
  r->ctype = ctype_char;
  r->c = c;
  return r;
}

char *make_next_label(void) {
  String *s = make_string();
  string_appendf(s, ".L%d", labelseq++);
  return get_cstring(s);
}

static Ast *ast_lvar(Ctype *ctype, char *name) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_LVAR;
  r->ctype = ctype;
  r->lname = name;
  r->next = NULL;
  if (locals) {
    Ast *p;
    for (p = locals; p->next; p = p->next);
    p->next = r;
  } else {
    locals = r;
  }
  return r;
}

static Ast *ast_lref(Ctype *ctype, Ast *lvar, int off) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_LREF;
  r->ctype = ctype;
  r->lref = lvar;
  r->lrefoff = off;
  return r;
}

static Ast *ast_gvar(Ctype *ctype, char *name, bool filelocal) __attribute__((unused));
static Ast *ast_gvar(Ctype *ctype, char *name, bool filelocal) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_GVAR;
  r->ctype = ctype;
  r->gname = name;
  r->glabel = filelocal ? make_next_label() : name;
  r->next = NULL;
  if (globals) {
    Ast *p;
    for (p = locals; p->next; p = p->next);
    p->next = r;
  } else {
    globals = r;
  }
  return r;
}

static Ast *ast_gref(Ctype *ctype, Ast *gvar, int off) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_GREF;
  r->ctype = ctype;
  r->gref = gvar;
  r->goff = off;
  return r;
}

static Ast *ast_string(char *str) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_STRING;
  r->ctype = make_array_type(ctype_char, strlen(str) + 1);
  r->sval = str;
  r->slabel = make_next_label();
  r->next = globals;
  globals = r;
  return r;
}

static Ast *ast_funcall(char *fname, int nargs, Ast **args) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_FUNCALL;
  r->ctype = ctype_int;
  r->fname = fname;
  r->nargs = nargs;
  r->args = args;
  return r;
}

static Ast *ast_decl(Ast *var, Ast *init) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_DECL;
  r->ctype = NULL;
  r->declvar = var;
  r->declinit = init;
  return r;
}

static Ast *ast_array_init(int size, Ast **array_init) {
  Ast *r = malloc(sizeof(Ast));
  r->type = AST_ARRAY_INIT;
  r->ctype = NULL;
  r->size = size;
  r->array_init = array_init;
  return r;
}

static Ctype* make_ptr_type(Ctype *ctype) {
  Ctype *r = malloc(sizeof(Ctype));
  r->type = CTYPE_PTR;
  r->ptr = ctype;
  return r;
}

static Ctype* make_array_type(Ctype *ctype, int size) {
  Ctype *r = malloc(sizeof(Ctype));
  r->type = CTYPE_ARRAY;
  r->ptr = ctype;
  r->size = size;
  return r;
}

static Ast *find_var(char *name) {
  for (Ast *p = locals; p; p = p->next)
    if (!strcmp(name, p->lname))
      return p;
  for (Ast *p = globals; p; p = p->next)
    if (!strcmp(name, p->gname))
      return p;
  return NULL;
}

static bool is_right_assoc(char op) {
  return op == '=';
}

static int priority(char op) {
  switch (op) {
    case '=':
      return 1;
    case '+': case '-':
      return 2;
    case '*': case '/':
      return 3;
    default:
      return -1;
  }
}

static Ast *read_func_args(char *fname) {
  Ast **args = malloc(sizeof(Ast*) * (MAX_ARGS + 1));
  int i = 0, nargs = 0;
  args[0] = NULL; //added to pass test:a()
  for (; i < MAX_ARGS; i++) {
    Token *tok = read_token();
    if (is_punct(tok, ')')) break;
    unget_token(tok);
    args[i] = read_expr(0);
    nargs++;
    tok = read_token();
    if (is_punct(tok, ')')) break;
    if (!is_punct(tok, ','))
      error("Unexpected token: '%s'", token_to_string(tok));
  }
  if (i == MAX_ARGS)
    error("Too many arguments: %s", fname);
  return ast_funcall(fname, nargs, args);
}

static Ast *read_ident_or_func(char *name) {
  Token *tok = read_token();
  if (is_punct(tok, '('))
    return read_func_args(name);
  unget_token(tok);
  Ast *v = find_var(name);
  if (!v)
    error("Undefined variable: %s", name);
  return v;
}

static Ast *read_prim(void) {
  Token *tok = read_token();
  if (!tok) return NULL;
  switch (tok->type) {
    case TTYPE_IDENT:
      return read_ident_or_func(tok->sval);
    case TTYPE_INT:
      return ast_int(tok->ival);
    case TTYPE_CHAR:
      return ast_char(tok->c);
    case TTYPE_STRING:
      return ast_string(tok->sval);
    case TTYPE_PUNCT:
      error("unexpected character: '%c'", tok->punct);
    default:
      error("internal error: unknown token type: %d", tok->type);
  }
}

#define swap(a, b)  \
  { typeof(a) tmp = b; b = a; a = tmp; }

static Ctype *result_type_int(jmp_buf *jmpbuf, char op, Ctype *a, Ctype *b) {
  if (a->type > b->type)
    swap(a, b);
  if (b->type == CTYPE_PTR) {
    if (op != '+' && op != '-')
      goto err;
    if (a->type != CTYPE_INT)
      goto err;
    return b;
  }
  switch (a->type) {
    case CTYPE_VOID:
      goto err;
    case CTYPE_INT:
    case CTYPE_CHAR:
      switch (b->type) {
        case CTYPE_INT:
        case CTYPE_CHAR:
          return ctype_int;
        case CTYPE_ARRAY:
        case CTYPE_PTR:
          return b;
      }
      error("internal error");
    case CTYPE_ARRAY:
      goto err;
    default:
      error("internal error");
  }
err:
  longjmp(*jmpbuf, 1);
}

static Ctype *result_type(char op, Ctype *a, Ctype *b) {
  jmp_buf jmpbuf;
  if (setjmp(jmpbuf) == 0)
    return result_type_int(&jmpbuf, op, a, b);
  error("incompatible operands: %c: <%s> and <%s>",
        op, ctype_to_string(a), ctype_to_string(b));
}

static void ensure_lvalue(Ast *ast) {
  switch (ast->type) {
    case AST_LVAR: case AST_LREF:
    case AST_GVAR: case AST_GREF:
      return;
    default:
      error("lvalue expected, but got %s", ast_to_string(ast));
  }
}

static Ast *read_unary_expr(void) {
  Token *tok = read_token();
  if (is_punct(tok, '&')) {
    Ast *operand = read_unary_expr();
    ensure_lvalue(operand);
    return ast_uop(AST_ADDR, make_ptr_type(operand->ctype), operand);
  }
  if (is_punct(tok, '*')) {
    Ast *operand = read_unary_expr();
    if (operand->ctype->type != CTYPE_PTR)
      error("pointer type expected, but got %s", ast_to_string(operand));
    return ast_uop(AST_DEREF, operand->ctype->ptr, operand);
  }
  unget_token(tok);
  return read_prim();
}

static Ast *convert_array(Ast *ast) {
  if (ast->type == AST_STRING)
    return ast_gref(make_ptr_type(ctype_char), ast, 0);
  if (ast->ctype->type != CTYPE_ARRAY)
    return ast;
  if (ast->type == AST_LVAR)
    return ast_lref(make_ptr_type(ast->ctype->ptr), ast, 0);
  if (ast->type != AST_GVAR)
    error("Internal error: Gvar expected, but got %s", ast_to_string(ast));
  return ast_gref(make_ptr_type(ast->ctype->ptr), ast, 0);
}

static Ast *read_expr(int prec) {
  Ast *ast = read_unary_expr();
  if (!ast) return NULL;
  for (;;) {
    Token *tok = read_token();
    if (tok->type != TTYPE_PUNCT) {
      unget_token(tok);
      return ast;
    }
    int prec2 = priority(tok->punct);
    if (prec2 < 0 || prec2 < prec) {
      unget_token(tok);
      return ast;
    }
    if (is_punct(tok, '='))
      ensure_lvalue(ast);
    else
      ast = convert_array(ast);
    Ast *rest = read_expr(prec2 + (is_right_assoc(tok->punct) ? 0 : 1));
    rest = convert_array(rest);
    Ctype *ctype = result_type(tok->punct, ast->ctype, rest->ctype);
    if (!is_punct(tok, '=') &&
        ast->ctype->type != CTYPE_PTR &&
        rest->ctype->type == CTYPE_PTR)
      swap(ast, rest);
    ast = ast_binop(tok->punct, ctype, ast, rest);
  }
}

static Ctype *get_ctype(Token *tok) {
  if (tok->type != TTYPE_IDENT)
    return NULL;
  if (!strcmp(tok->sval, "int"))
    return ctype_int;
  if (!strcmp(tok->sval, "char"))
    return ctype_char;
  return NULL;
}

static bool is_type_keyword(Token *tok) {
  return get_ctype(tok) != NULL;
}

static void expect(char punct) {
  Token *tok = read_token();
  if (!is_punct(tok, punct))
    error("'%c' expected, but got %s", punct, token_to_string(tok));
}

static Ast *read_decl_array_initializer(Ctype *ctype) {
  Token *tok = read_token();
  if (ctype->ptr->type == CTYPE_CHAR && tok->type == TTYPE_STRING)
    return ast_string(tok->sval);
  if (!is_punct(tok, '{'))
    error("Expected an initializer list, but got %s", token_to_string(tok));
  Ast **init = malloc(sizeof(Ast*) * ctype->size);
  for (int i = 0; i < ctype->size; i++) {
    init[i] = read_expr(0);
    result_type('=', init[i]->ctype, ctype->ptr);
    tok = read_token();
    if (is_punct(tok, '}') && (i == ctype->size - 1))
      break;
    if (!is_punct(tok, ','))
      error("comma expected, but got %s", token_to_string(tok));
    if (i == ctype->size - 1) {
      tok = read_token();
      if (!is_punct(tok, '}'))
        error("'}' expected, but got %s", token_to_string(tok));
      break;
    }
  }
  return ast_array_init(ctype->size, init);
}

static Ast *read_declinitializer(Ctype *ctype) {
  if (ctype->type == CTYPE_ARRAY)
    return read_decl_array_initializer(ctype);
  return read_expr(0);
}

static Ast *read_decl(void) {
  Ctype *ctype = get_ctype(read_token());
  Token *tok;
  for (;;) {
    tok = read_token();
    if (!is_punct(tok, '*'))
      break;
    ctype = make_ptr_type(ctype);
  }
  if (tok->type != TTYPE_IDENT)
    error("Identifier expected, but got %s", token_to_string(tok));
  Token *varname = tok;
  for (;;) {
    tok = read_token();
    if (is_punct(tok, '[')) {
      Ast *size = read_expr(0);
      if (size->type != AST_LITERAL || size->ctype->type != CTYPE_INT)
        error("Integer expected, but got %s", ast_to_string(size));
      expect(']');
      ctype = make_array_type(ctype, size->ival);
    } else {
      unget_token(tok);
      break;
    }
  }
  Ast *var = ast_lvar(ctype, varname->sval);
  expect('=');
  Ast *init = read_declinitializer(ctype);
  return ast_decl(var, init);
}

Ast *read_decl_or_stmt(void) {
  Token *tok = peek_token();
  if (!tok) return NULL;
  Ast *r = is_type_keyword(tok) ? read_decl() : read_expr(0);
  tok = read_token();
  if (!is_punct(tok, ';'))
    error("Unterminated expression: %s", token_to_string(tok));
  return r;
}

char *ctype_to_string(Ctype *ctype) {
  switch (ctype->type) {
    case CTYPE_VOID: return "void";
    case CTYPE_INT:  return "int";
    case CTYPE_CHAR: return "char";
    case CTYPE_PTR: {
      String *s = make_string();
      string_appendf(s, "%s", ctype_to_string(ctype->ptr));
      string_append(s, '*');
      return get_cstring(s);
    }
    case CTYPE_ARRAY: {
      String *s = make_string();
      string_appendf(s, "%s", ctype_to_string(ctype->ptr));
      string_appendf(s, "[%d]", ctype->size);
      return get_cstring(s);
    }
    default: error("Unknown ctype: %d", ctype);
  }
}

static void ast_to_string_int(Ast *ast, String *buf) {
  switch (ast->type) {
    case AST_LITERAL:
      switch (ast->ctype->type) {
        case CTYPE_INT:
          string_appendf(buf, "%d", ast->ival);
          break;
        case CTYPE_CHAR:
          string_appendf(buf, "'%c'", ast->c);
          break;
        default:
          error("internal error");
      }
      break;
    case AST_STRING:
      string_appendf(buf, "\"%s\"", quote_cstring(ast->sval));
      break;
    case AST_LVAR:
      string_appendf(buf, "%s", ast->lname);
      break;
    case AST_GVAR:
      string_appendf(buf, "%s", ast->gname);
      break;
    case AST_LREF:
      string_appendf(buf, "%s[%d]", ast_to_string(ast->lref), ast->lrefoff);
      break;
    case AST_GREF:
      string_appendf(buf, "%s[%d]", ast_to_string(ast->gref), ast->goff);
      break;
    case AST_FUNCALL:
      string_appendf(buf, "%s(", ast->fname);
      for (int i = 0; ast->args[i]; i++) {
        string_appendf(buf, "%s", ast_to_string(ast->args[i]));
        if (ast->args[i + 1])
          string_appendf(buf, ",");
      }
      string_appendf(buf, ")");
      break;
    case AST_DECL:
      string_appendf(buf, "(decl %s %s %s)",
                     ctype_to_string(ast->declvar->ctype),
                     ast->declvar->lname,
                     ast_to_string(ast->declinit));
      break;
    case AST_ARRAY_INIT:
      string_appendf(buf, "{");
      for (int i = 0; i < ast->size; i++) {
        ast_to_string_int(ast->array_init[i], buf);
        if (i != ast->size - 1)
          string_appendf(buf, ",");
      }
      string_appendf(buf, "}");
      break;
    case AST_ADDR:
      string_appendf(buf, "(& %s)", ast_to_string(ast->operand));
      break;
    case AST_DEREF:
      string_appendf(buf, "(* %s)", ast_to_string(ast->operand));
      break;
    default: {
      char *left = ast_to_string(ast->left);
      char *right = ast_to_string(ast->right);
      string_appendf(buf, "(%c %s %s)", ast->type, left, right);
    }
  }
}

char *ast_to_string(Ast *ast) {
  String *s = make_string();
  ast_to_string_int(ast, s);
  return get_cstring(s);
}