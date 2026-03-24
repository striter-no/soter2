## How to make new devcore module

```sh
cd devcore
mkdir MOD_NAME

cd MOD_NAME
mkdir src # For *.c files
mkdir include/MOD_NAME # For *.h files
```

## Structure in HEADERS

Each header MUST consist of:

1. Include guards as follows:

```c
#ifndef MODULE_FILE_H
#define MODULE_FILE_H

// code here

#endif
```

2. Main structure inside guards

```c

typedef struct {
    // ...
} module_realted_name;

```

3. Methods to create AND destroy a structure (plus run and stop, if not state machine)

```c

int module_system_init(module_realted_name *s, ...);
int module_system_end (module_realted_name *s);

// only for background systems
int module_system_run(module_realted_name *s, ...);
int module_system_stop(module_realted_name *s);
```

4. For state machines methods to serve incoming data needs to exist: `int module_system_serve(module_realted_name *s, T *data, ...)`
5. Any additional methods, if needed.

---
Each module can contain or system or utility functions. In case of system you need to decide, what will it do: listening in background for activity/looping through work iteration or just be state machine (preffered method). In case of background system you need to include `pthread_t daemon` and `atomic_bool is_running`

You need to follow MODUALITY in your code, do not make a god objects, divide responsibilities in different files, and you need to minimilize number of dependency modules for each new module.

---
Tips:

1) Oftenly structures `prot_queue` and `mt_eventsock` used in pair, called `notificated queue` (push(), notify())
