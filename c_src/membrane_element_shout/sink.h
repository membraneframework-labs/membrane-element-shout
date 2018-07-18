/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#ifndef __SINK_H__
#define __SINK_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <erl_nif.h>
#include <membrane/membrane.h>
#include <membrane_ringbuffer/ringbuffer.h>
#define MEMBRANE_LOG_TAG "Membrane.Element.Shout.Sink.Native"
#include <membrane/log.h>
#include <shout/shout.h>

typedef struct _SinkHandle SinkHandle;

struct _SinkHandle
{
  shout_t             *shout;              // libshout handle
  ErlNifTid           thread_id;           // Capture thread ID
  int                 thread_started;      // Flag indicating whether thread is supposed to be started
  int                 thread_running;      // Flag indicating whether thread is actually running
  ErlNifMutex        *lock;                // Mutex for synchronizing access to the handle
  MembraneRingBuffer *ringbuffer;          // Ringbuffer for pending payload
  ErlNifPid          *self_pid;            // PID of the process for receiving messages
};

typedef struct RingBufferItem {
  size_t size; // length of the data
  unsigned char *data; // data itself
} RingBufferItem;

#endif
