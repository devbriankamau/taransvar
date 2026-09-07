TaraSec kernel module — research prototype
==========================================

STATUS
------
This directory contains experimental kernel code used to test TaraSec's
cooperative-security and traffic-tagging concepts. It is not presented as a
production-hardened router/firewall module and should not be deployed on a
production router without independent security review, testing and hardening.

Security boundaries
-------------------
* TaraSec does not require application-payload inspection for its cooperative
  security model.
* The packet-interpreter hook is metadata-only. Application payload must not be
  copied or written to the kernel log.
* New packet parsing must validate skb/header bounds and use kernel-safe access
  methods before reading packet data.
* Experimental tagging currently uses TCP metadata and remains subject to
  protocol/interoperability research and independent validation.
* Debugging code that can destabilize the kernel must not be enabled in normal
  operation.

Development
-----------
Build against the running kernel headers with:

    make

The resulting tarakernel.ko is a development artifact. Load it only on a test
machine where a kernel failure or network interruption is acceptable:

    sudo insmod tarakernel.ko
    dmesg
    sudo rmmod tarakernel

Run `make clean` to remove generated build output.

Before production use
---------------------
The kernel path needs a dedicated security review covering at least memory
safety, skb/header validation, concurrency/locking, malformed packets, resource
exhaustion, logging/privacy, fail-open/fail-closed behaviour, interoperability,
and fuzz/stress testing.
