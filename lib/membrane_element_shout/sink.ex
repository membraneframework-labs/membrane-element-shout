defmodule Membrane.Element.Shout.Sink do
  use Membrane.Element.Base.Sink
  use Membrane.Mixins.Log
  alias Membrane.Element.Shout.Sink.Options
  alias Membrane.Element.Shout.Sink.Native
  alias Membrane.Caps.Audio.MPEG


  def_known_sink_pads %{
    :sink => {:always, [
      %MPEG{}
    ]}
  }


  # FIXME debug code
  @test_payload <<255, 251, 112, 196, 84, 128, 29, 161, 125, 59, 249, 234, 0, 1, 59,
    134, 163, 31, 176, 96, 0, 102, 95, 55, 34, 132, 80, 168, 98, 93, 38, 76,
    139, 197, 228, 81, 71, 255, 147, 229, 243, 114, 112, 184, 104, 95, 77, 52,
    62, 146, 75, 69, 21, 37, 255, 255, 65, 147, 77, 208, 65, 4, 211, 116, 16,
    50, 18, 132, 129, 174, 91, 255, 240, 31, 12, 3, 224, 64, 192, 62, 127, 194,
    160, 168, 136, 42, 10, 245, 157, 0, 72, 18, 164, 155, 36, 113, 140, 198,
    113, 36, 147, 49, 69, 86, 130, 141, 198, 16, 47, 85, 164, 145, 76, 26, 157,
    217, 107, 176, 237, 227, 65, 64, 32, 20, 88, 149, 60, 26, 14, 193, 163,
    185, 80, 216, 148, 52, 160, 105, 64, 211, 202, 157, 42, 26, 135, 97, 222,
    87, 242, 199, 184, 43, 5, 101, 143, 22, 124, 151, 255, 219, 193, 168, 138,
    179, 171, 193, 168, 53, 76, 65, 77, 69, 51, 46, 57, 57, 46, 53, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
    85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85>>


  # Private API

  @doc false
  def handle_init(%Options{host: host, port: port, mount: mount, password: password}) do
    case Native.create(host |> to_charlist, port, password |> to_charlist, mount |> to_charlist) do
      {:ok, native} ->
        {:ok, %{
          native: native,
        }}

      {:error, reason} ->
        {:error, {:native, reason}}
    end
  end


  @doc false
  def handle_play(%{native: native} = state) do
    # FIXME debug code
    Enum.each(1..100, fn(_) ->
      Native.write(native, @test_payload)
    end)

    case Native.start(native) do
      :ok ->
        # FIXME wait here for success/failure msg so we have synchronous,
        # immediate response whether playback has started or return some
        # indicator that this action is asynchronous.
        {:ok, state}

      {:error, reason} ->
        {:error, {:start, reason}, state}
    end
  end


  @doc false
  def handle_stop(%{native: native} = state) do
    case Native.stop(native) do
      :ok ->
        {:ok, %{state | native: nil}}

      {:error, reason} ->
        # That should really not happen, it is not recoverable error, it
        # rather indicates bug in the C code, so throw instead of returning
        # {:error, reason, new_state}.
        throw reason
    end
  end


  @doc false
  def handle_write(:sink, _caps, %Membrane.Buffer{payload: payload}, %{native: native} = state) do
    case Native.write(native, payload) do
      :ok ->
        {:ok, :enough, state}

      {:error, :overrun} ->
        # This actually should have not happen. We explicitely ask about a
        # buffer after one was already consumed, so it should never reach the
        # stage where we write buffers without prior request.
        warn("Overrun")

        case Native.stop(native) do
          :ok ->
            {:error, :overrun, %{state | native: nil}}

          {:error, reason} ->
            # That should really not happen, it is not recoverable error, it
            # rather indicates bug in the C code, so throw instead of returning
            # {:error, reason, new_state}.
            throw reason
        end
    end
  end


  @doc false
  # Handle request from NIF to get more data
  def handle_other(:membrane_send_demand, %{native: native} = state) do
    debug("Demand")

    # FIXME debug code
    Native.write(native, @test_payload)

    {:ok, state}
  end


  @doc false
  # Handle request from NIF that indicates send error
  def handle_other(:membrane_send_error, %{native: native} = state) do
    warn("Send error")
    case Native.stop(native) do
      :ok ->
        {:error, :send, %{state | native: nil}}

      {:error, reason} ->
        # That should really not happen, it is not recoverable error, it
        # rather indicates bug in the C code, so throw instead of returning
        # {:error, reason, new_state}.
        throw reason
    end
  end


  @doc false
  # Handle request from NIF that indicates underrun
  def handle_other(:membrane_send_underrun, %{native: native} = state) do
    warn("Underrun")
    case Native.stop(native) do
      :ok ->
        {:error, :underrun, %{state | native: nil}}

      {:error, reason} ->
        # That should really not happen, it is not recoverable error, it
        # rather indicates bug in the C code, so throw instead of returning
        # {:error, reason, new_state}.
        throw reason
    end
  end


  @doc false
  # Handle request from NIF that indicates that we're not able to connect
  def handle_other(:membrane_send_cannot_connect, %{native: native} = state) do
    warn("Connection error")
    case Native.stop(native) do
      :ok ->
        {:error, :cannot_connect, %{state | native: nil}}

      {:error, reason} ->
        # That should really not happen, it is not recoverable error, it
        # rather indicates bug in the C code, so throw instead of returning
        # {:error, reason, new_state}.
        throw reason
    end
  end
end
