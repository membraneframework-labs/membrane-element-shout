defmodule Membrane.Element.Shout.Mixfile do
  use Mix.Project

  def project do
    [app: :membrane_element_shout,
     compilers: ~w(bundlex.lib) ++ Mix.compilers,
     version: "0.0.1",
     elixir: "~> 1.3",
     elixirc_paths: elixirc_paths(Mix.env),
     description: "Membrane Multimedia Framework (Shout Element)",
     maintainers: ["Marcin Lewandowski"],
     licenses: ["MIT"],
     name: "Membrane Element: Shout",
     source_url: "https://github.com/membraneframework/membrane-element-shout",
     preferred_cli_env: [espec: :test],
     deps: deps()]
  end


  def application do
    [applications: [
      :membrane_core,
    ], mod: {Membrane.Element.Shout, []}]
  end


  defp elixirc_paths(:test), do: ["lib", "spec/support"]
  defp elixirc_paths(_),     do: ["lib",]


  defp deps do
    [
      {:membrane_core, git: "git@github.com:membraneframework/membrane-core.git", branch: "v0.1"},
      # {:membrane_core, path: "/Users/marcin/aktivitis/radiokit/membrane-core", override: true},
      {:membrane_common_c, git: "git@github.com:membraneframework/membrane-common-c.git"},
      {:membrane_caps_audio_mpeg, git: "git@github.com:membraneframework/membrane-caps-audio-mpeg.git"},
      {:bundlex, git: "git@github.com:radiokit/bundlex.git"},
    ]
  end
end
