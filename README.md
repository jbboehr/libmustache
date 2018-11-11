# libmustache

[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)](LICENSE.md)
[![Build Status](https://travis-ci.org/jbboehr/libmustache.png?branch=master)](https://travis-ci.org/jbboehr/libmustache)
[![Build status](https://ci.appveyor.com/api/projects/status/1bwyjyo1cel03b2r?svg=true)](https://ci.appveyor.com/project/jbboehr/libmustache)

C++ implementation of [Mustache](https://mustache.github.com/) intended mainly for use as a [PHP extension](https://github.com/jbboehr/php-mustache).


## Installation

#### Linux/OSX

You will need `autoconf`, `automake`, `make` and a C++ compiler.

``` sh
git clone git://github.com/jbboehr/libmustache.git --recursive
cd libmustache
autoreconf -fiv
./configure
make
sudo make install
```

#### Nix/NixOS

``` sh
nix-env -i -f https://github.com/jbboehr/libmustache/archive/master.tar.gz
```


## Credits

- [John Boehr](https://github.com/jbboehr)
- [Adam Baratz](https://github.com/adambaratz)
- [All Contributors](../../contributors)


## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.

