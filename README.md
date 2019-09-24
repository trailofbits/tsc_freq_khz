# A `tsc_freq_khz` Driver for Everyone

This driver exports the [Linux kernel's `tsc_khz` variable](https://github.com/torvalds/linux/blob/4ae004a9bca8bef118c2b4e76ee31c7df4514f18/arch/x86/kernel/tsc.c#L35-L36) via `sysfs` (in `/sys/devices/system/cpu/cpu0/tsc_freq_khz`) and enabled profiling and benchmarking tools to work in virtualized environments. It also makes these tools more accurate on systems where the time stamp counter is independent from clock speed, like newer Intel processors.

Several open source projects (X-Ray, Abseil) already check for this `sysfs` file, but until now it was only available in Google's production kernels.

This driver enables it for everyone.

## The Trouble with Timestamps

The x86 timestamp counter (TSC), despite its flaws, is still a good way to measure event duration for profiling and benchmarking.

Very useful profiling tools (like [LLVM's X-Ray](https://llvm.org/docs/XRay.html)), rely on the timestamp counter for accurate measurements.

To translate TSC tics to seconds, we need to know how fast the TSC is ticking.

Getting this information was impossible in cloud-based or other virtualized environments. The kernel does not export the TSC frequency directly, and the best approximation was the processor's maximum clock speed. 

Using maximum clock speed as an approximation for TSC frequency has two big problems:

* The two values may be different. Relying on maximum clock speed as TSC frequency gives the wrong results.
* The maximum CPU clock speed, accessible via `/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq`, was not available in cloud-based or other virtualized environments. The value is populated by the `cpufreq` driver used for frequency scaling, which is not typically permitted in virtualized environments.

## A Hint from Google

Some [timestamp measuerement code in LLVM's X-Ray](https://github.com/llvm-mirror/compiler-rt/blob/02495e511b3020bf41f7d53d8fcfd184d985c83a/lib/xray/xray_x86_64.cc#L77) refers to a mystery `sysfs` entry named `/sys/devices/system/cpu/cpu0/tsc_freq_khz`, which sounds like it provides *exactly what we need*. Unfortunately there are absolutely *no* references to it in the Linux kernel source code.

More searching reveals the following hint in [the Abseil source code](https://github.com/abseil/abseil-cpp/blob/master/absl/base/internal/sysinfo.cc#L219-L229): 

```
  // Google's production kernel has a patch to export the TSC
  // frequency through sysfs. If the kernel is exporting the TSC
  // frequency use that. There are issues where cpuinfo_max_freq
  // cannot be relied on because the BIOS may be exporting an invalid
  // p-state (on x86) or p-states may be used to put the processor in
  // a new mode (turbo mode). Essentially, those frequencies cannot
  // always be relied upon. The same reasons apply to /proc/cpuinfo as
  // well.
  if (ReadLongFromFile("/sys/devices/system/cpu/cpu0/tsc_freq_khz", &freq)) {
    return freq * 1e3;  // Value is kHz.
  }
```

So the value only exists in Google's production kernel, but it is not upstreamed to the mainline kernel tree.

The Linux kernel conveniently provides a proper exported symbol named `tsc_khz`. So it was only the simple matter of writing a driver to export it via `sysfs`, and everyone could have access to the timestamp counter frequency.

## Technical Details

This module creates a `sysfs` entry that reads the [`tsc_khz` variable defined by the kernel](https://github.com/torvalds/linux/blob/4ae004a9bca8bef118c2b4e76ee31c7df4514f18/arch/x86/kernel/tsc.c#L35-L36). Whether that value makes sense or is otherwise useful depends on the context and underlying hardware.

For example, on x86-based systems, using this tool only makes sense if your CPU supports `rdtscp` and a constant and non-stop TSC. This should be true for all relatively recent processors. You can use the fields in `/proc/cpuinfo` to check, just in case:
```sh
$ egrep -o '(constant_tsc|nonstop_tsc|rdtscp)' /proc/cpuinfo | sort -u
constant_tsc
nonstop_tsc
rdtscp
```

## Building and Usage

First, install the appropriate headers for your kernel:

```
sudo apt-get install linux-headers-$(uname -r)
```

Then, build the module:

```
make
```

This will produce a file named `tsc_freq_khz.ko`. Test it by inserting it into the kernel and looking at the log to see if it works.

```
$ sudo insmod ./tsc_freq_khz.ko
$ dmesg | grep tsc_freq_khz
[14045.345025] tsc_freq_khz: starting driver
[14045.345026] tsc_freq_khz: registering with sysfs
[14045.345028] tsc_freq_khz: successfully registered
```

The entry at `/sys/devices/system/cpu/cpu0/tsc_freq_khz` should now also be populated (the values on your system will be different):

```
$ cat /sys/devices/system/cpu/cpu0/tsc_freq_khz
2712020
```

## Future Work

This module needs sanity checks to ensure that this driver only loads on hardware with the appropriate TSC-related features.