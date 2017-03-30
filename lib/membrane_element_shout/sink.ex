defmodule Membrane.Element.Shout.Sink do
  use Membrane.Element.Base.Sink
  use Membrane.Mixins.Log
  alias Membrane.Element.Shout.Sink.Options
  alias Membrane.Caps.Audio.MPEG


  def_known_sink_pads %{
    :sink => {:always, [
      %MPEG{}
    ]}
  }


  # Private API

  @doc false
  def handle_init(%Options{host: host, port: port, mount: mount, password: password}) do
    {:ok, %{
      native: nil,
      host: host,
      port: port,
      mount: mount,
      password: password,
    }}
  end


  @doc false
  def handle_prepare(:stopped, %{host: host, port: port, mount: mount, password: password} = state) do
    # case :gen_tcp.connect(to_char_list(host), port, [mode: :binary, packet: :line, active: false, keepalive: true], connect_timeout) do
    #   {:ok, native} ->
    #     info("Connected to #{host}:#{port}")
    #
    #     credentials = "source:#{password}" |> Base.encode64
    #     info("Authenticating")
    #     case :gen_tcp.send(native, "SOURCE #{mount} HTTP/1.0\r\nAuthorization: Basic #{credentials}\r\nHost: #{host}\r\nUser-Agent: RadioKit Osmosis\r\nContent-Type: audio/mpeg\r\n\r\n") do
    #       :ok ->
    #         case :gen_tcp.recv(native, 0, request_timeout) do
    #           {:ok, "HTTP/1.0 200 OK\r\n"} ->
    #             info("Authenticated")
    #             {:ok, %{state | native: native}}
    #
    #           {:ok, response} ->
    #             warn("Got unexpected response: #{inspect(response)}")
    #             :ok = :gen_tcp.close(native)
    #             {:error, {:response, {:unexpected, response}}, %{state | native: nil}}
    #
    #           {:error, reason} ->
    #             warn("Failed to receive response: #{inspect(reason)}")
    #             :ok = :gen_tcp.close(native)
    #             {:error, {:response, reason}, %{state | native: nil}}
    #         end
    #
    #       {:error, reason} ->
    #         warn("Failed to send request: #{inspect(reason)}")
    #         :ok = :gen_tcp.close(native)
    #         {:error, {:request, reason}, %{state | native: nil}}
    #     end
    #
    #   {:error, reason} ->
    #     warn("Failed to connect to #{host}:#{port}: #{inspect(reason)}")
    #     {:error, {:connect, reason}, %{state | native: nil}}
    # end
  end


  @doc false
  def handle_stop(%{native: nil} = state) do
    {:ok, state}
  end

  def handle_stop(%{native: native} = state) do
    :ok = :gen_tcp.close(native)
    {:ok, %{state | native: nil}}
  end


  @doc false
  def handle_buffer(:sink, _caps, _buffer, %{native: nil} = state) do
    warn("Received buffer while not connected")
    {:ok, state}
  end

  def handle_buffer(:sink, _caps, nil, state) do
    warn("Underrun")
    {:ok, state} # FIXME?
  end

  def handle_buffer(:sink, _caps, %Membrane.Buffer{payload: payload}, %{native: native} = state) do
    samples_per_frame = 1152 # FIXME take this from Membrane.Caps.Audio.MPEG.samples_per_frame/2
    sample_rate = 44100 # FIXME take this from caps

    pre_send_time = Membrane.Time.native_monotonic_time()
    case :gen_tcp.send(native, payload) do
      :ok ->
        # Count frame duration in milliseconds
        frame_duration_ms =
          (samples_per_frame * 1000)
          |> div(sample_rate)

        # Wait until remaining time passes so frames are sent in sync to
        # the clock.
        #
        # Timeouts are not accurate. E.g. frame duration when there are 1152
        # samples per frame (typical scenario in MP3 file) and sample rate is
        # 44100 Hz is 26.122 ms.
        #
        # Moreover, :timer.sleep/1 has only 100Hz resolution, see
        # http://erlang.org/pipermail/erlang-questions/2007-March/025680.html.
        # The more accurate ways could be entering a loop but that eats CPU
        # quite a lot.
        #
        # So we use :timer.sleep/1 to wait for most of the timeout (timeout - 1ms)
        # to avoid eating CPU cycles and loop for the remaining part to be accurate.
        :timer.sleep(frame_duration_ms - 1)

        # Count frame duration in erlang native units
        frame_duration_native =
          System.convert_time_unit(samples_per_frame, :seconds, :native)
          |> div(sample_rate)

        # Determine how much time it already took to send and sleep in erlang
        # native units
        post_send_time = Membrane.Time.native_monotonic_time()
        remaining_timeout_native = frame_duration_native - (post_send_time - pre_send_time)

        # Sleep accurately for the remaining timeout
        sleep_accurate(remaining_timeout_native, post_send_time)

        {:ok, state}

      {:error, reason} ->
        warn("Failed to send buffer: #{inspect(reason)}")
        :ok = :gen_tcp.close(native)
        {:error, {:send, reason}, %{state | native: nil}}
    end
  end


  defp sleep_accurate(duration, _origin) when duration <= 0, do: :ok

  defp sleep_accurate(duration, origin) do
    now = Membrane.Time.native_monotonic_time()
    sleep_accurate(duration - (now - origin), now)
  end
end
