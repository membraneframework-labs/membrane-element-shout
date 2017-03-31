/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#include "sink.h"

#define MEMBRANE_LOG_TAG "Membrane.Element.Shout.Sink.Native"


ErlNifResourceType *RES_SINK_HANDLE_TYPE;

void res_sink_handle_destructor(ErlNifEnv *env, void *value) {
  SinkHandle *sink_handle = (SinkHandle *) value;

  if(sink_handle->shout != NULL) {
    MEMBRANE_DEBUG("Destroying Sink %p", sink_handle->shout);
    shout_close((shout_t *) sink_handle->shout);
  }

  if(sink_handle->ringbuffer != NULL) {
    membrane_ringbuffer_destroy(sink_handle->ringbuffer);
  }

  if(sink_handle->self_pid != NULL) {
    enif_free(sink_handle->self_pid);
  }

  if(sink_handle->lock != NULL) {
    enif_mutex_destroy(sink_handle->lock);
  }
}


int load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  RES_SINK_HANDLE_TYPE =
    enif_open_resource_type(env, NULL, "SinkHandle", res_sink_handle_destructor, flags, NULL);

  shout_init();

  return 0;
}


void unload(ErlNifEnv *env, void *priv_data) {
  shout_shutdown();
}


/**
 * Main function of the sending thread.
 */
static void *thread_func(void *args) {
  SinkHandle *sink_handle = (SinkHandle *) args;
  ErlNifEnv *msg_env;

  // Indicate that we are running
  enif_mutex_lock(sink_handle->lock);
  sink_handle->thread_running = 1;
  enif_mutex_unlock(sink_handle->lock);

  MEMBRANE_DEBUG("Send: Starting");

  // Keep the handle so it does not get garbage collected
  enif_keep_resource(sink_handle);

  MEMBRANE_DEBUG("Connecting to the server");
  if(shout_open(sink_handle->shout) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Failed to connect to the server: %s", shout_get_error(sink_handle->shout));
    shout_close(sink_handle->shout);

    // TODO send error upstream

    // Release the handle so it can be garbage collected
    enif_release_resource(sink_handle);

    return NULL;
  }

  MEMBRANE_DEBUG("Connected to the server");

  while(1) {
    // Check if we are exiting
    enif_mutex_lock(sink_handle->lock);
    if(sink_handle->thread_started == 0) {
      MEMBRANE_DEBUG("Send: Stop requested, exiting loop");
      enif_mutex_unlock(sink_handle->lock);
      break;
    }
    enif_mutex_unlock(sink_handle->lock);

    size_t capacity = membrane_ringbuffer_get_capacity(sink_handle->ringbuffer);
    size_t available = membrane_ringbuffer_get_available(sink_handle->ringbuffer);
    MEMBRANE_DEBUG("Send: Ready buffers: %zd", capacity - available);

    MembraneRingBufferItem* item = membrane_ringbuffer_pull(sink_handle->ringbuffer);
    if(item == NULL) {
      MEMBRANE_DEBUG("Send: Underrun, exiting loop");

      // Send error to the parent element
      msg_env = enif_alloc_env();
      if(!enif_send(NULL, sink_handle->self_pid, msg_env, enif_make_atom(msg_env, "membrane_send_underrun"))) {
        // Critical condition, should never happen.
        MEMBRANE_DEBUG("Send: enif_send failed, it is likely a bug in the element or the framework");
        enif_free_env(msg_env);
        shout_close(sink_handle->shout);
        enif_release_resource(sink_handle);
        return NULL;
      }
      enif_free_env(msg_env);

      break;
    }

    // Send request for more data
    msg_env = enif_alloc_env();
    if(!enif_send(NULL, sink_handle->self_pid, msg_env, enif_make_atom(msg_env, "membrane_send_demand"))) {
      // Critical condition, should never happen.
      MEMBRANE_DEBUG("Send: demand send failed, it is likely a bug in the element or the framework");
      enif_free_env(msg_env);
      shout_close(sink_handle->shout);
      enif_release_resource(sink_handle);
      return NULL;
    }
    enif_free_env(msg_env);

    // Send
    MEMBRANE_DEBUG("Send: Pushing payload");
    int send_ret = shout_send(sink_handle->shout, item->data, item->size);
    membrane_ringbuffer_item_destroy(item);

    if(send_ret != SHOUTERR_SUCCESS) {
      MEMBRANE_DEBUG("Send: error %s", shout_get_error(sink_handle->shout));

      // Send error to the parent element
      msg_env = enif_alloc_env();
      if(!enif_send(NULL, sink_handle->self_pid, msg_env, enif_make_atom(msg_env, "membrane_send_error"))) {
        // Critical condition, should never happen.
        MEMBRANE_DEBUG("Send: enif_send failed, it is likely a bug in the element or the framework");
        enif_free_env(msg_env);
        shout_close(sink_handle->shout);
        enif_release_resource(sink_handle);
        return NULL;
      }
      enif_free_env(msg_env);

      MEMBRANE_DEBUG("Send: Communication error, exiting loop");
      break;
    }

    // Sleep for necessary amount of time to keep frames in sync with the clock
    MEMBRANE_DEBUG("Send: Synchronizing clock");
    shout_sync(sink_handle->shout);
  }

  MEMBRANE_DEBUG("Send: Stopping");

  // Close shout
  shout_close(sink_handle->shout);

  // Clean ringbuffer
  membrane_ringbuffer_cleanup(sink_handle->ringbuffer);

  // Release the handle so can be garbage collected
  enif_release_resource(sink_handle);

  // Indicate that we are not running
  enif_mutex_lock(sink_handle->lock);
  sink_handle->thread_running = 0;
  enif_mutex_unlock(sink_handle->lock);

  MEMBRANE_DEBUG("Send: Stopped");
  return NULL;
}


/**
 * Creates shout sink.
 *
 * It accepts four arguments:
 *
 * - hostname - string with icecast server address,
 * - port - integer with icecast server port,
 * - password - string with icecast server password,
 * - mount - string with stream mount.
 *
 * So far it accepts only MP3 streams.
 *
 * On success, returns `{:ok, resource}`.
 *
 * On bad arguments passed, returns `{:error, {:args, field, description}}`.
 *
 * On sink initialization error, returns `{:error, {:internal, reason}}`.
 */
static ERL_NIF_TERM export_create(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  shout_t *shout;

  MEMBRANE_UTIL_PARSE_STRING_ARG(0, host, 255);
  MEMBRANE_UTIL_PARSE_UINT_ARG(1, port);
  MEMBRANE_UTIL_PARSE_STRING_ARG(2, password, 255);
  MEMBRANE_UTIL_PARSE_STRING_ARG(3, mount, 255);

  if(!(shout = shout_new())) {
    MEMBRANE_DEBUG("Could not allocate shout_t");
    return 1;
  }

  if(shout_set_host(shout, host) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting hostname: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_host");
  }

  if(shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting protocol: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_protocol");
  }

  if(shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting port: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_port");
  }

  if(shout_set_password(shout, password) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting password: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_password");
  }
  if(shout_set_mount(shout, mount) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting mount: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_mount");
  }

  if(shout_set_user(shout, "source") != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting user: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_user");
  }

  if(shout_set_format(shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
    MEMBRANE_DEBUG("Error setting user: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_format");
  }

  // Create handle for future reference
  SinkHandle *sink_handle = (SinkHandle *) enif_alloc_resource(RES_SINK_HANDLE_TYPE, sizeof(SinkHandle));
  MEMBRANE_DEBUG("Creating SinkHandle %p", sink_handle);
  sink_handle->shout = shout;
  sink_handle->thread_started = 0;
  sink_handle->thread_running = 0;
  sink_handle->lock = enif_mutex_create("lock");
  sink_handle->ringbuffer = membrane_ringbuffer_new(1<<24);
  sink_handle->self_pid = enif_alloc(sizeof(ErlNifPid));
  enif_self(env, sink_handle->self_pid);

  // Store handle as an erlang resource
  ERL_NIF_TERM sink_handle_term = enif_make_resource(env, sink_handle);
  enif_release_resource(sink_handle);

  return membrane_util_make_ok_tuple(env, sink_handle_term);
}


/**
 * Starts sending.
 *
 * Expects 1 argument:
 *
 * - sink resource
 *
 * On success, returns `:ok`.
 *
 * If sending was already started, returns `{:error, :already_started}`.
 *
 * On bad arguments passed, returns `{:error, {:args, field, description}}`.
 *
 * On internal error, returns `{:error, {:internal, reason}}`.
 */
static ERL_NIF_TERM export_start(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);

  enif_mutex_lock(sink_handle->lock);
  if(sink_handle->thread_started == 0 && sink_handle->thread_running == 0) {
    // Start the sending thread
    MEMBRANE_DEBUG("Starting sending thread for SinkHandle %p", (void *) sink_handle);
    sink_handle->thread_started = 1;
    enif_mutex_unlock(sink_handle->lock);

    int error = enif_thread_create("membrane_element_shout_sink", &(sink_handle->thread_id), thread_func, sink_handle, NULL);
    if(error != 0) {
      MEMBRANE_DEBUG("Failed to create thread: %d", error);
      enif_mutex_unlock(sink_handle->lock);
      return membrane_util_make_error_internal(env, "thread_create");
    }

    return membrane_util_make_ok(env);

  } else {
    enif_mutex_unlock(sink_handle->lock);
    return membrane_util_make_error(env, enif_make_atom(env, "already_started"));
  }
}


/**
 * Stops sending.
 *
 * Expects 1 argument:
 *
 * - sink resource
 *
 * On success, returns `:ok`.
 *
 * If sending was not started, returns `{:error, :not_started}`.
 *
 * On bad arguments passed, returns `{:error, {:args, field, description}}`.
 *
 * On internal error, returns `{:error, {:internal, reason}}`.
 */
static ERL_NIF_TERM export_stop(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);

  enif_mutex_lock(sink_handle->lock);
  if(sink_handle->thread_started == 1) {
    // Stop the sending thread
    MEMBRANE_DEBUG("Stopping sending thread for SinkHandle %p", (void *) sink_handle);
    sink_handle->thread_started = 0;
    enif_mutex_unlock(sink_handle->lock);

    MEMBRANE_DEBUG("Waiting for sending thread for SinkHandle %p to stop", (void *) sink_handle);
    void *thread_result;
    enif_thread_join(sink_handle->thread_id, &thread_result);
    MEMBRANE_DEBUG("Sending thread for SinkHandle %p has stopped", (void *) sink_handle);

    return membrane_util_make_ok(env);

  } else {
    enif_mutex_unlock(sink_handle->lock);
    return membrane_util_make_error(env, enif_make_atom(env, "not_started"));
  }
}


/**
 * Writes data to the shout sink.
 *
 * Expects 2 arguments:
 *
 * - handle to the sink,
 * - payload.
 *
 * On success, returns `:ok`.
 *
 * If sending was not started, returns `{:error, :not_started}`.
 *
 * On bad arguments passed, returns `{:error, {:args, field, description}}`.
 *
 * On internal error, returns `{:error, {:write, reason}}`.
 */
static ERL_NIF_TERM export_write(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);
  MEMBRANE_UTIL_PARSE_BINARY_ARG(1, payload);

  MEMBRANE_DEBUG("Writing data to SinkHandle %p", (void *) sink_handle);
  if(membrane_ringbuffer_push_from_binary(sink_handle->ringbuffer, &payload) != 1) {
    return membrane_util_make_error(env, enif_make_atom(env, "overrun"));

  } else {
    MEMBRANE_DEBUG("Wrote data to SinkHandle %p", (void *) sink_handle);
    return membrane_util_make_ok(env);
  }
}


static ErlNifFunc nif_funcs[] =
{
  {"create", 4, export_create, 0},
  {"start", 1, export_start, 0},
  {"stop", 1, export_stop, 0},
  {"write", 2, export_write, 0},
};


ERL_NIF_INIT(Elixir.Membrane.Element.Shout.Sink.Native, nif_funcs, load, NULL, NULL, unload)
