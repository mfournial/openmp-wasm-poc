# Clang OpenMP output to WASM Proof of Concept

This repo is a simple proof of concept that is it possible to take OpenMP code,
and have a compiler doing the OpenMP to C translation according to the
OpenMP specifications. And then compile the resulting code to Wasm without
having to compile the actual `libomp` library to Wasm as well.

## Requirements

* LLVM >8 (I'm running on 9 as you'll see later). My LLVM install is using the
Ubuntu nightly repo (if you have Ubuntu and frequently find yourself downloading
specific LLVM versions, this is **great**). But that means I've had to create
the first simlink of the output below (`clang-9` doesn't prepend `-9` to linker
invocation).
```bash
ls -l /usr/bin/wasm-*
>  /usr/bin/wasm-ld -> wasm-ld-9
>  /usr/bin/wasm-ld-9 -> ../lib/llvm-9/bin/wasm-ld
```
* emscripten
* wasi-sdk:
  * wasi-sdk called sysroot by clang
  * libclang rt: put it in the right place, for Ubuntu nightly:
```bash
sudo cp lib/wasi/libclang_rt.builtins-wasm32.a <path/to/llvm-9>/lib/clang/9.0.0/lib/wasi/libclang_rt.builtins-wasm32.a
```
* WABT: Wasm tooling (e.g. `wasm2wat`)
* WAVM, Wasmer (and Wasmtime)

## POC

If you check the [outputs](outputs) folder, you'll see the resulting (faulty
but existent Wasm modules) from the procedure described below.

### Steps taken

We will be trying tp compile this [minimum OpenMP example](easy.c)

#### Compiling OpenMP program to Wasm object files

Of course compiling straight away to Wasm fails because the linker cannot find
the `libomp` runtime symbols.
```bash
SYSROOT=<path/to/wasi-sdk>
clang-9 --target=wasm32-wasi --sysroot=$SYSROOT -O2 -o easy.wasm -fopenmp easy.cpp
> wasm-ld: error: /tmp/easy-288a60.o: undefined symbol: __kmpc_fork_call
```
So let's instead compile to an object file and see exactly what is going on:
```bash
clang-9 --target=wasm32-unknwon-wasi --sysroot=$SYSROOT -O2 -fopenmp -c easy.cpp

llvm-nm-9 easy.o
> 00000000 d .L.str
> 00000018 d .L__unnamed_1
> 00000052 t .omp_outlined.
>             U __kmpc_fork_call
> 00000001 T __original_main
>             U __stack_pointer
> 00000062 T main
```

#### Compiling a fake library to Wasm object files

This is really the hello world of OpenMP it seems, there is only one function
call to provide (`__kmpc_fork_call`). Let's see what this function is in the
LLVM codebase:
```bash
llvm-project/openmp > git grep -n "void.*kmpc_fork_call"                                                                                                                    
kmp.h:3614:KMP_EXPORT void __kmpc_fork_call(ident_t *, kmp_int32 nargs,
kmp_csupport.cpp:265:void __kmpc_fork_call(ident_t *loc, kmp_int32 argc, kmpc_micro microtask, ...) {
```
_Umm_ this is a variadic function with many complex types. I renounced to import
the header with the proper typedefs in the mini-library for now. It was
importing too many things that would have cluttered the output anyway.  
Instead, I checked the linker errors to so how many arguments the variadic fork
call was taking and which types they were. It gave me something of the form
`fork(i32, i32, i32, i32)` so I just went for 4 ints like you can see in the 
[mini-runtime.c](mini-runtime.c) file and compiled the object file:

```bash
clang-9 --target=wasm32-unknwon-wasi --sysroot=$SYSROOT -O2 -c mini-runtime.c
```

#### Linked everything together

```bash
wasm-ld-9 easy.o mini-runtime.o --no-entry -o test.wasmr
```

Which produces
```bash
wasm2wat test.wasm
> (module
>   (table (;0;) 1 1 funcref)
>   (memory (;0;) 2)
>   (global (;0;) (mut i32) (i32.const 66560))
>   (export "memory" (memory 0)))
```

To get an emscriptem version, which unlike the first one will run to a segfa: 
```bash
emcc easy.o mini-runtime.o -s EMIT_EMSCRIPTEN_METADATA=1 -o viaem.wasm
```

is not "correct" in terms of OpenMP but valid Wasm output (passes the Wasm
validation) which was the simple goal here! The wasm files are included
[in this repo](outputs) if you can't reproduce the steps above but want to look
at how they act on WAVM, Wastime etc. it's interesting for the least.

I'll need to look at why the Wasm code is being trapped.
