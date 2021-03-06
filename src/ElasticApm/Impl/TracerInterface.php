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

namespace Elastic\Apm\Impl;

use Closure;
use Elastic\Apm\CustomErrorData;
use Elastic\Apm\DistributedTracingData;
use Elastic\Apm\ElasticApm;
use Elastic\Apm\TransactionInterface;
use Throwable;

interface TracerInterface
{
    /**
     * Begins a new transaction and sets the new transaction as the current transaction for this tracer.
     *
     * @param string      $name      New transaction's name
     * @param string      $type      New transaction's type
     * @param float|null  $timestamp Start time of the new transaction
     * @param string|null $serializedDistTracingData
     *
     * @return TransactionInterface New transaction
     */
    public function beginCurrentTransaction(
        string $name,
        string $type,
        ?float $timestamp = null,
        ?string $serializedDistTracingData = null
    ): TransactionInterface;

    /**
     * Begins a new transaction, sets as the current transaction for this tracer,
     * runs the provided callback as the new transaction and automatically ends the new transaction.
     *
     * @param string      $name      New transaction's name
     * @param string      $type      New transaction's type
     * @param Closure     $callback  Callback to execute as the new transaction
     * @param float|null  $timestamp Start time of the new transaction
     * @param string|null $serializedDistTracingData
     *
     * @return mixed The return value of $callback
     * @template        T
     */
    public function captureCurrentTransaction(
        string $name,
        string $type,
        Closure $callback,
        ?float $timestamp = null,
        ?string $serializedDistTracingData = null
    );

    /**
     * @see ElasticApm::getCurrentTransaction()
     */
    public function getCurrentTransaction(): TransactionInterface;

    /**
     * Begins a new transaction.
     *
     * @param string      $name      New transaction's name
     * @param string      $type      New transaction's type
     * @param float|null  $timestamp Start time of the new transaction
     * @param string|null $serializedDistTracingData
     *
     * @return TransactionInterface New transaction
     */
    public function beginTransaction(
        string $name,
        string $type,
        ?float $timestamp = null,
        ?string $serializedDistTracingData = null
    ): TransactionInterface;

    /**
     * Begins a new transaction, runs the provided callback as the new transaction
     * and automatically ends the new transaction.
     *
     * @param string      $name      New transaction's name
     * @param string      $type      New transaction's type
     * @param Closure     $callback  Callback to execute as the new transaction
     * @param float|null  $timestamp Start time of the new transaction
     * @param string|null $serializedDistTracingData
     *
     * @return mixed The return value of $callback
     * @template        T
     */
    public function captureTransaction(
        string $name,
        string $type,
        Closure $callback,
        ?float $timestamp = null,
        ?string $serializedDistTracingData = null
    );

    /**
     * Reports error event based on the given
     *
     * @param Throwable $throwable
     *
     * @return string|null
     *
     * @see ElasticApm::createErrorFromThrowable
     */
    public function createErrorFromThrowable(Throwable $throwable): ?string;

    /**
     * Reports error event based on the given
     *
     * @param CustomErrorData $customErrorData
     *
     * @return string|null
     *
     * @see ElasticApm::createCustomError
     */
    public function createCustomError(CustomErrorData $customErrorData): ?string;

    /**
     * Returns true if this Tracer is a no-op (for example because Elastic APM is disabled)
     */
    public function isNoop(): bool;

    /**
     * @see ElasticApm::pauseRecording()
     */
    public function pauseRecording(): void;

    /**
     * @see ElasticApm::resumeRecording()
     */
    public function resumeRecording(): void;

    /**
     * Returns true if this Tracer has recording on i.e., not paused
     */
    public function isRecording(): bool;

    /**
     * @param string|null $ephemeralId
     *
     * @see ServiceAgentData::ephemeralId
     */
    public function setAgentEphemeralId(?string $ephemeralId): void;

    /**
     * Returns distributed tracing data for the current span/transaction
     */
    public function getSerializedCurrentDistributedTracingData(): string;
}
