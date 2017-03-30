# Membrane Multimedia Framework: Shout Element

This package provides elements that can be used to send streams to the
[Shout](http://shout.org) streaming server.

At the moment it supports only MPEG Audio stream.

# Installation

Add the following line to your `deps` in `mix.exs`.  Run `mix deps.get`.

```elixir
{:membrane_element_shout, git: "git@github.com:membraneframework/membrane-element-shout.git"}
```

Then add the following line to your `applications` in `mix.exs`.

```elixir
:membrane_element_shout
```

# Authors

Marcin Lewandowski
