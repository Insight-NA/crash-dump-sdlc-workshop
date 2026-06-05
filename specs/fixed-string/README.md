# Fixed-string — Session 07 demo feature

> **Stage:** skeleton only. Session 07 facilitator runs the spec-kit flow live.

## Brief

Provide a stack-allocated, fixed-capacity string type for `engine_demo` debug labels and
small-string scenarios. Interoperates with `eastl::string_view`. Compiles under
`-fno-exceptions -fno-rtti`; never allocates. Capacity is a non-type template parameter.
Constitutional articles 1, 2, 3 (interop boundary with `eastl::string_view`), 6.

## Demo arc

Same five-step spec-kit cadence as `lockless-ring-buffer/`, with a stretch goal extending
the buffer with a configurable backpressure policy bridging both features.
