/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#ifndef __SINK_H__
#define __SINK_H__

#include <stdio.h>
#include <erl_nif.h>
#include <membrane/membrane.h>
#include <shout/shout.h>

typedef struct _SinkHandle SinkHandle;

struct _SinkHandle
{
  shout_t        *shout;               // libshout handle
  ErlNifTid       thread_id;           // Capture thread ID
  int             thread_started;      // For storing information whether thread was started
  ErlNifMutex    *lock;                // Mutex for synchronizing access to the handle
};

#endif
