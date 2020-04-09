# DEAR

Discrete Events for Adaptive AUTOSAR (DEAR) is a framework that integrates the
reactor model [1] with the communication mechanisms of Adaptive AUTOSAR. This
allows to model deterministic distributed applications in Adaptive AUTOSAR. A
description of the mechanisms used in DEAR can be found in our paper [2].

The DEAR framework only works in conjunction with the Adaptive Platform
Demonstrator (APD). For the framework to work probably, some modifications
to the communication mechanisms of APD are required. Currently we do not
cannot provide those modifications in source code due to legal restrictions.
Please contact us, if you need more information on how to integrate DEAR
with APD.

## Build

DEAR build on top of [reactor-cpp](https://github.com/tud-ccc/reactor-cpp). Download, build and install it first:

```sh
git clone https://github.com/tud-ccc/reactor-cpp.git
mkdir reactor-cpp/build
cd reactor-cpp/build
cmake -DCMAKE_INSTALL_PREFIX=<install-dir> ..
make install
cd ../..
```

Then you can download, build and install the DEAR framework:

```sh
git clone https://github.com/tud-ccc/dear.gitw
mkdir dear/build
cd dear build
cmake -DCMAKE_INSTALL_PREFIX=<install-dir> ..
make install
```

When using cmake from the APD SDK build via yocto, there could be an error stating that cmake does not find the reactor-cpp dependency. In this case, you can tell cmake explicitly where to find reactor-cpp:

```sh
cmake -DCMAKE_INSTALL_PREFIX=<install-dir> -Dreactor-cpp_DIR=<install_dir>/share/reactor-cpp/cmake ..
```

## Publications

- [1] [Reactors: A Deterministic Model for
Composable Reactive Systems](https://people.eecs.berkeley.edu/~marten/pdf/Lohstroh_etAl_CyPhy19.pdf) (CyPhy'19)
- [2] [Achieving Determinism in Adaptive AUTOSAR](https://arxiv.org/pdf/1912.01367) (DATE'20)
