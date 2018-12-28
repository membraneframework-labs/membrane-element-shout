defmodule Membrane.Element.Shout.Sink do
  alias __MODULE__.Native
  alias Membrane.Buffer
  alias Membrane.Caps.Audio.MPEG
  use Membrane.Element.Base.Sink
  use Membrane.Log, tags: :membrane_element_shout

  def_options host: [type: :string],
              port: [type: :integer, spec: pos_integer],
              password: [type: :string],
              mount: [type: :string]

  def_input_pads input: [caps: MPEG, demand_unit: :buffers]

  @impl true
  def handle_init(%__MODULE__{host: host, port: port, mount: mount, password: password}) do
    with {:ok, native} <-
           Native.create(host, port, password, mount) do
      {:ok, %{native: native}}
    else
      {:error, reason} ->
        {:error, {:native, reason}}
    end
  end

  @impl true
  def handle_prepared_to_playing(_ctx, %{native: native} = state) do
    with :ok <- native |> Native.start() do
      # FIXME wait here for success/failure msg so we have synchronous,
      # immediate response whether playback has started or return some
      # indicator that this action is asynchronous.
      {:ok, state}
    else
      {:error, reason} ->
        {{:error, {:start, reason}}, state}
    end
  end

  @impl true
  def handle_playing_to_prepared(_ctx, %{native: native} = state) do
    with :ok <- Native.stop(native) do
      {:ok, %{state | native: nil}}
    else
      {:error, reason} ->
        {{:error, reason}, state}
    end
  end

  @impl true
  def handle_write(:input, %Buffer{payload: payload}, _ctx, %{native: native} = state) do
    {native |> Native.write(payload), state}
  end

  @impl true
  # Handle request from NIF to get more data
  def handle_other({:native_demand, size}, _ctx, state) do
    {{:ok, demand: {:input, &(&1 + size)}}, state}
  end

  def handle_other({:error, reason}, _ctx, state) do
    {{:error, {:native, reason}}, state}
  end

  @impl true
  def handle_shutdown(%{native: nil}), do: :ok
  def handle_shutdown(%{native: native}), do: native |> Native.stop()
end
