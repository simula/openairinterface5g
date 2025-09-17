# Thread pool

The **thread pool** is a working server, made of a set of worker threads that can be mapped on CPU cores.

All the thread pool functions are thread safe. The functions executed by worker threads are provided by the thread pool client, so the client has to handle the concurrency/parallel execution of his functions.

See header file doxygen comments for detailed description of the API
