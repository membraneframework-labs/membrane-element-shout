defmodule Membrane.Element.Shout.Sink.Native do
  @moduledoc """
  This module is an interface to native libshout-based sink.
  """

  use Unifex.Loader, nif: :sink
end
