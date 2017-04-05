###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
#
# author: José Bollo <jose.bollo@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

if (CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)
    message(FATAL_ERROR "Require at least gcc-4.9")
endif(CMAKE_C_COMPILER_VERSION VERSION_LESS 4.9)

INCLUDE(FindPkgConfig)

ADD_SUBDIRECTORY(tests)

ADD_DEFINITIONS(-DBINDING_INSTALL_DIR="${binding_install_dir}")
# Always add INFER_EXTENSION (more details in afb-hreq.c)
ADD_DEFINITIONS(-DINFER_EXTENSION)

CHECK_INCLUDE_FILES(magic.h HAVE_MAGIC_H)
CHECK_LIBRARY_EXISTS(magic magic_load "" HAVE_LIBMAGIC_SO)
IF(HAVE_MAGIC_H)
  IF(HAVE_LIBMAGIC_SO)
    SET(HAVE_LIBMAGIC "1")
  ENDIF(HAVE_LIBMAGIC_SO)
ENDIF(HAVE_MAGIC_H)

IF(NOT HAVE_LIBMAGIC)
  MESSAGE(FATAL_ERROR "\"magic.h\" or \"libmagic.so\" missing.
    Please install the \"file-devel\" or \"libmagic-dev\" package !")
ENDIF(NOT HAVE_LIBMAGIC)
ADD_DEFINITIONS(-DUSE_MAGIC_MIME_TYPE)

PKG_CHECK_MODULES(libsystemd REQUIRED libsystemd>=222)
PKG_CHECK_MODULES(libmicrohttpd REQUIRED libmicrohttpd>=0.9.48)
PKG_CHECK_MODULES(openssl REQUIRED openssl)
PKG_CHECK_MODULES(uuid REQUIRED uuid)

INCLUDE_DIRECTORIES(
	${include_dirs}
	${libsystemd_INCLUDE_DIRS}
	${libmicrohttpd_INCLUDE_DIRS}
	${uuid_INCLUDE_DIRS}
	${openssl_INCLUDE_DIRS}
)

ADD_LIBRARY(afb-lib STATIC
	afb-api-dbus.c
	afb-api-so.c
	afb-api-so-v1.c
	afb-api-so-v2.c
	afb-api-ws.c
	afb-apis.c
	afb-common.c
	afb-config.c
	afb-context.c
	afb-ditf.c
	afb-evt.c
	afb-hook.c
	afb-hreq.c
	afb-hsrv.c
	afb-hswitch.c
	afb-method.c
	afb-msg-json.c
	afb-perm.c
	afb-session.c
	afb-svc.c
	afb-subcall.c
	afb-websock.c
	afb-ws-client.c
	afb-ws-json1.c
	afb-ws.c
	afb-wsj1.c
	afb-xreq.c
	jobs.c
	locale-root.c
	sd-fds.c
	sig-monitor.c
	verbose.c
	websock.c
)

###########################################
# build and install afb-daemon
###########################################
ADD_EXECUTABLE(afb-daemon main.c)
TARGET_LINK_LIBRARIES(afb-daemon
	afb-lib
	${link_libraries}
	${libsystemd_LIBRARIES}
	${libmicrohttpd_LIBRARIES}
	${uuid_LIBRARIES}
	${openssl_LIBRARIES}
	-lmagic
	-ldl
	-lrt
)
INSTALL(TARGETS afb-daemon
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

###########################################
# build and install libafbwsc
###########################################
ADD_LIBRARY(afbwsc SHARED afb-ws.c afb-ws-client.c afb-wsj1.c websock.c)
SET_TARGET_PROPERTIES(afbwsc PROPERTIES
	VERSION ${LIBAFBWSC_VERSION}
	SOVERSION ${LIBAFBWSC_SOVERSION})
TARGET_LINK_LIBRARIES(afbwsc
	${libsystemd_LIBRARIES}
	-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/export-afbwsc.map
	-Wl,--as-needed
	-Wl,--gc-sections
)
INSTALL(TARGETS afbwsc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL(FILES afb-wsj1.h afb-ws-client.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/afb)

###########################################
# build and install afb-client-demo
###########################################
ADD_EXECUTABLE(afb-client-demo afb-client-demo.c)
TARGET_LINK_LIBRARIES(afb-client-demo
	afbwsc
	${link_libraries}
	${libsystemd_LIBRARIES}
)
INSTALL(TARGETS afb-client-demo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})


