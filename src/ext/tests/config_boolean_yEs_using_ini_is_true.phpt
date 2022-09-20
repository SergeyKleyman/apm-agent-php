--TEST--
Boolean configuration option value 'yEs' (in this case using ini file) should be interpreted as true and it should be case insensitive
--SKIPIF--
<?php if ( ! extension_loaded( 'elastic_apm' ) ) die( 'skip'.'Extension elastic_apm must be installed' ); ?>
--ENV--
ELASTIC_APM_LOG_LEVEL_STDERR=CRITICAL
--INI--
elastic_apm.enabled=yEs
elastic_apm.process_ast_to_instrument=yEs
--FILE--
<?php
declare(strict_types=1);
require __DIR__ . '/../tests_util/tests_util.php';

$expectedVal = true;

elasticApmAssertBoolOptionValueSetViaIni('enabled', $expectedVal);
elasticApmAssertSame("elastic_apm_is_enabled()", elastic_apm_is_enabled(), $expectedVal);

elasticApmAssertBoolOptionValueSetViaIni('process_ast_to_instrument', $expectedVal);

echo 'Test completed'
?>
--EXPECT--
Test completed
