defmodule Membrane.Element.Shout.Sink do
  @moduledoc """
  Sink sending the data to an Icecast/Shoutcast server using `libshout` native library.

  The element has it's own timing, so when used in pipeline all the elements have to work in `pull` mode
  or be in sync with system clock. Otherwise, you risk overflowing buffers.
  """

  alias __MODULE__.Consumer
  alias Membrane.{Buffer, Event}
  alias Membrane.Caps.Audio.MPEG
  use Membrane.Element.Base.Sink
  use Membrane.Log, tags: :membrane_element_shout

  def_options host: [type: :string],
              port: [type: :integer, spec: pos_integer],
              password: [type: :string],
              mount: [type: :string],
              ringbuffer_size: [type: :integer, spec: pos_integer, default: 20]

  def_input_pad :input, caps: MPEG, demand_unit: :buffers

  @impl true
  def handle_init(%__MODULE__{} = options) do
    with {:ok, consumer} <- Consumer.create(options) do
      {:ok, %{consumer: consumer, ringbuffer_size: options.ringbuffer_size}}
    else
      {:error, reason} ->
        {:error, {:consumer, reason}}
    end
  end

  @impl true
  def handle_prepared_to_playing(_ctx, %{ringbuffer_size: buf_size} = state) do
    {{:ok, demand: {:input, buf_size}}, state}
  end

  @impl true
  def handle_event(_pad, %Event.StartOfStream{}, _ctx, %{consumer: consumer} = state) do
    with :ok <- consumer |> Consumer.start() do
      # FIXME wait here for success/failure msg so we have synchronous,
      # immediate response whether playback has started or return some
      # indicator that this action is asynchronous.
      {:ok, state}
    else
      {:error, reason} ->
        {{:error, {:start, reason}}, state}
    end
  end

  def handle_event(_pad, _event, _ctx, state), do: {:ok, state}

  @impl true
  def handle_playing_to_prepared(_ctx, %{consumer: consumer} = state) do
    with :ok <- Consumer.stop(consumer) do
      {:ok, %{state | consumer: nil}}
    else
      {:error, reason} ->
        {{:error, reason}, state}
    end
  end

  @impl true
  def handle_write(:input, %Buffer{payload: payload}, _ctx, %{consumer: consumer} = state) do
    {Consumer.write(payload, consumer), state}
  end

  @impl true
  # Handle request from NIF to get more data
  def handle_other({:native_demand, size}, _ctx, state) do
    {{:ok, demand: {:input, &(&1 + size)}}, state}
  end

  def handle_other({:native_error, reason}, _ctx, state) do
    {{:error, {:native, reason}}, state}
  end
end
