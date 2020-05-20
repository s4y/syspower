# syspower

Read real-time power consumption from Mac hardware.

## Usage

```sh
make && ./syspower
```

## Example output

```
PSTR (System Total):    13.0979
PCPC (CPU package CPU): 1.125
PCPG (CPU package GPU): 0.046875
PCPT (CPU package Tot): 13.0979
PG0R (GPU 0 rail):      0.0740947

PSTR (System Total):    14.3846
PCPC (CPU package CPU): 0.882812
PCPG (CPU package GPU): 0.015625
PCPT (CPU package Tot): 14.3846
PG0R (GPU 0 rail):      0.0730083

```

…and so on, once per second.
