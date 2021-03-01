/*
 * Licensed to Elasticsearch B.V. under one or more contributor
 * license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright
 * ownership. Elasticsearch B.V. licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "elastic_apm_API.h"
#include "util.h"
#include "log.h"
#include "Tracer.h"
#include "elastic_apm_alloc.h"
#include "numbered_intercepting_callbacks.h"
#include "tracer_PHP_part.h"
#include "backend_comm.h"

#define ELASTIC_APM_CURRENT_LOG_CATEGORY ELASTIC_APM_LOG_CATEGORY_EXT_API

bool elasticApmIsEnabled()
{
    const bool result = getTracerCurrentConfigSnapshot( getGlobalTracer() )->enabled;

    ELASTIC_APM_LOG_TRACE( "Result: %s", boolToString( result ) );
    return result;
}

ResultCode elasticApmGetConfigOption( String optionName, zval* return_value )
{
    ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY_MSG( "optionName: `%s'", optionName );

    char txtOutStreamBuf[ELASTIC_APM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE];
    GetConfigManagerOptionValueByNameResult getOptValueByNameRes;
    getOptValueByNameRes.txtOutStream = ELASTIC_APM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );
    getOptValueByNameRes.streamedParsedValue = NULL;
    const ResultCode resultCode = getConfigManagerOptionValueByName(
            getGlobalTracer()->configManager
            , optionName
            , &getOptValueByNameRes );
    if ( resultCode == resultSuccess ) *return_value = getOptValueByNameRes.parsedValueAsZval;

    ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT_MSG(
            "ResultCode: %s. Option's: name: `%s', value: %s"
            , resultCodeToString( resultCode ), optionName, getOptValueByNameRes.streamedParsedValue );
    return resultCode;
}

enum
{
    maxFunctionsToIntercept = numberedInterceptingCallbacksCount
};
static uint32_t g_nextFreeFunctionToInterceptId = 0;
struct CallToInterceptData
{
    zif_handler originalHandler;
    zend_function* funcEntry;
};
typedef struct CallToInterceptData CallToInterceptData;
static CallToInterceptData g_functionsToInterceptData[maxFunctionsToIntercept];

static zend_execute_data* g_interceptedCallInProgressZendExecuteData = NULL;
static zif_handler g_interceptedCallInProgressOriginalHandler = NULL;
static uint32_t g_interceptedCallInProgressRegistrationId = 0;

static
void internalFunctionCallInterceptingImpl( uint32_t interceptRegistrationId, zend_execute_data* execute_data, zval* return_value )
{
    ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY_MSG( "interceptRegistrationId: %u", interceptRegistrationId );

    if ( g_interceptedCallInProgressRegistrationId != 0 )
    {
        ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY_MSG(
                "There's already an intercepted call in progress with interceptRegistrationId: %u."
                "Nesting intercepted calls is not supported yet so invoking the original handler directly..."
                , g_interceptedCallInProgressRegistrationId );
        g_functionsToInterceptData[ interceptRegistrationId ].originalHandler( execute_data, return_value );
        return;
    }

    ELASTIC_APM_ASSERT( g_interceptedCallInProgressZendExecuteData == NULL
                        , "call in progress interceptRegistrationId: %u, nested call interceptRegistrationId: %u"
                        , g_interceptedCallInProgressRegistrationId, interceptRegistrationId );
    ELASTIC_APM_ASSERT( g_interceptedCallInProgressOriginalHandler == NULL
                        , "call in progress interceptRegistrationId: %u, nested call interceptRegistrationId: %u"
                        , g_interceptedCallInProgressRegistrationId, interceptRegistrationId );
    g_interceptedCallInProgressZendExecuteData = execute_data;
    g_interceptedCallInProgressOriginalHandler = g_functionsToInterceptData[ interceptRegistrationId ].originalHandler;
    g_interceptedCallInProgressRegistrationId = interceptRegistrationId;

    tracerPhpPartInterceptedCall( interceptRegistrationId, execute_data, return_value );

    g_interceptedCallInProgressZendExecuteData = NULL;
    g_interceptedCallInProgressOriginalHandler = NULL;
    g_interceptedCallInProgressRegistrationId = 0;

    ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT_MSG( "interceptRegistrationId: %u", interceptRegistrationId );
}

void resetCallInterceptionOnRequestShutdown()
{
    // We restore original handlers in the reverse order
    // so that if the same function is registered for interception more than once
    // the original handler will be restored correctly
    ELASTIC_APM_FOR_EACH_BACKWARDS( i, g_nextFreeFunctionToInterceptId )
    {
        CallToInterceptData* data = &( g_functionsToInterceptData[ i ] );

        data->funcEntry->internal_function.handler = data->originalHandler;
    }

    g_nextFreeFunctionToInterceptId = 0;
}

bool addToFunctionsToInterceptData( zend_function* funcEntry, uint32_t* interceptRegistrationId )
{
    if ( g_nextFreeFunctionToInterceptId >= maxFunctionsToIntercept )
    {
        ELASTIC_APM_LOG_ERROR( "Reached maxFunctionsToIntercept."
                               " maxFunctionsToIntercept: %u. g_nextFreeFunctionToInterceptId: %u."
                               , maxFunctionsToIntercept, g_nextFreeFunctionToInterceptId );
        return false;
    }

    *interceptRegistrationId = g_nextFreeFunctionToInterceptId ++;
    g_functionsToInterceptData[ *interceptRegistrationId ].funcEntry = funcEntry;
    g_functionsToInterceptData[ *interceptRegistrationId ].originalHandler = funcEntry->internal_function.handler;
    funcEntry->internal_function.handler = g_numberedInterceptingCallback[ *interceptRegistrationId ];

    return true;
}


ResultCode elasticApmInterceptCallsToInternalMethod( String className, String methodName, uint32_t* interceptRegistrationId )
{
    ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "className: `%s'; methodName: `%s'", className, methodName );

    ResultCode resultCode;

    zend_class_entry* classEntry = zend_hash_str_find_ptr( CG( class_table ), className, strlen( className ) );
    if ( classEntry == NULL )
    {
        ELASTIC_APM_LOG_ERROR( "zend_hash_str_find_ptr( CG( class_table ), ... ) failed. className: `%s'", className );
        resultCode = resultFailure;
        goto failure;
    }

    zend_function* funcEntry = zend_hash_str_find_ptr( &classEntry->function_table, methodName, strlen( methodName ) );
    if ( funcEntry == NULL )
    {
        ELASTIC_APM_LOG_ERROR( "zend_hash_str_find_ptr( &classEntry->function_table, ... ) failed."
                               " className: `%s'; methodName: `%s'", className, methodName );
        resultCode = resultFailure;
        goto failure;
    }

    if ( ! addToFunctionsToInterceptData( funcEntry, interceptRegistrationId ) )
    {
        resultCode = resultFailure;
        goto failure;
    }

    resultCode = resultSuccess;

    finally:

    ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
    return resultCode;

    failure:
    goto finally;
}

ResultCode elasticApmInterceptCallsToInternalFunction( String functionName, uint32_t* interceptRegistrationId )
{
    ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "functionName: `%s'", functionName );

    ResultCode resultCode;

    zend_function* funcEntry = zend_hash_str_find_ptr( EG( function_table ), functionName, strlen( functionName ) );
    if ( funcEntry == NULL )
    {
        ELASTIC_APM_LOG_ERROR( "zend_hash_str_find_ptr( EG( function_table ), ... ) failed."
                               " functionName: `%s'", functionName );
        resultCode = resultFailure;
        goto failure;
    }

    if ( ! addToFunctionsToInterceptData( funcEntry, interceptRegistrationId ) )
    {
        resultCode = resultFailure;
        goto failure;
    }

    resultCode = resultSuccess;

    finally:

    ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "resultCode: %s (%d). interceptRegistrationId: %u.", resultCodeToString( resultCode ), resultCode, *interceptRegistrationId );
    return resultCode;

    failure:
    goto finally;
}

void elasticApmCallInterceptedOriginal( zval* return_value )
{
    ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY();

    ELASTIC_APM_ASSERT( g_interceptedCallInProgressZendExecuteData != NULL, "" );
    ELASTIC_APM_ASSERT( g_interceptedCallInProgressOriginalHandler != NULL, "" );

    g_interceptedCallInProgressOriginalHandler( g_interceptedCallInProgressZendExecuteData, return_value );

    ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT();
}

ResultCode elasticApmSendToServer( double serverTimeoutMilliseconds, StringView serializedMetadata, StringView serializedEvents )
{
    ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY();

    ResultCode resultCode;
    Tracer* const tracer = getGlobalTracer();

    ELASTIC_APM_CALL_IF_FAILED_GOTO( saveMetadataFromPhpPart( &tracer->requestScoped, serializedMetadata ) );
    sendEventsToApmServer( serverTimeoutMilliseconds, getTracerCurrentConfigSnapshot( tracer ), serializedEvents );

    resultCode = resultSuccess;

    finally:
    ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
    return resultCode;

    failure:
    goto finally;
}
