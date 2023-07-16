Building
--------

To build the tracing library execute the following steps:

```
git clone git@github.com:q713/simbricks.git
git switch log-analysis
git submodule update --init
cd trace
```

Now depending on whether you want to build in Debug or Release mode either execute

```
cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_OTLP=ON -DWITH_EXAMPLES_HTTP=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF -B build/debug -G Ninja .
```

or 

```
cmake -DCMAKE_BUILD_TYPE=Release -DWITH_OTLP=ON -DWITH_EXAMPLES_HTTP=ON -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF -B build/debug -G Ninja .
```
