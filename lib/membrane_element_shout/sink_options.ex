defmodule Membrane.Element.Shout.Sink.Options do
  @type host_t :: String.t
  @type port_t :: pos_integer
  @type password_t :: String.t
  @type mount_t :: String.t

  @type t :: %Membrane.Element.Shout.Sink.Options{
    host: host_t,
    port: port_t,
    password: password_t,
    mount: mount_t,
  }

  defstruct \
    host: nil,
    port: nil,
    password: nil,
    mount: nil
end
