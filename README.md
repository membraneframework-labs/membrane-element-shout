# Membrane Multimedia Framework: Shout Element

[![CircleCI](https://circleci.com/gh/membraneframework/membrane-element-shout.svg?style=svg)](https://circleci.com/gh/membraneframework/membrane-element-shout)

## This element is depracated in favour of [Membrane Element Icecast](https://github.com/membraneframework/membrane-element-icecast)

This package provides elements that can be used to send streams to the
[Icecast](http://icecast.org) streaming server.

It is based on [libshout](http://downloads.xiph.org/releases/libshout/).

At the moment it supports only MPEG Audio streams.

# Installation

Add the following line to your `deps` in `mix.exs`.  Run `mix deps.get`.

```elixir
{:membrane_element_shout, github: "membraneframework/membrane-element-shout"}
```

## Copyright and License

Copyright 2019, [Software Mansion](https://swmansion.com/?utm_source=git&utm_medium=readme&utm_campaign=membrane)

[![Software Mansion](https://membraneframework.github.io/static/logo/swm_logo_readme.png)](https://swmansion.com/?utm_source=git&utm_medium=readme&utm_campaign=membrane)

Licensed under the [Apache License, Version 2.0](LICENSE)
