defmodule Membrane.Element.Shout.BundlexProject do
  use Bundlex.Project

  def project do
    [
      nifs: [
        sink: [
          sources: ["_generated/sink.c", "sink.c"],
          deps: [unifex: :unifex, membrane_common_c: [:membrane, :membrane_ringbuffer]],
          pkg_configs: [
            "shout"
          ]
        ]
      ]
    ]
  end
end
