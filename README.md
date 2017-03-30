# Membrane Multimedia Framework: Shout Element

This package provides elements that can be used to send streams to the
[Icecast](http://icecast.org) streaming server.

It is based on [libshout](http://downloads.xiph.org/releases/libshout/).

At the moment it supports only MPEG Audio streams.

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
