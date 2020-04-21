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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php_elasticapm.h"

// external libraries
#include <php_ini.h>
#include <zend_types.h>

#include "lifecycle.h"
#include "supportability.h"
#include "elasticapm_API.h"
#include "ConfigManager.h"
#include "elasticapm_assert.h"


ZEND_DECLARE_MODULE_GLOBALS( elasticapm )

Tracer* getGlobalTracer()
{
    return &(ZEND_MODULE_GLOBALS_ACCESSOR(elasticapm, globalTracer));
}

#ifndef ZEND_PARSE_PARAMETERS_NONE
#   define ZEND_PARSE_PARAMETERS_NONE() \
        ZEND_PARSE_PARAMETERS_START(0, 0) \
        ZEND_PARSE_PARAMETERS_END()
#endif

static inline ZEND_RESULT_CODE resultCodeToZend( ResultCode resultCode )
{
    if ( resultCode == resultSuccess ) return SUCCESS;
    return FAILURE;
}

static inline ResultCode zendToResultCode( ZEND_RESULT_CODE zendResultCode )
{
    if ( zendResultCode == SUCCESS ) return resultSuccess;
    return resultFailure;
}

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(elasticapm)
{
    return resultCodeToZend( elasticApmRequestInit() );
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(elasticapm)
{
    return resultCodeToZend( elasticApmRequestShutdown() );
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(elasticapm)
{
    elasticapmModuleInfo( zend_module );
}
/* }}} */

#define ELASTICAPM_INI_ENTRY_IMPL( optName, isReloadableFlag ) \
    PHP_INI_ENTRY( \
        "elasticapm." optName \
        , /* default value: */ NULL \
        , isReloadableFlag \
        , /* on_modify (validator): */ NULL )

#define ELASTICAPM_INI_ENTRY( optName ) ELASTICAPM_INI_ENTRY_IMPL( optName, PHP_INI_ALL )

#define ELASTICAPM_NOT_RELOADABLE_INI_ENTRY( optName ) ELASTICAPM_INI_ENTRY_IMPL( optName, PHP_INI_PERDIR )

PHP_INI_BEGIN()
    #ifdef PHP_WIN32
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_ALLOW_ABORT_DIALOG )
    #endif
    ELASTICAPM_NOT_RELOADABLE_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_ABORT_ON_MEMORY_LEAK )
    #if ( ELASTICAPM_ASSERT_ENABLED_01 != 0 )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_ASSERT_LEVEL )
    #endif
    ELASTICAPM_NOT_RELOADABLE_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_ENABLED )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_INTERNAL_CHECKS_LEVEL )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_FILE )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_LEVEL )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_LEVEL_FILE )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_LEVEL_STDERR )
    #ifndef PHP_WIN32
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_LEVEL_SYSLOG )
    #endif
    #ifdef PHP_WIN32
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_LOG_LEVEL_WIN_SYS_DEBUG )
    #endif
    #if ( ELASTICAPM_MEMORY_TRACKING_ENABLED_01 != 0 )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_MEMORY_TRACKING_LEVEL )
    #endif
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_SECRET_TOKEN )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_SERVER_URL )
    ELASTICAPM_INI_ENTRY( ELASTICAPM_CFG_OPT_NAME_SERVICE_NAME )
PHP_INI_END()

#undef ELASTICAPM_INI_ENTRY_IMPL
#undef ELASTICAPM_INI_ENTRY
#undef ELASTICAPM_NOT_RELOADABLE_INI_ENTRY
#undef ELASTICAPM_SECRET_INI_ENTRY

ResultCode registerElasticApmIniEntries( int module_number, IniEntriesRegistrationState* iniEntriesRegistrationState )
{
    ELASTICAPM_ASSERT_VALID_PTR( iniEntriesRegistrationState );

    ResultCode resultCode;
    const ConfigManager* const cfgManager = getGlobalTracer()->configManager;

    resultCode = zendToResultCode( (ZEND_RESULT_CODE)REGISTER_INI_ENTRIES() );
    if ( resultCode != resultSuccess )
    {
        ELASTICAPM_LOG_ERROR( "REGISTER_INI_ENTRIES(...) failed. resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
        goto failure;
    }
    iniEntriesRegistrationState->entriesRegistered = true;

    ELASTICAPM_FOR_EACH_OPTION_ID( optId )
    {
        bool isSecret;
        String name = NULL;
        String envVarName = NULL;
        StringView iniName;
        getConfigManagerOptionMetadata(
                cfgManager,
                optId,
                /* out */ &isSecret,
                /* out */ &name,
                /* out */ &envVarName,
                /* out */ &iniName );
        if ( ! isSecret ) continue;
        resultCode = zend_ini_register_displayer( (char*)( iniName.begin ), (uint32_t)( iniName.length ), displaySecretIniValue );
        if ( resultCode != resultSuccess )
        {
            ELASTICAPM_LOG_ERROR( "REGISTER_INI_DISPLAYER(...) failed. resultCode: %s (%d). iniName: %.*s."
                                , resultCodeToString( resultCode ), resultCode, (int) iniName.length, iniName.begin );
            goto failure;
        }
    }

    resultCode = resultSuccess;

    finally:
    return resultCode;

    failure:
    unregisterElasticApmIniEntries( module_number, iniEntriesRegistrationState );
    goto finally;
}

void unregisterElasticApmIniEntries( int module_number, IniEntriesRegistrationState* iniEntriesRegistrationState )
{
    if ( iniEntriesRegistrationState->entriesRegistered )
    {
        UNREGISTER_INI_ENTRIES();
        iniEntriesRegistrationState->entriesRegistered = false;
    }
}

PHP_MINIT_FUNCTION(elasticapm)
{
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_NOT_SET", logLevel_not_set, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_OFF", logLevel_off, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_CRITICAL", logLevel_critical, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_ERROR", logLevel_error, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_WARNING", logLevel_warning, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_NOTICE", logLevel_notice, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_INFO", logLevel_info, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_DEBUG", logLevel_debug, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_LOG_LEVEL_TRACE", logLevel_trace, CONST_CS|CONST_PERSISTENT );

    REGISTER_LONG_CONSTANT( "ELASTICAPM_ASSERT_LEVEL_NOT_SET", assertLevel_not_set, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_ASSERT_LEVEL_OFF", assertLevel_off, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_ASSERT_LEVEL_O_1", assertLevel_O_1, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_ASSERT_LEVEL_O_N", assertLevel_O_n, CONST_CS|CONST_PERSISTENT );
    REGISTER_LONG_CONSTANT( "ELASTICAPM_ASSERT_LEVEL_ALL", assertLevel_all, CONST_CS|CONST_PERSISTENT );

    return resultCodeToZend( elasticApmModuleInit( type, module_number ) );
}

PHP_MSHUTDOWN_FUNCTION(elasticapm)
{
    return resultCodeToZend( elasticApmModuleShutdown( type, module_number ) );
}

/* {{{ bool elasticapm_is_enabled()
 */
PHP_FUNCTION( elasticapm_is_enabled )
{
    ZEND_PARSE_PARAMETERS_NONE();

    RETURN_BOOL( elasticApmIsEnabled() )
}
/* }}} */

/* {{{ string elasticapm_get_current_transaction_id()
 */
PHP_FUNCTION( elasticapm_get_current_transaction_id )
{
    const String retVal = elasticApmGetCurrentTransactionId();

    ZEND_PARSE_PARAMETERS_NONE();

    if ( retVal == NULL ) RETURN_NULL()
    RETURN_STRING( retVal )
}
/* }}} */

/* {{{ string elasticapm_get_current_trace_id()
 */
PHP_FUNCTION( elasticapm_get_current_trace_id )
{
    const String retVal = elasticApmGetCurrentTraceId();

    ZEND_PARSE_PARAMETERS_NONE();

    if ( retVal == NULL ) RETURN_NULL()
    RETURN_STRING( retVal )
}
/* }}} */

/* {{{ mixed elasticapm_get_config_option_by_name( string $optionName )
 */
PHP_FUNCTION( elasticapm_get_config_option_by_name )
{
    char* optionName = NULL;
    size_t optionNameLength = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING( optionName, optionNameLength )
    ZEND_PARSE_PARAMETERS_END();

    if ( elasticApmGetConfigOption( optionName, return_value ) != resultSuccess ) RETURN_NULL()
}
/* }}} */

/* {{{ arginfo
 */
ZEND_BEGIN_ARG_INFO(elasticapm_no_paramters_arginfo, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX( elasticapm_string_paramter_arginfo, 0, 0, 1 )
    ZEND_ARG_TYPE_INFO( 0, optionName, IS_STRING, 0 )
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ elasticapm_functions[]
 */
static const zend_function_entry elasticapm_functions[] =
{
    PHP_FE( elasticapm_is_enabled, elasticapm_no_paramters_arginfo )
    PHP_FE( elasticapm_get_current_transaction_id, elasticapm_no_paramters_arginfo )
    PHP_FE( elasticapm_get_current_trace_id, elasticapm_no_paramters_arginfo )
    PHP_FE( elasticapm_get_config_option_by_name, elasticapm_string_paramter_arginfo )
    PHP_FE_END
};
/* }}} */

/* {{{ elasticapm_module_entry
 */
zend_module_entry elasticapm_module_entry = {
	STANDARD_MODULE_HEADER,
	"elasticapm",					/* Extension name */
	elasticapm_functions,			/* zend_function_entry */
	PHP_MINIT(elasticapm),		    /* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(elasticapm),		/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(elasticapm),			/* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(elasticapm),		/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(elasticapm),			/* PHP_MINFO - Module info */
	PHP_ELASTICAPM_VERSION,		    /* Version */
	PHP_MODULE_GLOBALS(elasticapm), /* PHP_MODULE_GLOBALS */
	NULL, 					        /* PHP_GINIT */
	NULL,		                    /* PHP_GSHUTDOWN */
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_ELASTICAPM
#   ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#   endif
ZEND_GET_MODULE(elasticapm)
#endif
