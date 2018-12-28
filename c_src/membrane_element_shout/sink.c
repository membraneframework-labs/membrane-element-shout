/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#include "sink.h"

#include <stdlib.h>
#include <string.h>

#define RINGBUFFER_SIZE 8

void handle_destroy_state(UnifexEnv *env, UnifexNifState *state) {
  if (state->shout != NULL) {
    MEMBRANE_DEBUG(env, "Destroying Sink %p", state->shout);
    shout_close(state->shout);
  }

  // TODO handle case when we get garbage collected while still running

  if (state->ringbuffer != NULL) {
    membrane_ringbuffer_destroy(state->ringbuffer);
  }

  if (state->lock != NULL) {
    unifex_mutex_destroy(state->lock);
  }
}

int on_load(UnifexEnv *env, void **priv_data) {
  UNIFEX_UNUSED(env);
  UNIFEX_UNUSED(priv_data);

  shout_init();
  return 0;
}

void on_unload(UnifexEnv *env, void *priv_data) {
  UNIFEX_UNUSED(env);
  UNIFEX_UNUSED(priv_data);

  shout_shutdown();
}

/**
 * Main function of the sending thread.
 */
static void *thread_func(void *arg) {
  UnifexNifState *state = (UnifexNifState *)arg;
  UnifexEnv *msg_env = unifex_alloc_env();
  int res;
  int underrun_cnt = 0;
  // Keep the handle so it does not get garbage collected
  unifex_keep_state(NULL, state);

  // Indicate that we are running
  unifex_mutex_lock(state->lock);
  state->thread_running = 1;
  unifex_mutex_unlock(state->lock);

  MEMBRANE_THREADED_DEBUG(msg_env, "Send: Starting");
  unifex_clear_env(msg_env);

  res = send_native_demand(msg_env, state->self_pid, UNIFEX_SEND_THREADED,
                           RINGBUFFER_SIZE);
  unifex_clear_env(msg_env);

  if (!res) {
    goto thread_cleanup;
  }

  MEMBRANE_THREADED_DEBUG(msg_env, "Connecting to the server");
  unifex_clear_env(msg_env);

  if (shout_open(state->shout) != SHOUTERR_SUCCESS) {
    MEMBRANE_THREADED_WARN(msg_env, "Failed to connect to the server: %s",
                           shout_get_error(state->shout));
    unifex_clear_env(msg_env);
    send_native_error_shout_open(msg_env, state->self_pid, UNIFEX_SEND_THREADED,
                                 (char *)shout_get_error(state->shout));
    unifex_clear_env(msg_env);
    goto thread_cleanup;
  }

  MEMBRANE_THREADED_INFO(msg_env, "Connected to icecast");
  unifex_clear_env(msg_env);

  while (1) {
    // Check if we are exiting
    unifex_mutex_lock(state->lock);
    if (state->thread_started == 0) {
      MEMBRANE_THREADED_DEBUG(msg_env, "Send: Stop requested, exiting loop");
      unifex_clear_env(msg_env);
      unifex_mutex_unlock(state->lock);
      goto thread_cleanup;
    }
    unifex_mutex_unlock(state->lock);

    size_t ready_size =
        membrane_ringbuffer_get_read_available(state->ringbuffer);
    MEMBRANE_THREADED_DEBUG(msg_env, "Send: Ready buffers: %zd", ready_size);
    unifex_clear_env(msg_env);

    RingBufferItem item;
    size_t read_cnt = membrane_ringbuffer_read(state->ringbuffer, &item, 1);

    if (read_cnt == 1) {
      res =
          send_native_demand(msg_env, state->self_pid, UNIFEX_SEND_THREADED, 1);
      unifex_clear_env(msg_env);
      if (!res) {
        goto thread_cleanup;
      }

      if (underrun_cnt > 0) {
        MEMBRANE_THREADED_WARN(msg_env, "Send: Underrun: %d frames",
                               underrun_cnt);
        unifex_clear_env(msg_env);
        underrun_cnt = 0;
      }
      // Send
      MEMBRANE_THREADED_DEBUG(msg_env, "Send: Pushing payload");
      unifex_clear_env(msg_env);
      res = shout_send(state->shout, item.data, item.size);
      unifex_free(item.data);

      if (res != SHOUTERR_SUCCESS) {
        MEMBRANE_THREADED_WARN(msg_env, "Send: error %s",
                               shout_get_error(state->shout));
        unifex_clear_env(msg_env);
        send_native_error_shout_send(msg_env, state->self_pid,
                                     UNIFEX_SEND_THREADED,
                                     (char *)shout_get_error(state->shout));
        unifex_clear_env(msg_env);
        goto thread_cleanup;
      }
    } else {
      underrun_cnt++;
      // Constant sponsored by Institute of Constants Out Of Nowhere
      // a.k.a. Mateusz Front
      nanosleep((const struct timespec[]){{0, 24000000L}}, NULL);
    }

    // Sleep for necessary amount of time to keep frames in sync with the clock
    MEMBRANE_THREADED_DEBUG(msg_env, "Send: Synchronizing clock");
    unifex_clear_env(msg_env);
    shout_sync(state->shout);
  }

thread_cleanup:

  if (underrun_cnt > 0) {
    MEMBRANE_THREADED_WARN(msg_env, "Send: Underrun: %d frames", underrun_cnt);
    unifex_clear_env(msg_env);
  }

  MEMBRANE_THREADED_DEBUG(msg_env, "Send: Stopping");
  unifex_clear_env(msg_env);

  // Close shout
  shout_close(state->shout);

  // Clean ringbuffer
  membrane_ringbuffer_cleanup(state->ringbuffer);

  // Indicate that we are not running
  unifex_mutex_lock(state->lock);
  state->thread_running = 0;
  unifex_mutex_unlock(state->lock);

  // Release the handle so it can be garbage collected
  unifex_release_state(NULL, state);

  MEMBRANE_THREADED_DEBUG(msg_env, "Send: Stopped");
  unifex_free_env(msg_env);
  return NULL;
}

/**
 * Initializes shout sink and creates a state returned to Erlang VM.
 */
UNIFEX_TERM create(UnifexEnv *env, char *host, unsigned int port,
                   char *password, char *mount) {
  shout_t *shout;

  if (!(shout = shout_new())) {
    MEMBRANE_WARN(env, "Could not allocate shout_t");
    return 1;
  }

  if (shout_set_host(shout, host) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting hostname: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_host");
  }

  if (shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting protocol: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_protocol");
  }

  if (shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting port: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_port");
  }

  if (shout_set_password(shout, password) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting password: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_password");
  }
  if (shout_set_mount(shout, mount) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting mount: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_mount");
  }

  if (shout_set_user(shout, "source") != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting user: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_user");
  }

  if (shout_set_format(shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting user: %s", shout_get_error(shout));
    return create_result_error_internal(env, "shout_set_format");
  }

  UnifexNifState *state = unifex_alloc_state(env);
  MEMBRANE_DEBUG(env, "Creating SinkHandle %p", state);
  state->shout = shout;
  state->thread_started = 0;
  state->thread_running = 0;
  state->lock = unifex_mutex_create("shout_sink_lock");
  state->ringbuffer =
      membrane_ringbuffer_new(RINGBUFFER_SIZE, sizeof(RingBufferItem));
  unifex_self(env, &state->self_pid);

  UNIFEX_TERM result = create_result_ok(env, state);
  unifex_release_state(env, state);

  return result;
}

/**
 * Starts a thread sending data to a server.
 */
UNIFEX_TERM start(UnifexEnv *env, UnifexNifState *state) {
  unifex_mutex_lock(state->lock);

  if (state->thread_started != 0 || state->thread_running != 0) {
    unifex_mutex_unlock(state->lock);
    return start_result_error_already_started(env);
  }

  // Start the sending thread
  MEMBRANE_DEBUG(env, "Starting sending thread for SinkState %p",
                 (void *)state);

  state->thread_started = 1;
  unifex_mutex_unlock(state->lock);

  int error = unifex_thread_create("membrane_element_shout_sink",
                                   &(state->thread_id), thread_func, state);
  if (error != 0) {
    MEMBRANE_WARN(env, "Failed to create thread: %d", error);
    return start_result_error_internal(env, "thread_create");
  }

  return start_result_ok(env);
}

/**
 * Stops sending thread.
 */
UNIFEX_TERM stop(UnifexEnv *env, UnifexNifState *state) {
  UNIFEX_TERM result;
  unifex_mutex_lock(state->lock);
  if (state->thread_started == 0) {
    result = stop_result_error_not_started(env);
    goto stop_exit;
  }
  // Stop the sending thread
  MEMBRANE_DEBUG(env, "Waiting for sending thread for SinkState %p to stop",
                 (void *)state);

  void *thread_result;

  state->thread_started = 0;
  unifex_mutex_unlock(state->lock);
  unifex_thread_join(state->thread_id, &thread_result);
  unifex_mutex_lock(state->lock);

  MEMBRANE_DEBUG(env, "Sending thread for SinkHandle %p has stopped",
                 (void *)state);

  result = stop_result_ok(env);

stop_exit:
  unifex_mutex_unlock(state->lock);
  return result;
}

/**
 * Writes data to the shout sink.
 */
UNIFEX_TERM write(UnifexEnv *env, UnifexPayload *payload,
                  UnifexNifState *state) {
  MEMBRANE_DEBUG(env, "Writing data to SinkHandle %p", (void *)state);
  RingBufferItem item;
  item.size = payload->size;
  item.data = unifex_alloc(item.size);
  memcpy(item.data, payload->data, item.size);
  if (!membrane_ringbuffer_write(state->ringbuffer, &item, 1)) {
    return write_result_error_internal(env, "overrun");
  } else {
    MEMBRANE_DEBUG(env, "Wrote data to SinkHandle %p", (void *)state);
    return write_result_ok(env);
  }
}
