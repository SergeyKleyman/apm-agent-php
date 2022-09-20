--TEST--
Boolean configuration option value 'FaLSe' (in this case using environment variable) should be interpreted as false and it should be case insensitive
--SKIPIF--
<?php if ( ! extension_loaded( 'elastic_apm' ) ) die( 'skip'.'Extension elastic_apm must be installed' ); ?>
--ENV--
ELASTIC_APM_LOG_LEVEL_STDERR=CRITICAL
ELASTIC_APM_ENABLED=FaLSe
ELASTIC_APM_PROCESS_AST_TO_INSTRUMENT=FaLSe
--FILE--
<?php
declare(strict_types=1);
require __DIR__ . '/../tests_util/tests_util.php';

$rawVal='FaLSe';
$expectedVal = false;

elasticApmAssertBoolOptionValueSetViaEnvVar('enabled', $rawVal, $expectedVal);
elasticApmAssertSame('elastic_apm_is_enabled()', elastic_apm_is_enabled(), $expectedVal);

elasticApmAssertBoolOptionValueSetViaEnvVar('process_ast_to_instrument', $rawVal, $expectedVal);

echo 'Test completed'
?>
--EXPECT--
Test completed
