# Source C Files

This file lists all production `.c` source files in the repository, excluding
test files (`src/test/`, `src/core/unittest/`, `src/platform/unittest/`),
generated files (`src/generated/`), and fuzzing files (`src/fuzzing/`).

Total: 100 source `.c` files.

## Binary Entry Points (src/bin/)

src/bin/linux/init.c
src/bin/static/empty.c
src/bin/winkernel/driver.c
src/bin/winkernel/msquicpcw.c
src/bin/winkernel/nmrprovider.c
src/bin/winuser/dllmain.c
src/bin/winuser_fuzz/dllmain.c

## Core QUIC Protocol (src/core/)

src/core/ack_tracker.c
src/core/api.c
src/core/bbr.c
src/core/binding.c
src/core/configuration.c
src/core/congestion_control.c
src/core/connection.c
src/core/connection_pool.c
src/core/crypto.c
src/core/crypto_tls.c
src/core/cubic.c
src/core/datagram.c
src/core/frame.c
src/core/injection.c
src/core/library.c
src/core/listener.c
src/core/lookup.c
src/core/loss_detection.c
src/core/mtu_discovery.c
src/core/operation.c
src/core/packet.c
src/core/packet_builder.c
src/core/packet_space.c
src/core/partition.c
src/core/path.c
src/core/range.c
src/core/recv_buffer.c
src/core/registration.c
src/core/send.c
src/core/send_buffer.c
src/core/sent_packet_metadata.c
src/core/settings.c
src/core/sliding_window_extremum.c
src/core/stream.c
src/core/stream_recv.c
src/core/stream_send.c
src/core/stream_set.c
src/core/timer_wheel.c
src/core/version_neg.c
src/core/worker.c

## Platform Abstraction Layer (src/platform/)

src/platform/cert_capi.c
src/platform/certificates_capi.c
src/platform/certificates_darwin.c
src/platform/certificates_posix.c
src/platform/cgroup.c
src/platform/crypt.c
src/platform/crypt_bcrypt.c
src/platform/crypt_openssl.c
src/platform/datapath_epoll.c
src/platform/datapath_iouring.c
src/platform/datapath_kqueue.c
src/platform/datapath_linux.c
src/platform/datapath_raw.c
src/platform/datapath_raw_dummy.c
src/platform/datapath_raw_linux.c
src/platform/datapath_raw_socket.c
src/platform/datapath_raw_socket_linux.c
src/platform/datapath_raw_socket_win.c
src/platform/datapath_raw_win.c
src/platform/datapath_raw_xdp_linux.c
src/platform/datapath_raw_xdp_linux_kern.c
src/platform/datapath_raw_xdp_win.c
src/platform/datapath_unix.c
src/platform/datapath_win.c
src/platform/datapath_winkernel.c
src/platform/datapath_winuser.c
src/platform/datapath_xplat.c
src/platform/hashtable.c
src/platform/pcp.c
src/platform/platform_posix.c
src/platform/platform_winkernel.c
src/platform/platform_winuser.c
src/platform/platform_worker.c
src/platform/selfsign_capi.c
src/platform/selfsign_openssl.c
src/platform/storage_posix.c
src/platform/storage_winkernel.c
src/platform/storage_winuser.c
src/platform/tls_openssl.c
src/platform/tls_quictls.c
src/platform/tls_schannel.c
src/platform/toeplitz.c

## Performance Tools (src/perf/)

src/perf/bin/histogram/hdr_histogram.c

## ETW Tracing Tools (src/tools/etw/)

src/tools/etw/binding.c
src/tools/etw/cxn.c
src/tools/etw/library.c
src/tools/etw/listener.c
src/tools/etw/main.c
src/tools/etw/session.c
src/tools/etw/stream.c
src/tools/etw/trace.c
src/tools/etw/worker.c

## Sample Application (src/tools/sample/)

src/tools/sample/sample.c
