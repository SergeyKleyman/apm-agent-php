<?php

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

declare(strict_types=1);

error_reporting(E_ALL | E_STRICT);

function elasticApmOnAssertFailure(string $condDesc, string $expr, $actual, $expected)
{
    if ($expected === $actual) {
        return;
    }

    $indent = "\t\t\t\t\t\t";
    echo "========================================\n";
    echo "====================\n";
    echo "===\n";
    echo "\n";
    echo "Expected and actual values for:\n";
    echo "\n$indent";
    echo "$expr\n";
    echo "\n";
    echo "are not $condDesc.\n";
    echo "\n";
    echo "Expected value:\n";
    echo "\n$indent";
    var_dump($expected);
    echo "\n";
    echo "Actual value:\n";
    echo "\n$indent";
    var_dump($actual);
    echo "\n";
    echo "===\n";
    echo "====================\n";
    echo "========================================\n";
    die();
}

function elasticApmAssertSame(string $expr, $actual, $expected): void
{
    if ($expected === $actual) {
        return;
    }

    elasticApmOnAssertFailure("the same", $expr, $actual, $expected);
}

function elasticApmAssertEqual(string $expr, $actual, $expected): void
{
    if ($expected == $actual) {
        return;
    }

    elasticApmOnAssertFailure("equal", $expr, $actual, $expected);
}

function elasticApmIsOsWindows(): bool
{
    return strtoupper(substr(PHP_OS, 0, 3)) === 'WIN';
}

function elasticApmAssertBoolOptionValueSetViaIni(string $optName, bool $expectedVal): void
{
    $iniName = 'elastic_apm.' . $optName;
    elasticApmAssertEqual("ini_get('$iniName')", ini_get($iniName), $expectedVal);

    /** @noinspection PhpUndefinedFunctionInspection */
    elasticApmAssertSame(
        "elastic_apm_get_config_option_by_name('$optName')",
        elastic_apm_get_config_option_by_name($optName),
        $expectedVal
    );
}

function elasticApmAssertBoolOptionValueSetViaEnvVar(string $optName, string $rawVal, bool $expectedVal): void
{
    $envVarName = 'ELASTIC_APM_' . strtoupper($optName);
    elasticApmAssertSame("getenv('$envVarName')", getenv($envVarName), $rawVal);

    /** @noinspection PhpUndefinedFunctionInspection */
    elasticApmAssertSame(
        "elastic_apm_get_config_option_by_name('$optName')",
        elastic_apm_get_config_option_by_name($optName),
        $expectedVal
    );
}

elasticApmAssertSame("getenv('ELASTIC_APM_LOG_LEVEL_STDERR')", getenv('ELASTIC_APM_LOG_LEVEL_STDERR'), 'CRITICAL');
/** @noinspection PhpUndefinedFunctionInspection, PhpUndefinedConstantInspection */
elasticApmAssertSame(
    "elastic_apm_get_config_option_by_name('log_level_stderr')",
    elastic_apm_get_config_option_by_name('log_level_stderr'),
    ELASTIC_APM_LOG_LEVEL_CRITICAL
);
