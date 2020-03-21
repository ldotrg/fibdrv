CONFIG_MODULE_SIG=n
TARGET_MODULE := fibdrv
obj-m := $(TARGET_MODULE).o

$(TARGET_MODULE)-objs := fibdrv_main.o \
                         bignum_k/apm.o \
                         bignum_k/bignum.o \
                         bignum_k/format.o \
                         bignum_k/mul.o \
                         bignum_k/sqr.o


ccflags-y := -std=gnu99 -Wno-declaration-after-statement -msse2

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: client.c
	$(CC) -o $@ $^

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

check: all
	$(MAKE) unload
	$(MAKE) load
	echo 2 > /proc/fibonacci/fib_flag 
	sudo ./client > out
	$(MAKE) unload
	@diff -u out scripts/expected.txt && $(call pass)
	@scripts/verify.py

.PHONY : clean plot
plot:
	gnuplot -e 'in="performance.csv";out="fibtime.png";gtitle="Fibonacci Sequence Performance"' plot.gp
