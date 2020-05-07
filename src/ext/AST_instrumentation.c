/*
   +----------------------------------------------------------------------+
   | Elastic APM agent for PHP                                            |
   +----------------------------------------------------------------------+
   | Copyright (c) 2020 Elasticsearch B.V.                                |
   +----------------------------------------------------------------------+
   | Elasticsearch B.V. licenses this file under the Apache 2.0 License.  |
   | See the LICENSE file in the project root for more information.       |
   +----------------------------------------------------------------------+
 */

#include "AST_instrumentation.h"
#include "log.h"
#include <zend_ast.h>
#include <zend_compile.h>
#include <zend_arena.h>
#include "util.h"

zend_ast_process_t original_zend_ast_process;

#define ZEND_AST_ALLOC( size ) zend_arena_alloc(&CG(ast_arena), size);

String zendAstKindToString( zend_ast_kind kind )
{
#   define ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( enumMember ) \
        case enumMember: \
            return #enumMember; \
    /**/

    switch ( kind )
    {
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ZVAL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONSTANT )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ZNODE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_FUNC_DECL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLOSURE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_METHOD )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLASS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ARROW_FUNC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ARG_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ARRAY )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ENCAPS_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_EXPR_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_STMT_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_IF )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_SWITCH_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CATCH_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PARAM_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLOSURE_USES )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PROP_DECL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONST_DECL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLASS_CONST_DECL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_NAME_LIST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_TRAIT_ADAPTATIONS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_USE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_MAGIC_CONST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_TYPE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONSTANT_CLASS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_VAR )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_UNPACK )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_UNARY_PLUS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_UNARY_MINUS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CAST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_EMPTY )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ISSET )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_SILENCE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_SHELL_EXEC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLONE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_EXIT )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PRINT )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_INCLUDE_OR_EVAL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_UNARY_OP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PRE_INC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PRE_DEC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_POST_INC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_POST_DEC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_YIELD_FROM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLASS_NAME )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_GLOBAL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_UNSET )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_RETURN )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_LABEL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_REF )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_HALT_COMPILER )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ECHO )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_THROW )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_GOTO )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_BREAK )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONTINUE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_DIM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PROP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_STATIC_PROP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CALL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CLASS_CONST )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ASSIGN )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ASSIGN_REF )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ASSIGN_OP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_BINARY_OP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_GREATER )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_GREATER_EQUAL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_AND )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_OR )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ARRAY_ELEM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_NEW )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_INSTANCEOF )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_YIELD )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_COALESCE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_ASSIGN_COALESCE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_STATIC )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_WHILE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_DO_WHILE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_IF_ELEM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_SWITCH )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_SWITCH_CASE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_DECLARE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_USE_TRAIT )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_TRAIT_PRECEDENCE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_METHOD_REFERENCE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_NAMESPACE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_USE_ELEM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_TRAIT_ALIAS )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_GROUP_USE )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PROP_GROUP )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_METHOD_CALL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_STATIC_CALL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONDITIONAL )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_TRY )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CATCH )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PARAM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_PROP_ELEM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_CONST_ELEM )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_FOR )
        ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE( ZEND_AST_FOREACH )

        default:
            return "UNKNOWN";
    }
#   undef ELASTICAPM_GEN_ENUM_TO_STRING_SWITCH_CASE
}

static inline
size_t calcAstListAllocSize( uint32_t children )
{
    return sizeof( zend_ast_list ) - sizeof( zend_ast* ) + sizeof( zend_ast* ) * children;
}

static
zend_ast* createStringAst( char* str, size_t len, uint32_t attr )
{
    zval zv;
    zend_ast * ast;
    ZVAL_NEW_STR( &zv, zend_string_init( str, len, 0 ) );
    ast = zend_ast_create_zval_with_lineno( &zv, 0 );
    ast->attr = attr;
    return ast;
}

static
zend_ast* createCatchTypeAst()
{
    zend_ast * name = createStringAst( "Throwable", sizeof( "Throwable" ) - 1, ZEND_NAME_FQ );
    return zend_ast_create_list( 1, ZEND_AST_NAME_LIST, name );
}

static
zend_ast* createCatchAst( uint32_t lineno )
{
    zend_ast * exVarNameAst = createStringAst( "ex", sizeof( "ex" ) - 1, 0 );
    zend_ast * catchTypeAst = createCatchTypeAst();

    zend_ast * instrumentationPostHookCallAst =
            zend_ast_create( ZEND_AST_CALL
                             , createStringAst( "instrumentationPostHookException", sizeof( "instrumentationPostHookException" ) - 1, ZEND_NAME_FQ )
                             , zend_ast_create_list( 1
                                                     , ZEND_AST_ARG_LIST
                                                     , zend_ast_create( ZEND_AST_VAR
                                                                        , createStringAst( "ex", sizeof( "ex" ) - 1, 0 ) )
            ) );

    zend_ast * throwAst = zend_ast_create( ZEND_AST_THROW, instrumentationPostHookCallAst );
    throwAst->lineno = lineno;
    zend_ast * throwStmtListAst = zend_ast_create_list( 1, ZEND_AST_STMT_LIST, throwAst );
    throwStmtListAst->lineno = lineno;
    zend_ast * catchAst = zend_ast_create_list( 1
                                                , ZEND_AST_CATCH_LIST
                                                , zend_ast_create( ZEND_AST_CATCH
                                                                   , catchTypeAst
                                                                   , exVarNameAst
                                                                   , throwStmtListAst ) );
    catchAst->lineno = lineno;
    return catchAst;
}

struct TransformContext
{
    bool isInsideFunction;
    bool isFunctionRetByRef;
};
typedef struct TransformContext TransformContext;

TransformContext g_transformContext = { .isInsideFunction = false, .isFunctionRetByRef = false };

TransformContext makeTransformContext( TransformContext* base, bool isInsideFunction, bool isFunctionRetByRef )
{
    TransformContext transformCtx = *base;
    transformCtx.isInsideFunction = isInsideFunction;
    transformCtx.isFunctionRetByRef = isFunctionRetByRef;
    return transformCtx;
}

static
zend_ast* transformAst( zend_ast* ast, int nestingDepth );

static
zend_ast* transformFunctionAst( zend_ast* ast, int nestingDepth )
{
    char txtOutStreamBuf[ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( ast->kind ) );

    zend_ast * transformedAst;
    zend_ast_decl* funcDeclAst = (zend_ast_decl*) ast;

    TransformContext savedTransformCtx = makeTransformContext(
            &g_transformContext
            , /* isInsideFunction: */ true
            , /* isFunctionRetByRef */ funcDeclAst->flags & ZEND_ACC_RETURN_REFERENCE );

    if ( ! isStringViewPrefixIgnoringCase( stringToView( ZSTR_VAL( funcDeclAst->name ) )
                                           , ELASTICAPM_STRING_LITERAL_TO_VIEW( "functionToInstrument" ) ) )
    {
        transformedAst = ast;
        goto finally;
    }

    zend_ast_list* funcStmtListAst = zend_ast_get_list( funcDeclAst->child[ 2 ] );

    // guess at feasible line numbers
    uint32_t funcStmtBeginLineNumber = funcStmtListAst->lineno;
    uint32_t funcStmtEndLineNumber = funcStmtListAst->child[ funcStmtListAst->children - 1 ]->lineno;

    zend_ast * callInstrumentationPreHookAst =
            zend_ast_create(
                    ZEND_AST_CALL
                    , createStringAst( "instrumentationPreHook", sizeof( "instrumentationPreHook" ) - 1, ZEND_NAME_FQ )
                    , zend_ast_create_list( 1
                                            , ZEND_AST_ARG_LIST
                                            , zend_ast_create( ZEND_AST_CALL
                                                               , createStringAst( "func_get_args", sizeof( "func_get_args" ) - 1, ZEND_NAME_FQ )
                                                               , zend_ast_create_list( 0, ZEND_AST_ARG_LIST ) )
            ) );

    zend_ast * callInstrumentationPostHookAst =
            zend_ast_create(
                    ZEND_AST_CALL
                    , createStringAst( "instrumentationPostHookRetVoid", sizeof( "instrumentationPostHookRetVoid" ) - 1, ZEND_NAME_FQ )
                    , zend_ast_create_list( 0, ZEND_AST_ARG_LIST ) );

    zend_ast * catchAst = createCatchAst( funcStmtEndLineNumber );
    zend_ast * finallyAst = NULL;

    zend_ast * tryCatchAst = zend_ast_create( ZEND_AST_TRY, transformAst( funcDeclAst->child[ 2 ], nestingDepth + 1 ), catchAst, finallyAst );
    tryCatchAst->lineno = funcStmtBeginLineNumber;
    zend_ast_list* newFuncBodyAst = ZEND_AST_ALLOC( calcAstListAllocSize( 3 ) );
    newFuncBodyAst->kind = ZEND_AST_STMT_LIST;
    newFuncBodyAst->lineno = funcStmtBeginLineNumber;
    newFuncBodyAst->children = 3;
    newFuncBodyAst->child[ 0 ] = callInstrumentationPreHookAst;
    newFuncBodyAst->child[ 1 ] = tryCatchAst;
    newFuncBodyAst->child[ 2 ] = callInstrumentationPostHookAst;
    funcDeclAst->child[ 2 ] = (zend_ast*) newFuncBodyAst;
    transformedAst = ast;

    finally:
    g_transformContext = savedTransformCtx;
    textOutputStreamRewind( &txtOutStream );
    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( transformedAst->kind ) );
    return transformedAst;
}

static
zend_ast* transformReturnAst( zend_ast* ast, int nestingDepth )
{
    char txtOutStreamBuf[ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( ast->kind ) );

    zend_ast * transformedAst;

    // if there isn't an active function then don't wrap it
    // e.g. return at file scope
    if ( ! g_transformContext.isInsideFunction )
    {
        transformedAst = ast;
        goto finally;
    }

    zend_ast * returnExprAst = ast->child[ 0 ];
    // If it's an empty return;
    if ( returnExprAst == NULL )
    {
        zend_ast * callInstrumentationPostHookAst = zend_ast_create(
                ZEND_AST_CALL
                , createStringAst( "instrumentationPostHookRetVoid", sizeof( "instrumentationPostHookRetVoid" ) - 1, ZEND_NAME_FQ )
                , zend_ast_create_list( 0, ZEND_AST_ARG_LIST ) );
        transformedAst = zend_ast_create_list( 2, ZEND_AST_STMT_LIST, callInstrumentationPostHookAst, ast );
        goto finally;
    }

    // Either: return by reference or not
    char* name;
    size_t len;
    if ( g_transformContext.isFunctionRetByRef )
    {
        name = "instrumentationPostHookRetByRef";
        len = sizeof( "instrumentationPostHookRetByRef" ) - 1;
    }
    else
    {
        name = "instrumentationPostHookRetNotByRef";
        len = sizeof( "instrumentationPostHookRetNotByRef" ) - 1;
    }
    zend_ast * callInstrumentationPostHookAst = zend_ast_create(
            ZEND_AST_CALL
            , createStringAst( name, len, ZEND_NAME_FQ )
            , zend_ast_create_list( 1, ZEND_AST_ARG_LIST, returnExprAst ) );
    ast->child[ 0 ] = callInstrumentationPostHookAst;
    transformedAst = ast;

    finally:
    textOutputStreamRewind( &txtOutStream );
    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( transformedAst->kind ) );
    return transformedAst;
}

static
zend_ast* transformChildrenAst( zend_ast* ast, int nestingDepth )
{
    char txtOutStreamBuf[ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( ast->kind ) );

    zend_ast * transformedAst = ast;

    uint32_t childrenCount = zend_ast_get_num_children( ast );
    ELASTICAPM_FOR_EACH_INDEX( i, childrenCount )
    {
        if ( ast->child[ i ] != NULL )
        {
            ast->child[ i ] = transformAst( ast->child[ i ], nestingDepth + 1 );
        }
    }

    textOutputStreamRewind( &txtOutStream );
    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( transformedAst->kind ) );
    return transformedAst;
}

static
zend_ast* transformAst( zend_ast* ast, int nestingDepth )
{
    char txtOutStreamBuf[ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( ast->kind ) );

    zend_ast * transformedAst;

    if ( zend_ast_is_list( ast ) )
    {
        zend_ast_list* list = zend_ast_get_list( ast );
        uint32_t i;
        // TODO: Sergey Kleyman: process list itself somehow?
        // from original code (https://github.com/morrisonlevi/ddastrace/blob/master/process.c#L173)
        for ( i = 0 ; i < list->children ; i ++ )
        {
            if ( list->child[ i ] )
            {
                list->child[ i ] = transformAst( list->child[ i ], nestingDepth + 1 );
            }
        }
        transformedAst = ast;
        goto finally;
    }

    switch ( ast->kind )
    {
        case ZEND_AST_ZVAL:
        case ZEND_AST_CONSTANT:
            transformedAst = ast;
            goto finally;

        case ZEND_AST_FUNC_DECL:
        case ZEND_AST_METHOD:
            transformedAst = transformFunctionAst( ast, nestingDepth + 1 );
            goto finally;

        case ZEND_AST_RETURN:
            transformedAst = transformReturnAst( ast, nestingDepth + 1 );
            goto finally;

        default:
            transformedAst = transformChildrenAst( ast, nestingDepth + 1 );
            goto finally;
    }

    finally:
    textOutputStreamRewind( &txtOutStream );
    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT_MSG( "%skind: %s", streamIndent( nestingDepth, &txtOutStream ), zendAstKindToString( transformedAst->kind ) );
    return transformedAst;
}

static
void elasticApmProcessAstRoot( zend_ast* ast )
{
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "ast->kind: %s", zendAstKindToString( ast->kind ) );

    zend_ast * transformedAst = transformAst( ast, 0 );
    if ( original_zend_ast_process != NULL ) original_zend_ast_process( transformedAst );

    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT();
}

void astInstrumentationInit()
{
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY();

    original_zend_ast_process = zend_ast_process;
    zend_ast_process = elasticApmProcessAstRoot;

    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT();
}

void astInstrumentationShutdown()
{
    ELASTICAPM_LOG_DEBUG_FUNCTION_ENTRY();

    zend_ast_process = original_zend_ast_process;

    ELASTICAPM_LOG_DEBUG_FUNCTION_EXIT();
}
