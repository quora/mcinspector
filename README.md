# MCInspector - Memcache Objects Inspector

[![Hex.pm](https://img.shields.io/hexpm/l/plug.svg)](LICENSE)

## Introduction

MCInspector is a lightweight tool to get a summary of objects or dump the keys from a running Memcache server process without interrupting it.


## Requirements and building

The tool is designed to work on Linux only. The *no_inline_ascii_resp* option should not be specified on the Memcached process. It requires the kernel > 3.2 and the glibc > 2.15 to compile.


```text
$ git clone https://github.com/quora/mcinspector.git
$ cd mcinspector
$ make
```

## Usage
This tool has been tested on **Memcached-1.4.14**, **Memcached-1.4.22** and **Memcached-1.4.36**. It also works on **Memcached-1.5.2** but only if the Memcached is started with *-o inline_ascii_resp* option.

Below examples can be done in one pass by using all processors together.  The examples also assume it uses the default ':' char as keyspace delimiter. Full list of arguments can be seen by running the binaries with no argument.

### Get objects summary
```text
$ printf "stats\nstats slabs\nstats items\nstats settings\n" | \
      netcat 127.0.0.1 11211 > /tmp/mc_stat_file
$ sudo ./mcinspector --stats-file=/tmp/mc_stat_file --processor=item-aggregator
```

### Clean expired objects
Though recent Memcached versions have built-in feature of cleaning up expired objects, this is an alternative way and can be useful if you are running an old version of Memcached.
```text
$ printf "stats\nstats slabs\nstats items\nstats settings\n" | \
       netcat 127.0.0.1 11211 > /tmp/mc_stat_file
$ sudo ./mcinspector \
      --stats-file=/tmp/mc_stat_file \
      --processor=expired-dumper \
      --expired-dump-file=/tmp/mc_expired_list
$ ./mccleaner \
      --expired-keys-file=/tmp/mc_expired_list \
      --mc-port=11211 \
      --clean-batch=200 \
      --sleep-interval=10
```
### Dump all keys in a category
The example is to dump keys in 'user_info' category which has size less than 200 bytes.
```text
$ printf "stats\nstats slabs\nstats items\nstats settings\n" | \
       netcat 127.0.0.1 11211 > /tmp/mc_stat_file
$ sudo ./mcinspector \
      --stats-file=/tmp/mc_stat_file \
      --processor=item-dumper \
      --category-to-dump=user_info \
      --category-dump-file=./keylist_of_user_info.txt \
      --dump-size-max=200
```


## Performance
In our production machines, which run on Intel Xeon E5-2670 CPUs, it is able to detect 130 million objects from a 23GB Memcached process in 85 seconds using a single core. The number of detected keys is about 99.9% of the number shown in Memcached's 'STATS' output.


## License
MCInspector is released under the [Apache 2.0 Licence](https://github.com/quora/mcinspector/blob/master/LICENSE).
