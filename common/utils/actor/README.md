# Overview

This is a simple actor implementation (see https://en.wikipedia.org/wiki/Actor_model).

Actor is implemented as a single thread with single producer/consumer queue as input. You can think of it as a single thread threadpool

If you need concurrency, consider allocating more than one actor.

# Thread safety

There is two ways to ensure thread safety between two actors
 - Use core affinity to set both actor to run on the same core.
 - Use data separation, like in testcase thread_safety_with_actor_specific_data
