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

namespace ElasticApmTests\Util;

use ElasticApmTests\Util\Deserialization\DeserializationUtil;

class SpanCompositeDto
{
    use AssertValidTrait;

    /** @var string */
    public $compressionStrategy;

    /** @var int */
    public $count;

    /** @var float */
    public $durationsSum;

    /**
     * @param mixed $value
     *
     * @return self
     */
    public static function deserialize($value): self
    {
        $result = new self();
        DeserializationUtil::deserializeKeyValuePairs(
            DeserializationUtil::assertDecodedJsonMap($value),
            function ($key, $value) use ($result): bool {
                switch ($key) {
                    case 'compression_strategy':
                        $result->compressionStrategy = self::assertValidNonNullableString($value);
                        return true;
                    case 'count':
                        $result->count = self::assertValidCount($value, /* minValue: */ 2);
                        return true;
                    case 'sum':
                        $result->durationsSum = self::assertValidDuration($value);
                        return true;
                    default:
                        return false;
                }
            }
        );

        $result->assertValid();
        return $result;
    }

    public function assertValid(): void
    {
        self::assertValidString($this->compressionStrategy, /* isNullable: */ false);
        self::assertValidCount($this->count, /* minValue: */ 2);
        self::assertValidDuration($this->durationsSum);
    }
}
