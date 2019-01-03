/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#pragma once

#include <membrane/membrane.h>
#include <membrane_ringbuffer/ringbuffer.h>
#define MEMBRANE_LOG_TAG "Elixir.Membrane.Element.Shout.Sink.Native"
#include <membrane/log.h>
#include <shout/shout.h>

typedef struct SinkState {
  shout_t *shout;      // libshout handle
  UnifexTid thread_id; // Capture thread ID
  int thread_started;  // Flag indicating whether thread is supposed to be
                       // started
  int thread_running;  // Flag indicating whether thread is actually running
  UnifexMutex *lock;   // Mutex for synchronizing access to the handle
  UnifexMutex *cond_lock; // Mutex used to synchronize signals from condition variable
  UnifexCond *cond;    // Condition variable used to wait for buffers on underrun
  MembraneRingBuffer *ringbuffer; // Ringbuffer for pending payload
  UnifexPid self_pid;             // PID of the process for receiving messages
} UnifexNifState;

typedef struct RingBufferItem {
  size_t size;         // length of the data
  unsigned char *data; // data itself
} RingBufferItem;

#include "_generated/sink.h"
