# libmustache

[![Software License](https://img.shields.io/badge/license-MIT-brightgreen.svg?style=flat)](LICENSE.md)
[![Build Status](https://travis-ci.org/jbboehr/libmustache.png?branch=master)](https://travis-ci.org/jbboehr/libmustache)
[![Build status](https://ci.appveyor.com/api/projects/status/1bwyjyo1cel03b2r?svg=true)](https://ci.appveyor.com/project/jbboehr/libmustache)

C++ implementation of [Mustache](https://mustache.github.com/) intended mainly for use as a [PHP extension](https://github.com/jbboehr/php-mustache).


## Installation

#### Linux

For Ubuntu LTS, the library is available in a [PPA](https://launchpad.net/~jbboehr/+archive/ubuntu/mustache), or via source:

``` sh
sudo apt-get install git-core build-essential autoconf automake
git clone git://github.com/jbboehr/libmustache.git --recursive
cd libmustache
autoreconf -fiv
./configure
make
sudo make install
```

#### OSX

You can install using [Homebrew](http://brew.sh/) via the [PHP brew repository](https://github.com/Homebrew/homebrew-php).

``` sh
brew install libmustache
```


## Credits

- [John Boehr](https://github.com/jbboehr)
- [Adam Baratz](https://github.com/adambaratz)
- [All Contributors](../../contributors)


## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.

