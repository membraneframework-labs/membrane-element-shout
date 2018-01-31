defmodule Membrane.Element.Shout.Sink do
  use Membrane.Element.Base.Sink
  use Membrane.Mixins.Log, tags: :membrane_element_shout
  alias Membrane.Element.Shout.Sink.Options
  alias Membrane.Element.Shout.Sink.Native
  # alias Membrane.Caps.Audio.MPEG


  def_known_sink_pads %{
    :sink => {:always, {:pull, demand_in: :buffers}, :any}
  }

  # Private API

  @doc false
  def handle_init(%Options{host: host, port: port, mount: mount, password: password}) do
    with {:ok, native} <-
      Native.create(host |> to_charlist, port, password |> to_charlist, mount |> to_charlist)
    do
      {:ok, %{native: native}}
    else
      {:error, reason} ->
        {:error, {:native, reason}}
    end
  end


  @doc false
  def handle_play(%{native: native} = state) do
    with :ok <- native |> Native.start do
      # FIXME wait here for success/failure msg so we have synchronous,
      # immediate response whether playback has started or return some
      # indicator that this action is asynchronous.
      {:ok, state}
    else
      {:error, reason} ->
        {{:error, {:start, reason}}, state}
    end
  end

  @doc false
  def handle_prepare(:playing, %{native: native} = state) do
    with :ok <- Native.stop(native) do
      {:ok, %{state | native: nil}}
    else
      {:error, reason} ->
        {{:error, reason}, state}
    end
  end
  def handle_prepare(playback, state), do: super(playback, state)


  @doc false
  def handle_write1(:sink, %Membrane.Buffer{payload: payload}, _ctx, %{native: native} = state) do
    {native |> Native.write(payload), state}
  end

  def handle_event(:sink, %Membrane.Event{type: :channel_added}, _, state) do
    info "new channel"
    {:ok, state}
  end
  def handle_event(:sink, %Membrane.Event{type: :channel_removed}, _, state) do
    info "end of channel"
    {:ok, state}
  end
  def handle_event(pad, event, ctx, state) do
    super(pad, event, ctx, state)
  end

  @doc false
  # Handle request from NIF to get more data
  def handle_other({:native_demand, size}, state) do
    {{:ok, demand: {:sink, size}}, state}
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
  def handle_terminate(%{native: native}), do: native |> Native.stop

end
