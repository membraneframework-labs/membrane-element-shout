defmodule Membrane.Element.Shout.Sink.Native do
  @moduledoc """
  This module is an interface to native libshout-based sink.
  """

  use Bundlex.Loader, nif: :sink

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
          {:ok, any}
          | {:error, {:args, atom, String.t()}}
          | {:error, {:internal, atom}}
  defnif create(host, port, password, mount)

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
          :ok
          | {:error, :already_started}
          | {:error, {:args, atom, String.t()}}
          | {:error, {:internal, atom}}
  defnif start(native)

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
          :ok
          | {:error, :not_started}
          | {:error, {:args, atom, String.t()}}
          | {:error, {:internal, atom}}
  defnif stop(native)

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
          :ok
          | {:error, {:args, atom, String.t()}}
          | {:error, {:internal, atom}}
  defnif write(native, payload)
end
