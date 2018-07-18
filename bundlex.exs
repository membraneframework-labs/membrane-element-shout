defmodule Membrane.Element.Shout.BundlexProject do
  use Bundlex.Project

  def project do
    [
      nifs: [
        sink: [
          sources: ["sink.c"],
          deps: [membrane_common_c: [:membrane, :membrane_ringbuffer]],
          pkg_configs: [
            "shout"
          ]
        ]
      ]
    ]
  end
end
