#!/usr/bin/env bash

# CPU isolation
# sudo vim /etc/default/grub
# GRUB_CMDLINE_LINUX_DEFAULT="isolcpus=4,5"
# sudo update-grub
# sudo reboot

# Turn off address space layout randomization 
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

cpuidx_arr=(4 5)
for i in "${cpuidx_arr[@]}"
do
    sudo sh -c "echo performance > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor"
done
# sudo sh -c "echo <cpu bitmask> /proc/irq/<irq number>/smp_affinity"
# Bind all irq to CPU0-3
sudo sh -c "echo f > /proc/irq/default_smp_affinity"
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

echo 0 > /proc/fibonacci/fib_flag
sudo taskset 0x20 ./client