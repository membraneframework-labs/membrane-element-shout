defmodule Membrane.Element.Shout.Sink do
  alias __MODULE__.Native
  alias Membrane.{Buffer, Event}
  alias Membrane.Caps.Audio.MPEG
  use Membrane.Element.Base.Sink
  use Membrane.Log, tags: :membrane_element_shout

  def_options host: [type: :string],
              port: [type: :integer, spec: pos_integer],
              password: [type: :string],
              mount: [type: :string]

  def_known_sink_pads sink: {:always, {:pull, demand_in: :buffers}, MPEG}

  @impl true
  def handle_init(%__MODULE__{host: host, port: port, mount: mount, password: password}) do
    with {:ok, native} <-
           Native.create(host |> to_charlist, port, password |> to_charlist, mount |> to_charlist) do
      {:ok, %{native: native, native_demand: 0, received_buffers: 0}}
    else
      {:error, reason} ->
        {:error, {:native, reason}}
    end
  end

  @impl true
  def handle_play(%{native: native} = state) do
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
  def handle_prepare(:playing, %{native: native} = state) do
    with :ok <- Native.stop(native) do
      {:ok, %{state | native: nil}}
    else
      {:error, reason} ->
        {{:error, reason}, state}
    end
  end

  def handle_prepare(playback, state), do: super(playback, state)

  @impl true
  def handle_write1(:sink, %Buffer{payload: payload}, _ctx, %{native: native} = state) do
    state = state |> Map.update!(:native_demand, &(&1 - 1))
    state = state |> Map.update!(:received_buffers, &(&1 + 1))
    {native |> Native.write(payload), state}
  end

  @impl true
  def handle_event(:sink, %Event{type: :channel_added}, _, state) do
    info("new channel")
    {:ok, state}
  end

  def handle_event(:sink, %Event{type: :channel_removed}, _, state) do
    info("end of channel")
    {:ok, state}
  end

  def handle_event(pad, event, ctx, state) do
    super(pad, event, ctx, state)
  end

  @impl true
  # Handle request from NIF to get more data
  def handle_other({:native_demand, size}, state) do
    total_size = state.native_demand + size

    discontinuity =
      if total_size >= 4 do
        payload = %{duration: 8 + state.received_buffers}
        [event: {:sink, %Event{type: :discontinuity, payload: payload}}]
      else
        []
      end

    {{:ok, discontinuity ++ [demand: {:sink, size}]}, %{state | native_demand: total_size}}
  end

  @doc false
  # Handle request from NIF that indicates send error
  def handle_other({:native_send_error, reason}, state) do
    {{:error, {:native_send, reason}}, state}
  end

  @doc false
  # Handle request from NIF that indicates that we're not able to connect
  def handle_other({:native_cannot_connect, reason}, state) do
    {{:error, {:native_connect, reason}}, state}
  end

  @doc false
  def handle_terminate(%{native: nil}), do: :ok
  def handle_terminate(%{native: native}), do: native |> Native.stop()
end
