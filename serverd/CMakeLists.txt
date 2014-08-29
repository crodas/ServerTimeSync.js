project(cronosd)

cmake_minimum_required(VERSION 2.8)

set(uv deps/uv)

set(uvfiles ${uv}/src/fs-poll.c ${uv}/src/heap-inl.h ${uv}/src/inet.c ${uv}/src/queue.h
    ${uv}/src/threadpool.c ${uv}/src/uv-common.c ${uv}/src/uv-common.h ${uv}/src/version.c)
if(UNIX)
    list(APPEND uvfiles
       ${uv}/src/unix/async.c 
       ${uv}/src/unix/atomic-ops.h 
       ${uv}/src/unix/core.c 
       ${uv}/src/unix/dl.c 
       ${uv}/src/unix/fs.c 
       ${uv}/src/unix/getaddrinfo.c 
       ${uv}/src/unix/getnameinfo.c 
       ${uv}/src/unix/internal.h 
       ${uv}/src/unix/loop-watcher.c 
       ${uv}/src/unix/loop.c 
       ${uv}/src/unix/pipe.c 
       ${uv}/src/unix/poll.c 
       ${uv}/src/unix/process.c 
       ${uv}/src/unix/signal.c 
       ${uv}/src/unix/spinlock.h 
       ${uv}/src/unix/stream.c 
       ${uv}/src/unix/tcp.c 
       ${uv}/src/unix/thread.c 
       ${uv}/src/unix/timer.c 
       ${uv}/src/unix/tty.c 
       ${uv}/src/unix/udp.c
    )

    if (APPLE)
        list(APPEND uvfiles
                ${uv}/src/unix/darwin.c
                ${uv}/src/unix/darwin-proctitle.c
                ${uv}/src/unix/fsevents.c
                ${uv}/src/unix/kqueue.c
                ${uv}/src/unix/proctitle.c
        )
    endif()
    if (ANDROID)
        list (APPEND uvfiles ${uv}/src/unix/android-ifaddrs.c
                    ${uv}/src/unix/pthread-fixes.c
        )
    endif()
    if("${CMAKE_SYSTEM}" MATCHES "Linux")
        list(APPEND uvfiles
                ${uv}/src/unix/linux-core.c
                ${uv}/src/unix/linux-inotify.c
                ${uv}/src/unix/linux-syscalls.c
                ${uv}/src/unix/linux-syscalls.h
                ${uv}/src/unix/proctitle.c
        )
    endif()
elseif(WIN32)
    # i dont care!
endif()


include_directories(${uv}/include ${uv}/src)

set(http-parser deps/http-parser)
include_directories(${http-parser})
set(HTTP ${http-parser}/http_parser.c)

find_package(Threads)

add_executable(cronosd ${uvfiles} ${HTTP} main.c httpd.c dict.c dict_types.c)
target_link_libraries (cronosd ${CMAKE_THREAD_LIBS_INIT} dl rt)
