defmodule Membrane.Element.Shout.Sink.Native do
  @moduledoc """
  This module is an interface to native libshout-based sink.
  """

  require Bundlex.Loader
  @on_load :load_nifs

  @doc false
  def load_nifs do
    Bundlex.Loader.load_lib_nif!(:membrane_element_shout, :membrane_element_shout_sink)
  end



  @doc """
  Creates shout sink.

  It accepts four arguments:

  - hostname - string with icecast server address,
  - port - integer with icecast server port,
  - password - string with icecast server password,
  - mount - string with stream mount.

  So far it accepts only MP3 streams.

  On success, returns `{:ok, resource}`.

  On bad arguments passed, returns `{:error, {:args, field, description}}`.

  On sink initialization error, returns `{:error, {:internal, reason}}`.
  """
  @spec create(charlist, pos_integer, charlist, charlist) ::
    {:ok, any} |
    {:error, {:args, atom, String.t}} |
    {:error, {:internal, atom}}
  def create(_host, _port, _password, _mount), do: raise "NIF fail"


  @doc """
  Starts sending.

  Expects 1 argument:

  - sink resource

  On success, returns `:ok`.

  If sending was already started, returns `{:error, :already_started}`.

  On bad arguments passed, returns `{:error, {:args, field, description}}`.

  On internal error, returns `{:error, {:internal, reason}}`.
  """
  @spec start(any) ::
    :ok |
    {:error, :already_started} |
    {:error, {:args, atom, String.t}} |
    {:error, {:internal, atom}}
  def start(_native), do: raise "NIF fail"


  @doc """
  Stops sending.

  Expects 1 argument:

  - sink resource

  On success, returns `:ok`.

  If sending was not started, returns `{:error, :not_started}`.

  On bad arguments passed, returns `{:error, {:args, field, description}}`.

  On internal error, returns `{:error, {:internal, reason}}`.
  """
  @spec stop(any) ::
    :ok |
    {:error, :not_started} |
    {:error, {:args, atom, String.t}} |
    {:error, {:internal, atom}}
  def stop(_native), do: raise "NIF fail"


  @doc """
  Writes data to the shout sink.

  Expects 2 arguments:

  - handle to the sink,
  - payload.

  On success, returns `:ok`.

  On bad arguments passed, returns `{:error, {:args, field, description}}`.

  On internal error, returns `{:error, {:write, reason}}`.
  """
  @spec write(any, bitstring) ::
    :ok |
    {:error, {:args, atom, String.t}} |
    {:error, {:internal, atom}}
  def write(_native, _payload), do: raise "NIF fail"
end
