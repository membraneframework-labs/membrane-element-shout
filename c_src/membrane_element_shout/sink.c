/**
 * Membrane Element: Shout - Erlang native interface to libshout.
 *
 * All Rights Reserved, (c) 2016 Marcin Lewandowski
 */

#include "sink.h"

#define RINGBUFFER_SIZE 8

#define UNUSED(x) (void)(x)


ErlNifResourceType *RES_SINK_HANDLE_TYPE;

void res_sink_handle_destructor(ErlNifEnv *env, void *value) {
  SinkHandle *sink_handle = (SinkHandle *) value;

  if(sink_handle->shout != NULL) {
    MEMBRANE_DEBUG(env,"Destroying Sink %p", sink_handle->shout);
    shout_close((shout_t *) sink_handle->shout);
  }

  // TODO handle case when we get garbage collected while still running

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
  UNUSED(priv_data);
  UNUSED(load_info);
  int flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  RES_SINK_HANDLE_TYPE =
    enif_open_resource_type(env, NULL, "SinkHandle", res_sink_handle_destructor, flags, NULL);

  shout_init();

  return 0;
}


void unload(ErlNifEnv *env, void *priv_data) {
  UNUSED(env);
  UNUSED(priv_data);
  shout_shutdown();
}


#define ENIF_SEND(term, cleanup) \
  msg_env = enif_alloc_env(); \
  if(!enif_send(NULL, sink_handle->self_pid, msg_env, term)) { \
    MEMBRANE_THREADED_WARN("Send: enif_send failed, it is likely a bug in the element or the framework"); \
    enif_free_env(msg_env); \
    cleanup; \
    enif_release_resource(sink_handle); \
    enif_mutex_lock(sink_handle->lock); \
    sink_handle->thread_running = 0; \
    enif_mutex_unlock(sink_handle->lock); \
    return NULL; \
  } \
  enif_free_env(msg_env);


#define ENIF_SEND_DEMAND(size, cleanup) \
  ENIF_SEND(enif_make_tuple2(msg_env, \
    enif_make_atom(msg_env, "native_demand"), \
    enif_make_int(msg_env, size) \
  ), cleanup);

/**
 * Sends message to the element, that will be an atom made of given message.
 *
 * In unlikely case of sending fails, quits the thread executing first given
 * cleanup code.
 */
#define ENIF_SEND_ATOM(message, cleanup) \
  ENIF_SEND(enif_make_atom(msg_env, message), cleanup);

/**
 * Sends message to the element, that will be a 2-element tuple with
 * atom made of given message and shout error string.
 *
 * In unlikely case of sending fails, quits the thread executing first given
 * cleanup code.
 */
#define ENIF_SEND_ATOM_WITH_ERROR(message, cleanup) \
  ENIF_SEND(enif_make_tuple2(msg_env, enif_make_atom(msg_env, message), enif_make_string(msg_env, shout_get_error(sink_handle->shout), ERL_NIF_LATIN1)), cleanup);


/**
 * Main function of the sending thread.
 */
static void *thread_func(void *args) {
  SinkHandle *sink_handle = (SinkHandle *) args;
  ErlNifEnv *msg_env;
  int underrun_cnt = 0;

  // Indicate that we are running
  enif_mutex_lock(sink_handle->lock);
  sink_handle->thread_running = 1;
  enif_mutex_unlock(sink_handle->lock);

  MEMBRANE_THREADED_DEBUG("Send: Starting");

  ENIF_SEND_DEMAND(RINGBUFFER_SIZE, {shout_close(sink_handle->shout);});

  // Keep the handle so it does not get garbage collected
  enif_keep_resource(sink_handle);

  MEMBRANE_THREADED_DEBUG("Connecting to the server");
  if(shout_open(sink_handle->shout) != SHOUTERR_SUCCESS) {
    MEMBRANE_THREADED_WARN("Failed to connect to the server: %s", shout_get_error(sink_handle->shout));
    ENIF_SEND_ATOM_WITH_ERROR("native_cannot_connect", { shout_close(sink_handle->shout); });

    goto thread_cleanup;
  }

  MEMBRANE_THREADED_INFO("Connected to icecast");

  while(1) {
    // Check if we are exiting
    enif_mutex_lock(sink_handle->lock);
    if(sink_handle->thread_started == 0) {
      MEMBRANE_THREADED_DEBUG("Send: Stop requested, exiting loop");
      enif_mutex_unlock(sink_handle->lock);
      break;
    }
    enif_mutex_unlock(sink_handle->lock);

    size_t ready_size = membrane_ringbuffer_get_read_available(sink_handle->ringbuffer);
    MEMBRANE_THREADED_DEBUG("Send: Ready buffers: %zd", ready_size);

    RingBufferItem item;
    size_t read_cnt = membrane_ringbuffer_read(sink_handle->ringbuffer, &item, 1);

    if(read_cnt == 1){
      ENIF_SEND_DEMAND(1, {shout_close(sink_handle->shout);});
      if(underrun_cnt > 0) {
        MEMBRANE_THREADED_WARN("Send: Underrun: %d frames", underrun_cnt);
        underrun_cnt = 0;
      }
      // Send
      MEMBRANE_THREADED_DEBUG("Send: Pushing payload");
      int send_ret = shout_send(sink_handle->shout, item.data, item.size);
      free(item.data);

      if(send_ret != SHOUTERR_SUCCESS) {
        MEMBRANE_THREADED_WARN("Send: error %s", shout_get_error(sink_handle->shout));
        ENIF_SEND_ATOM_WITH_ERROR("native_send_error", { shout_close(sink_handle->shout); });
        break;
      }
    } else {
      underrun_cnt++;
      usleep(24000);
    }

    // Sleep for necessary amount of time to keep frames in sync with the clock
    MEMBRANE_THREADED_DEBUG("Send: Synchronizing clock");
    shout_sync(sink_handle->shout);
  }

thread_cleanup:

  if(underrun_cnt > 0) {
    MEMBRANE_THREADED_WARN("Send: Underrun: %d frames", underrun_cnt);
  }

  MEMBRANE_THREADED_DEBUG("Send: Stopping");

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

  MEMBRANE_THREADED_DEBUG("Send: Stopped");
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
  UNUSED(argc);
  shout_t *shout;

  MEMBRANE_UTIL_PARSE_STRING_ARG(0, host, 255);
  MEMBRANE_UTIL_PARSE_UINT_ARG(1, port);
  MEMBRANE_UTIL_PARSE_STRING_ARG(2, password, 255);
  MEMBRANE_UTIL_PARSE_STRING_ARG(3, mount, 255);

  if(!(shout = shout_new())) {
    MEMBRANE_WARN(env,"Could not allocate shout_t");
    return 1;
  }

  if(shout_set_host(shout, host) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting hostname: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_host");
  }

  if(shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env,"Error setting protocol: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_protocol");
  }

  if(shout_set_port(shout, port) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting port: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_port");
  }

  if(shout_set_password(shout, password) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting password: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_password");
  }
  if(shout_set_mount(shout, mount) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting mount: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_mount");
  }

  if(shout_set_user(shout, "source") != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting user: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_user");
  }

  if(shout_set_format(shout, SHOUT_FORMAT_MP3) != SHOUTERR_SUCCESS) {
    MEMBRANE_WARN(env, "Error setting user: %s", shout_get_error(shout));
    return membrane_util_make_error_internal(env, "shout_set_format");
  }

  // Create handle for future reference
  SinkHandle *sink_handle = (SinkHandle *) enif_alloc_resource(RES_SINK_HANDLE_TYPE, sizeof(SinkHandle));
  MEMBRANE_DEBUG(env, "Creating SinkHandle %p", sink_handle);
  sink_handle->shout = shout;
  sink_handle->thread_started = 0;
  sink_handle->thread_running = 0;
  sink_handle->lock = enif_mutex_create("lock");
  sink_handle->ringbuffer = membrane_ringbuffer_new(RINGBUFFER_SIZE, sizeof(RingBufferItem));
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
  UNUSED(argc);
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);

  enif_mutex_lock(sink_handle->lock);
  if(sink_handle->thread_started == 0 && sink_handle->thread_running == 0) {
    // Start the sending thread
    MEMBRANE_DEBUG(env, "Starting sending thread for SinkHandle %p", (void *) sink_handle);
    sink_handle->thread_started = 1;
    enif_mutex_unlock(sink_handle->lock);

    int error = enif_thread_create("membrane_element_shout_sink", &(sink_handle->thread_id), thread_func, sink_handle, NULL);
    if(error != 0) {
      MEMBRANE_WARN(env, "Failed to create thread: %d", error);
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
  UNUSED(argc);
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);

  enif_mutex_lock(sink_handle->lock);
  if(sink_handle->thread_started == 1) {
    // Stop the sending thread
    MEMBRANE_DEBUG(env, "Stopping sending thread for SinkHandle %p", (void *) sink_handle);
    sink_handle->thread_started = 0;
    enif_mutex_unlock(sink_handle->lock);

    MEMBRANE_DEBUG(env, "Waiting for sending thread for SinkHandle %p to stop", (void *) sink_handle);
    void *thread_result;
    enif_thread_join(sink_handle->thread_id, &thread_result);
    MEMBRANE_DEBUG(env, "Sending thread for SinkHandle %p has stopped", (void *) sink_handle);

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
  UNUSED(argc);
  MEMBRANE_UTIL_PARSE_RESOURCE_ARG(0, sink_handle, SinkHandle, RES_SINK_HANDLE_TYPE);
  MEMBRANE_UTIL_PARSE_BINARY_ARG(1, payload);

  MEMBRANE_DEBUG(env,"Writing data to SinkHandle %p", (void *) sink_handle);
  RingBufferItem item;
  item.size = payload.size;
  item.data = malloc(item.size);
  memcpy(item.data, payload.data, item.size);
  if(!membrane_ringbuffer_write(sink_handle->ringbuffer, &item, 1)) {
    return membrane_util_make_error(env, enif_make_atom(env, "overrun"));
  } else {
    MEMBRANE_DEBUG(env,"Wrote data to SinkHandle %p", (void *) sink_handle);
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


ERL_NIF_INIT(Elixir.Membrane.Element.Shout.Sink.Native.Nif, nif_funcs, load, NULL, NULL, unload)
