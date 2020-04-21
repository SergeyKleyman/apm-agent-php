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

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#ifndef PHP_WIN32
#   include <syslog.h>
#endif
#include "elasticapm_clock.h"
#include "util.h"
#include "elasticapm_alloc.h"
#include "platform.h"
#include "TextOutputStream.h"
#include "Tracer.h"

String logLevelNames[ numberOfLogLevels ] =
{
    [ logLevel_off ] = "OFF",
    [ logLevel_critical ] = "CRITICAL",
    [ logLevel_error ] = "ERROR",
    [ logLevel_warning ] = "WARNING",
    [ logLevel_notice ] = "NOTICE",
    [ logLevel_info ] = "INFO",
    [ logLevel_debug ] = "DEBUG",
    [ logLevel_trace ] = "TRACE"
};

enum { loggerMessageBufferSize = 1000 * 1000 + 1 };

struct TimeZoneShift
{
    bool isPositive;
    UInt8 hours;
    UInt8 minutes;
};
typedef struct TimeZoneShift TimeZoneShift;

struct LocalTime
{
    UInt16 years;
    UInt8 months;
    UInt8 days;
    UInt8 hours;
    UInt8 minutes;
    UInt8 seconds;
    UInt32 microseconds;
    TimeZoneShift timeZoneShift;
};
typedef struct LocalTime LocalTime;

static void calcTimeZoneShift( long secondsAheadUtc, TimeZoneShift* timeZoneShift )
{
    const long secondsAheadUtcAbs = secondsAheadUtc >= 0 ? secondsAheadUtc : -secondsAheadUtc;
    const unsigned long minutesAheadUtcAbs = (long)( round( secondsAheadUtcAbs / 60.0 ) );

    timeZoneShift->isPositive = secondsAheadUtc >= 0;
    timeZoneShift->minutes = (UInt8) ( minutesAheadUtcAbs % 60 );
    timeZoneShift->hours = (UInt8) ( minutesAheadUtcAbs / 60 );
}

static void getCurrentLocalTime( LocalTime* localCurrentTime )
{
    struct timeval currentTime_UTC_timeval = { 0 };
    struct tm currentTime_local_tm = { 0 };
    long secondsAheadUtc = 0;

    if ( getSystemClockCurrentTimeAsUtc( &currentTime_UTC_timeval ) != 0 ) return;
    if ( ! convertUtcToLocalTime( currentTime_UTC_timeval.tv_sec, &currentTime_local_tm, &secondsAheadUtc ) ) return;

    // tm_year is years since 1900
    localCurrentTime->years = (UInt16) ( 1900 + currentTime_local_tm.tm_year );
    // tm_mon is months since January - [0, 11]
    localCurrentTime->months = (UInt8) ( currentTime_local_tm.tm_mon + 1 );
    localCurrentTime->days = (UInt8) currentTime_local_tm.tm_mday;
    localCurrentTime->hours = (UInt8) currentTime_local_tm.tm_hour;
    localCurrentTime->minutes = (UInt8) currentTime_local_tm.tm_min;
    localCurrentTime->seconds = (UInt8) currentTime_local_tm.tm_sec;
    localCurrentTime->microseconds = (UInt32) currentTime_UTC_timeval.tv_usec;

    calcTimeZoneShift( secondsAheadUtc, &( localCurrentTime->timeZoneShift ) );
}

// 2020-02-15 21:51:32.123456+02:00 | ERROR    | 12345:67890 | lifecycle.c:482     | sendEventsToApmServer        | Couldn't connect to server blah blah blah blah blah blah blah blah
// 2020-02-15 21:51:32.123456+02:00 | WARNING  | 12345:67890 | ConfigManager.c:45  | constructSnapshotUsingDefaults | Not found blah blah blah blah blah blah blah blah
// 2020-02-15 21:51:32.123456+02:00 | CRITICAL |   345:  345 | lifecycle.c:482     | constructSnapshotUsingDefaults | Send failed. Error message: Couldn't connect to server. server_url: `http://localhost:8200'
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^   ^^^^^ ^^^^^   ^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ^                                  ^          ^     ^       ^                     ^                              Message text
// ^                                  ^          ^     ^       ^                     function name (padded with spaces on the right to 30 chars)
// ^                                  ^          ^     ^       file name:line number (padded with spaces on the right to 20 chars)
// ^                                  ^          ^     thread ID (padded with spaces on the left to 5 chars) - included only if ZTS is defined
// ^                                  ^          process ID (padded with spaces on the left to 5 chars)
// ^                                  level (padded with spaces on the right to 8 chars)
// timestamp (no padding)


static void appendTimestamp( TextOutputStream* txtOutStream )
{
    // 2020-02-15 21:51:32.123456+02:00

    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

    LocalTime timestamp = { 0 };
    getCurrentLocalTime( &timestamp );

    streamPrintf(
            txtOutStream
            , "%04d-%02d-%02d %02d:%02d:%02d.%06d%c%02d:%02d"
            , timestamp.years
            , timestamp.months
            , timestamp.days
            , timestamp.hours
            , timestamp.minutes
            , timestamp.seconds
            , timestamp.microseconds
            , timestamp.timeZoneShift.isPositive ? '+' : '-'
            , timestamp.timeZoneShift.hours
            , timestamp.timeZoneShift.minutes );
}

static const char logLinePartsSeparator[] = " | ";

static void appendSeparator( TextOutputStream* txtOutStream )
{
    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

    streamStringView( ELASTICAPM_STRING_LITERAL_TO_VIEW( logLinePartsSeparator ), txtOutStream );
}

#define ELASTICAPM_LOG_FMT_PAD_START ""
#define ELASTICAPM_LOG_FMT_PAD_END "-"

#define ELASTICAPM_LOG_FMT_STRING_PAD_END( minWidth ) \
    "%" ELASTICAPM_LOG_FMT_PAD_END ELASTICAPM_PP_STRINGIZE( minWidth ) "s"

#define ELASTICAPM_LOG_FMT_NUMBER_PAD_END( minWidth ) \
    "%" ELASTICAPM_LOG_FMT_PAD_END ELASTICAPM_PP_STRINGIZE( minWidth ) "d"

#define ELASTICAPM_LOG_FMT_NUMBER_PAD_START( minWidth ) \
    "%" ELASTICAPM_LOG_FMT_PAD_START ELASTICAPM_PP_STRINGIZE( minWidth ) "d"

static void appendLevel( LogLevel level, TextOutputStream* txtOutStream )
{
    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

#define ELASTICAPM_LOG_FMT_LEVEL_MIN_WIDTH 8

    if ( level < numberOfLogLevels )
    {
        // if it's a level with a name
        streamPrintf( txtOutStream
                    , ELASTICAPM_LOG_FMT_STRING_PAD_END( ELASTICAPM_LOG_FMT_LEVEL_MIN_WIDTH )
                    , logLevelNames[ level ] );
    }
    else
    {
        // otherwise print it as a number
        streamPrintf( txtOutStream
                    // TODO: Sergey Kleyman: Test with log level 100
                    , ELASTICAPM_LOG_FMT_NUMBER_PAD_END( ELASTICAPM_LOG_FMT_LEVEL_MIN_WIDTH )
                    , level );
    }

#undef ELASTICAPM_LOG_FMT_LEVEL_MIN_WIDTH
}

#ifndef ELASTICAPM_HAS_THREADS_01
#   ifdef ZTS
#       define ELASTICAPM_HAS_THREADS_01 1
#   else
#       define ELASTICAPM_HAS_THREADS_01 0
#   endif
#endif

static void appendProcessThreadIds( TextOutputStream* txtOutStream )
{
    // 12345:67890

    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

    pid_t processId = getCurrentProcessId();

#define ELASTICAPM_LOG_FMT_PROCESS_ID_MIN_WIDTH 5

    streamPrintf(
            txtOutStream
            , ELASTICAPM_LOG_FMT_NUMBER_PAD_START( ELASTICAPM_LOG_FMT_PROCESS_ID_MIN_WIDTH )
            , processId );

#undef ELASTICAPM_LOG_FMT_PROCESS_ID_MIN_WIDTH

#if ( ELASTICAPM_HAS_THREADS_01 != 0 )

    pid_t threadId = getCurrentThreadId();

#define ELASTICAPM_LOG_FMT_THREAD_ID_MIN_WIDTH 5

    textOutputStreamPrintf(
            txtOutStream
            , ":" ELASTICAPM_LOG_FMT_NUMBER_PAD_START( ELASTICAPM_LOG_FMT_THREAD_ID_MIN_WIDTH )
            , threadId );

#undef ELASTICAPM_LOG_FMT_THREAD_ID_MIN_WIDTH
#endif
}

static
void appendFileNameLineNumberPart(
        StringView filePath,
        UInt lineNumber,
        TextOutputStream* txtOutStream )
{
    // lifecycle.c:482

    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

    enum { tempBufferSize = 100 };
    char tempBuffer[tempBufferSize];
    StringView normalizedFileName = extractLastPartOfFilePathStringView( filePath );

    snprintf(
            tempBuffer,
            tempBufferSize,
            "%.*s:%u",
            (int) normalizedFileName.length,
            normalizedFileName.begin,
            lineNumber );

    #define ELASTICAPM_LOG_FMT_FILE_NAME_AND_LINE_NUMBER_MIN_WIDTH 20
    streamPrintf(
            txtOutStream
            , ELASTICAPM_LOG_FMT_STRING_PAD_END( ELASTICAPM_LOG_FMT_FILE_NAME_AND_LINE_NUMBER_MIN_WIDTH )
            , tempBuffer );
    #undef ELASTICAPM_LOG_FMT_FILE_NAME_AND_LINE_NUMBER_MIN_WIDTH
}

static void appendFunctionName( StringView funcName, TextOutputStream* txtOutStream )
{
    ELASTICAPM_ASSERT_VALID_PTR_TEXT_OUTPUT_STREAM( txtOutStream );

    #define ELASTICAPM_LOG_FMT_FUNCTION_NAME_MIN_WIDTH 30
    #define ELASTICAPM_LOG_FMT_FUNCTION_NAME_MAX_WIDTH 50
    streamPrintf(
            txtOutStream
            , "%" ELASTICAPM_LOG_FMT_PAD_END ELASTICAPM_PP_STRINGIZE( ELASTICAPM_LOG_FMT_FUNCTION_NAME_MIN_WIDTH ) ".*s"
            , (int) ( ELASTICAPM_MIN( funcName.length, ELASTICAPM_LOG_FMT_FUNCTION_NAME_MAX_WIDTH ) )
            , funcName.begin );
    #undef ELASTICAPM_LOG_FMT_FUNCTION_NAME_MAX_WIDTH
    #undef ELASTICAPM_LOG_FMT_FUNCTION_NAME_MIN_WIDTH
}

static
StringView buildCommonPrefix(
        LogLevel level,
        StringView filePath,
        UInt lineNumber,
        StringView funcName,
        char* buffer,
        size_t bufferSize )
{
    // 2020-02-15 21:51:32.123456+02:00 | ERROR    | 12345:67890 | lifecycle.c:482     | sendEventsToApmServer |<separator might have trailing space>

    TextOutputStream txtOutStream = makeTextOutputStream( buffer, bufferSize );
    // We don't need terminating '\0' after the prefix because we return it as StringView
    txtOutStream.autoTermZero = false;
    TextOutputStreamState txtOutStreamStateOnEntryStart;
    if ( ! textOutputStreamStartEntry( &txtOutStream, &txtOutStreamStateOnEntryStart ) )
        return ELASTICAPM_STRING_LITERAL_TO_VIEW( ELASTICAPM_TEXT_OUTPUT_STREAM_NOT_ENOUGH_SPACE_MARKER );

    appendTimestamp( &txtOutStream );
    appendSeparator( &txtOutStream );
    appendLevel( level, &txtOutStream );
    appendSeparator( &txtOutStream );
    appendProcessThreadIds( &txtOutStream );
    appendSeparator( &txtOutStream );
    appendFileNameLineNumberPart( filePath, lineNumber, &txtOutStream );
    appendSeparator( &txtOutStream );
    appendFunctionName( funcName, &txtOutStream );
    appendSeparator( &txtOutStream );

    textOutputStreamEndEntry( &txtOutStreamStateOnEntryStart, &txtOutStream );
    return textOutputStreamContentAsStringView( &txtOutStream );
}

static
StringView findEndOfLineSequence( StringView text )
{
    // The order in endOfLineSequences is important because we need to check longer sequences first
    StringView endOfLineSequences[] =
            {
                    ELASTICAPM_STRING_LITERAL_TO_VIEW( "\r\n" )
                    , ELASTICAPM_STRING_LITERAL_TO_VIEW( "\n" )
                    , ELASTICAPM_STRING_LITERAL_TO_VIEW( "\r" )
            };

    ELASTICAPM_FOR_EACH_INDEX( textPos, text.length )
    {
        ELASTICAPM_FOR_EACH_INDEX( eolSeqIndex, ELASTICAPM_STATIC_ARRAY_SIZE( endOfLineSequences ) )
        {
            if ( text.length - textPos < endOfLineSequences[ eolSeqIndex ].length ) continue;

            StringView eolSeqCandidate = makeStringView( &( text.begin[ textPos ] ), endOfLineSequences[ eolSeqIndex ].length );
            if ( areStringViewsEqual( eolSeqCandidate, endOfLineSequences[ eolSeqIndex ] ) )
            {
                return eolSeqCandidate;
            }
        }
    }

    return makeEmptyStringView();
}

static
StringView insertPrefixAtEachNewLine(
        Logger* logger
        , StringView sinkSpecificPrefix
        , StringView sinkSpecificEndOfLine
        , StringView commonPrefix
        , StringView oldMessage
        , size_t maxSizeForNewMessage
)
{
    ELASTICAPM_ASSERT_VALID_PTR( logger->auxMessageBuffer );
    ELASTICAPM_ASSERT( maxSizeForNewMessage <= loggerMessageBufferSize );

    StringView indent = ELASTICAPM_STRING_LITERAL_TO_VIEW( "    " ); // 4 spaces
    TextOutputStream txtOutStream = makeTextOutputStream( logger->auxMessageBuffer, maxSizeForNewMessage );
    // We don't need terminating '\0' after the prefix because we return it as StringView
    txtOutStream.autoTermZero = false;
    TextOutputStreamState txtOutStreamStateOnEntryStart;
    if ( ! textOutputStreamStartEntry( &txtOutStream, &txtOutStreamStateOnEntryStart ) )
        return ELASTICAPM_STRING_LITERAL_TO_VIEW( ELASTICAPM_TEXT_OUTPUT_STREAM_NOT_ENOUGH_SPACE_MARKER );

    const char* oldMessageEnd = stringViewEnd( oldMessage );
    StringView oldMessageLeft = oldMessage;
    for ( ;; )
    {
        StringView eolSeq = findEndOfLineSequence( oldMessageLeft );
        if ( isEmptyStringView( eolSeq ) ) break;

        streamStringView( makeStringViewFromBeginEnd( oldMessageLeft.begin, stringViewEnd( eolSeq ) ), &txtOutStream );
        streamStringView( sinkSpecificPrefix, &txtOutStream );
        streamStringView( commonPrefix, &txtOutStream );
        streamStringView( indent, &txtOutStream );
        oldMessageLeft = makeStringViewFromBeginEnd( stringViewEnd( eolSeq ), oldMessageEnd );
    }

    // If we didn't write anything to new message part then it means the old one is just one line
    // so there's no need to insert any prefixes
    if ( isEmptyStringView( textOutputStreamContentAsStringView( &txtOutStream ) ) ) return makeEmptyStringView();

    streamStringView( oldMessageLeft, &txtOutStream );

    textOutputStreamEndEntry( &txtOutStreamStateOnEntryStart, &txtOutStream );
    return textOutputStreamContentAsStringView( &txtOutStream );
}

static
String concatPrefixAndMsg(
        Logger* logger,
        StringView sinkSpecificPrefix,
        StringView sinkSpecificEndOfLine,
        StringView commonPrefix,
        String msgFmt,
        va_list msgArgs )
{
    ELASTICAPM_ASSERT_VALID_PTR( logger );
    ELASTICAPM_ASSERT_VALID_PTR( logger->messageBuffer );

    TextOutputStream txtOutStream = makeTextOutputStream( logger->messageBuffer, loggerMessageBufferSize );
    TextOutputStreamState txtOutStreamStateOnEntryStart;
    if ( ! textOutputStreamStartEntry( &txtOutStream, &txtOutStreamStateOnEntryStart ) )
        return ELASTICAPM_TEXT_OUTPUT_STREAM_NOT_ENOUGH_SPACE_MARKER;

    streamStringView( sinkSpecificPrefix, &txtOutStream );
    streamStringView( commonPrefix, &txtOutStream );
    const char* messagePartBegin = textOutputStreamGetFreeSpaceBegin( &txtOutStream );
    streamVPrintf( &txtOutStream, msgFmt, msgArgs );
    StringView messagePart = textOutputStreamViewFrom( &txtOutStream, messagePartBegin );
    size_t maxSizeForNewMessage = textOutputStreamGetFreeSpaceSize( &txtOutStream ) + messagePart.length - sinkSpecificEndOfLine.length;
    StringView newMessagePart = insertPrefixAtEachNewLine( logger, sinkSpecificPrefix, sinkSpecificEndOfLine, commonPrefix, messagePart, maxSizeForNewMessage );
    if ( ! isEmptyStringView( newMessagePart ) )
    {
        textOutputStreamGoBack( &txtOutStream, messagePart.length );
        streamStringView( newMessagePart, &txtOutStream );
    }
    streamStringView( sinkSpecificEndOfLine, &txtOutStream );
    return textOutputStreamEndEntry( &txtOutStreamStateOnEntryStart, &txtOutStream );
}

static
void writeToStderr( StringView prefix, String msgFmt, va_list msgArgs )
{
    fprintf( stderr, "%.*s", (int) prefix.length, prefix.begin );
    vfprintf( stderr, msgFmt, msgArgs );
    fprintf( stderr, "\n" );
    fflush( stderr );
}

//////////////////////////////////////////////////////////////////////////////
//
// syslog
//
#ifndef PHP_WIN32
static
int logLevelToSyslog( LogLevel level )
{
    switch ( level )
    {
        case logLevel_trace: // NOLINT(bugprone-branch-clone)
        case logLevel_debug:
            return LOG_DEBUG;

        case logLevel_info:
            return LOG_INFO;

        case logLevel_notice:
            return LOG_NOTICE;

        case logLevel_warning:
            return LOG_WARNING;

        case logLevel_error:
            return LOG_ERR;

        case logLevel_critical:
            return LOG_CRIT;

        default:
            return LOG_DEBUG;
    }
}

static
void writeToSyslog( Logger* logger, LogLevel level, StringView prefix, String msgFmt, va_list msgArgs )
{
    String fullText = concatPrefixAndMsg(
            logger,
            /* sinkSpecificPrefix: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "Elastic APM PHP Tracer | " ),
            /* sinkSpecificEndOfLine: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "" ),
            prefix,
            msgFmt,
            msgArgs );

    syslog( logLevelToSyslog( level ), "%s", fullText );
}
#endif
//
// syslog
//
//////////////////////////////////////////////////////////////////////////////

#ifdef PHP_WIN32
static
void writeToWinSysDebug( Logger* logger, StringView prefix, String msgFmt, va_list msgArgs )
{
    String fullText = concatPrefixAndMsg(
            logger,
            /* sinkSpecificPrefix: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "Elastic APM PHP Tracer | " ),
            /* sinkSpecificEndOfLine: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "\n" ),
            prefix,
            msgFmt,
            msgArgs );

    writeToWindowsSystemDebugger( fullText );
}
#endif

// TODO: Sergey Kleyman: Implement log to file
//static bool openAndAppendToFile( Logger* logger, StringView fullText )
//{
//#ifdef PHP_WIN32
//
//    FILE* file = fopen( logger->config.file, "a" );
//
//    fwrite(  );
//
//#else
//
//    // Use lower level system calls - "open" and "write" to get stronger guarantee:
//    //
//    //      O_APPEND
//    //              The file is opened in append mode.  Before each write(2), the
//    //              file offset is positioned at the end of the file, as if with
//    //              lseek(2).  The modification of the file offset and the write
//    //              operation are performed as a single atomic step.
//    //
//    // http://man7.org/linux/man-pages/man2/open.2.html
//    //
//    int file = open( logger->config.file, O_WRONLY | O_APPEND | O_CREAT );
//
//
//
//#endif
//
//    #ifdef PHP_WIN32
//    #else
//    write( , , fullLine );
//    #endif
//
//    if ( file )
//    {
//
//    }
//
//    finally:
//    if ( file != NULL )
//    {
//        #ifdef PHP_WIN32
//        fclose( file );
//        #else
//        close( file );
//        #endif
//        file = NULL;
//    }
//
//    failure:
//    logger->fileFailed = true;
//    goto finally;
//}
//
//static void writeToFile( Logger* logger, StringView prefix, String msgFmt, va_list msgArgs )
//{
//    if ( isNullOrEmtpyString( logger->config.file ) ) return;
//    if ( logger->fileFailed ) return;
//
//    openAndAppendToFile(
//            logger
//            , concatPrefixAndMsg( logger
//                                  , /* sinkPrefix: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "" )
//                                  , /* end-of-line: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "" )
//                                  , prefix
//                                  , msgFmt
//                                  , msgArgs ) );
//}

#ifdef ELASTICAPM_LOG_CUSTOM_SINK_FUNC

// Declare to avoid warnings
void ELASTICAPM_LOG_CUSTOM_SINK_FUNC( String fullText );

static
void buildFullTextAndWriteToCustomSink( Logger* logger, StringView prefix, String msgFmt, va_list msgArgs )
{
    String fullText = concatPrefixAndMsg( logger
                                          , /* prefix: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "" )
                                          , /* end-of-line: */ ELASTICAPM_STRING_LITERAL_TO_VIEW( "" )
                                          , prefix
                                          , msgFmt
                                          , msgArgs );

    ELASTICAPM_LOG_CUSTOM_SINK_FUNC( fullText );
}

#endif // #ifdef ELASTICAPM_LOG_CUSTOM_SINK_FUNC

void logWithLogger(
        Logger* logger,
        bool isForced,
        LogLevel level,
        StringView filePath,
        UInt lineNumber,
        StringView funcName,
        String msgPrintfFmt, ... )
{
    if ( logger->reentrancyDepth + 1 > maxLoggerReentrancyDepth ) return;
    ++logger->reentrancyDepth;
    ELASTICAPM_ASSERT( logger->reentrancyDepth > 0 );

    ELASTICAPM_ASSERT_VALID_PTR( logger );

    enum { commonPrefixBufferSize = 200 + ELASTICAPM_TEXT_OUTPUT_STREAM_RESERVED_SPACE_SIZE };
    char commonPrefixBuffer[ commonPrefixBufferSize ];

    StringView commonPrefix = buildCommonPrefix( level, filePath, lineNumber, funcName, commonPrefixBuffer, commonPrefixBufferSize );

    if ( isForced || logger->config.levelPerSinkType[ logSink_stderr ] >= level )
    {
        // create a separate copy of va_list because functions using it (such as fprintf, etc.) modify it
        va_list msgArgs;
        va_start( msgArgs, msgPrintfFmt );
        writeToStderr( commonPrefix, msgPrintfFmt, msgArgs );
        va_end( msgArgs );
        fflush( stderr );
    }

    #ifndef PHP_WIN32
    if ( isForced || logger->config.levelPerSinkType[ logSink_syslog ] >= level )
    {
        // create a separate copy of va_list because functions using it (such as fprintf, etc.) modify it
        va_list msgArgs;
        va_start( msgArgs, msgPrintfFmt );
        writeToSyslog( logger, level, commonPrefix, msgPrintfFmt, msgArgs );
        va_end( msgArgs );
    }
    #endif

    #ifdef PHP_WIN32
    if ( isForced || logger->config.levelPerSinkType[ logSink_winSysDebug ] >= level )
    {
        // create a separate copy of va_list because functions using it (such as fprintf, etc.) modify it
        va_list msgArgs;
        va_start( msgArgs, msgPrintfFmt );
        writeToWinSysDebug( logger, commonPrefix, msgPrintfFmt, msgArgs );
        va_end( msgArgs );
    }
    #endif

    // TODO: Sergey Kleyman: Implement log to file
//    if ( isForced || logger->config.levelPerSinkType[ logSink_file ] >= level )
//    {
//        // create a separate copy of va_list because functions using it (such as fprintf, etc.) modify it
//        va_list msgArgs;
//        va_start( msgArgs, msgPrintfFmt );
//        writeToFile( logger, commonPrefix, msgPrintfFmt, msgArgs );
//        va_end( msgArgs );
//    }

#ifdef ELASTICAPM_LOG_CUSTOM_SINK_FUNC
    va_list msgArgs;
    va_start( msgArgs, msgPrintfFmt );
    buildFullTextAndWriteToCustomSink( logger, commonPrefix, msgPrintfFmt, msgArgs );
    va_end( msgArgs );
#endif

    ELASTICAPM_ASSERT( logger->reentrancyDepth > 0 );
    --logger->reentrancyDepth;
}

static
LogLevel findMaxLevel( const LogLevel* levelsArray, size_t levelsArraySize, LogLevel minLevel )
{
    int max = minLevel;
    ELASTICAPM_FOR_EACH_INDEX( i, levelsArraySize ) if ( levelsArray[ i ] > max ) max = levelsArray[ i ];
    return max;
}

LogLevel calcMaxEnabledLogLevel( LogLevel levelPerSinkType[ numberOfLogSinkTypes ] )
{
    return findMaxLevel( levelPerSinkType, numberOfLogSinkTypes, /* minValue */ logLevel_not_set );
}

static
void setLoggerConfigToDefaults( LoggerConfig* config )
{
    ELASTICAPM_FOR_EACH_INDEX( sinkTypeIndex, numberOfLogSinkTypes )
        config->levelPerSinkType[ sinkTypeIndex ] = defaultLogLevelPerSinkType[ sinkTypeIndex ];

    config->file = NULL;
}

static
LogLevel deriveLevelForSink( LogLevel levelForSink, LogLevel generalLevel, LogLevel defaultLevelForSink )
{
    if ( levelForSink != logLevel_not_set ) return levelForSink;
    if ( generalLevel != logLevel_not_set ) return generalLevel;
    return defaultLevelForSink;
}

static void deriveLoggerConfig( const LoggerConfig* newConfig, LogLevel generalLevel, LoggerConfig* derivedNewConfig )
{
    LoggerConfig defaultConfig;

    setLoggerConfigToDefaults( &defaultConfig );

    ELASTICAPM_FOR_EACH_LOG_SINK_TYPE( logSinkType )
    {
        derivedNewConfig->levelPerSinkType[ logSinkType ] = deriveLevelForSink(
                newConfig->levelPerSinkType[ logSinkType ], generalLevel, defaultConfig.levelPerSinkType[ logSinkType ] );
    }
}

static bool areEqualLoggerConfigs( const LoggerConfig* config1, const LoggerConfig* config2 )
{
    ELASTICAPM_FOR_EACH_LOG_SINK_TYPE( logSinkType )
        if ( config1->levelPerSinkType[ logSinkType ] != config2->levelPerSinkType[ logSinkType ] ) return false;

    if ( ! areEqualNullableStrings( config1->file, config2->file ) ) return false;

    return true;
}

static void logConfigChangeInLevel( String dbgLevelDesc, LogLevel oldLevel, LogLevel newLevel )
{
    char txtOutStreamBuf[ ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE ];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );

    if ( oldLevel == newLevel )
        ELASTICAPM_LOG_DEBUG( "%s did not change. Its value is still %s."
                            , dbgLevelDesc
                            , streamLogLevel( oldLevel, &txtOutStream ) );
    else
        ELASTICAPM_LOG_DEBUG( "%s changed from %s to %s."
                            , dbgLevelDesc
                            , streamLogLevel( oldLevel, &txtOutStream )
                            , streamLogLevel( newLevel, &txtOutStream ) );
}

String logSinkTypeName[ numberOfLogSinkTypes ] =
{
    [ logSink_stderr ] = "Stderr",

    #ifndef PHP_WIN32
    [ logSink_syslog ] = "Syslog",
    #endif

    #ifdef PHP_WIN32
    [ logSink_winSysDebug ] = "Windows system debugger",
    #endif

    [ logSink_file ] = "File"
};

LogLevel defaultLogLevelPerSinkType[ numberOfLogSinkTypes ] =
{
    [ logSink_stderr ] = logLevel_critical,

    #ifndef PHP_WIN32
    [ logSink_syslog ] = logLevel_error,
    #endif

    #ifdef PHP_WIN32
    [ logSink_winSysDebug ] = logLevel_debug,
    #endif

    [ logSink_file ] = logLevel_info
};

static void logConfigChange(
        const LoggerConfig* oldConfig,
        LogLevel oldMaxEnabledLevel,
        const LoggerConfig* newConfig,
        LogLevel newMaxEnabledLevel )
{
    char txtOutStreamBuf[ ELASTICAPM_TEXT_OUTPUT_STREAM_ON_STACK_BUFFER_SIZE ];
    TextOutputStream txtOutStream = ELASTICAPM_TEXT_OUTPUT_STREAM_FROM_STATIC_BUFFER( txtOutStreamBuf );

    ELASTICAPM_FOR_EACH_LOG_SINK_TYPE( logSinkType )
    {
        textOutputStreamRewind( &txtOutStream );
        logConfigChangeInLevel( streamPrintf( &txtOutStream, "Log level for sink %s", logSinkTypeName[ logSinkType ] )
                                , oldConfig->levelPerSinkType[ logSinkType ]
                                , newConfig->levelPerSinkType[ logSinkType ] );
    }

    textOutputStreamRewind( &txtOutStream );
    logConfigChangeInLevel( "Max enabled log level", oldMaxEnabledLevel, newMaxEnabledLevel );

    textOutputStreamRewind( &txtOutStream );
    if ( areEqualNullableStrings( oldConfig->file, newConfig->file ) )
        ELASTICAPM_LOG_DEBUG( "Path for file logging sink did not change. Its value is still %s."
                              , streamUserString( newConfig->file, &txtOutStream ) );
    else
        ELASTICAPM_LOG_DEBUG( "Path for file logging sink changed from %s to %s."
                            , streamUserString( oldConfig->file, &txtOutStream )
                            , streamUserString( newConfig->file, &txtOutStream ) );
}

void reconfigureLogger( Logger* logger, const LoggerConfig* newConfig, LogLevel generalLevel )
{
    LoggerConfig derivedNewConfig = *newConfig;
    deriveLoggerConfig( newConfig, generalLevel, &derivedNewConfig );

    if ( areEqualLoggerConfigs( &logger->config, &derivedNewConfig ) )
    {
        ELASTICAPM_LOG_DEBUG( "Logger configuration did not change" );
        return;
    }

    const LoggerConfig oldConfig = logger->config;
    const LogLevel oldMaxEnabledLevel = logger->maxEnabledLevel;
    logger->config = derivedNewConfig;
    logger->maxEnabledLevel = calcMaxEnabledLogLevel( logger->config.levelPerSinkType );
    logConfigChange( &oldConfig, oldMaxEnabledLevel, &logger->config, logger->maxEnabledLevel );
}

ResultCode constructLogger( Logger* logger )
{
    ELASTICAPM_ASSERT_VALID_PTR( logger );

    ResultCode resultCode;

    setLoggerConfigToDefaults( &( logger->config ) );
    logger->maxEnabledLevel = calcMaxEnabledLogLevel( logger->config.levelPerSinkType );
    logger->messageBuffer = NULL;
    logger->auxMessageBuffer = NULL;
    logger->fileFailed = false;

    ELASTICAPM_PEMALLOC_STRING_IF_FAILED_GOTO( loggerMessageBufferSize, logger->messageBuffer );
    ELASTICAPM_PEMALLOC_STRING_IF_FAILED_GOTO( loggerMessageBufferSize, logger->auxMessageBuffer );

    resultCode = resultSuccess;

    finally:
    return resultCode;

    failure:
    destructLogger( logger );
    goto finally;
}

void destructLogger( Logger* logger )
{
    ELASTICAPM_ASSERT_VALID_PTR( logger );

    ELASTICAPM_PEFREE_STRING_AND_SET_TO_NULL( loggerMessageBufferSize, logger->auxMessageBuffer );
    ELASTICAPM_PEFREE_STRING_AND_SET_TO_NULL( loggerMessageBufferSize, logger->messageBuffer );
}

Logger* getGlobalLogger()
{
    return &getGlobalTracer()->logger;
}
