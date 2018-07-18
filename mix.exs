defmodule Membrane.Element.Shout.Mixfile do
  use Mix.Project

  def project do
    [
      app: :membrane_element_shout,
      compilers: [:bundlex] ++ Mix.compilers(),
      version: "0.0.1",
      elixir: "~> 1.6",
      elixirc_paths: elixirc_paths(Mix.env()),
      description: "Membrane Multimedia Framework (Shout Element)",
      maintainers: ["Membrane Team"],
      licenses: ["Proprietary"],
      name: "Membrane Element: Shout",
      source_url: "https://github.com/membraneframework/membrane-element-shout",
      preferred_cli_env: [espec: :test],
      deps: deps()
    ]
  end

  def application do
    [extra_applications: [], mod: {Membrane.Element.Shout, []}]
  end

  defp elixirc_paths(:test), do: ["lib", "spec/support"]
  defp elixirc_paths(_), do: ["lib"]

  defp deps do
    [
      {:membrane_core, git: "git@github.com:membraneframework/membrane-core.git"},
      {:membrane_common_c, "~> 0.1"},
      {:membrane_caps_audio_mpeg, "~> 0.1"},
      {:bundlex, "~> 0.1"}
    ]
  end
end
