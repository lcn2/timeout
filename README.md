# timeout

run a command and timeout after a period of time


# To install

```sh
make clobber all
sudo make install clobber
```


# To use

```sh
timeout [-h] [-V] [-n] seconds cmd [arg ...]

    -h            print help message and exit
    -V            print version string and exit

    -n            noop - do nothing (def: do the tasks)

    seconds       seconds until timeout (may be a float)
    cmd           command to execute until timeout
   [arg...]       optional args to the command

Exit codes:
    0         all OK
    2         -h and help string printed or -V and version string printed
    3         command line error
 >= 10        internal error

/timeout version: 1.2.1 2025-03-24
```


# Example

```sh
/usr/local/bin/timeout 4.5 /usr/bin/tail -f /var/log/messages
```


# Reporting Security Issues

To report a security issue, please visit "[Reporting Security Issues](https://github.com/lcn2/timeout/security/policy)".
