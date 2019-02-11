defmodule Membrane.Element.Shout.Sink.Consumer do
  @moduledoc """
  This module is a wrapper over native Shout consumer taking care of proper
  resource management.
  """

  use GenServer
  alias Membrane.Element.Shout.Sink

  defmodule Native do
    @moduledoc false
    use Unifex.Loader, nif: :sink
  end

  @type t :: %__MODULE__{guard: pid(), native_ref: reference()}
  defstruct [:guard, :native_ref]

  @spec create(%Sink{}, pid()) :: {:ok, t} | {:error, any()}
  def create(%Sink{} = options, monitored_pid \\ self()) do
    %{host: host, port: port, mount: mount, password: password, ringbuffer_size: buf_size} =
      options

    with {:ok, native_ref} <- Native.create(host, port, password, mount, buf_size),
         {:ok, guard} <- GenServer.start(__MODULE__, {native_ref, monitored_pid}) do
      {:ok, %__MODULE__{guard: guard, native_ref: native_ref}}
    end
  end

  @spec start(t) :: :ok | {:error, any()}
  def start(%__MODULE__{native_ref: ref}) do
    Native.start(ref)
  end

  @spec write(Membrane.Payload.t(), t) :: :ok | {:error, any()}
  def write(data, %__MODULE__{native_ref: ref}) do
    Native.write_data(data, ref)
  end

  @spec stop(t) :: :ok
  def stop(%__MODULE__{guard: guard}) do
    GenServer.cast(guard, :stop)
  end

  @impl true
  def init({native_ref, monitored_pid}) do
    Process.monitor(monitored_pid)
    {:ok, %{native_ref: native_ref}, :hibernate}
  end

  @impl true
  def handle_info({:DOWN, _ref, :process, _object, _reason}, state) do
    {:stop, :normal, state}
  end

  @impl true
  def handle_cast(:stop, state) do
    {:stop, :normal, state}
  end

  @impl true
  def terminate(reason, %{native_ref: native_ref}) do
    Native.stop(native_ref)
    reason
  end
end
