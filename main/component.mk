#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_SRCDIRS :=  . 
	drivers/source 
	framework/source
	applications/source
	clib/crc16-clang
	clib/bror
	clib/bror-freertos
	clib/lagan-clang
	clib/wifi-esp32
	clib/async-clang
	clib/tz-time
	clib/tzmalloc
	clib/tzlist
	clib/udp-esp32
	clib/clock-esp32
	clib/tzfifo
	clib/utz-clang
	clib/dcom-clang
	clib/tzbox
	clib/knock-clang
	clib/tzaccess-clang
	clib/tziot-clang
COMPONENT_PRIV_INCLUDEDIRS := include
	drivers/include
	framework/include
	applications/include
	clib/crc16-clang
	clib/bror
	clib/bror-freertos
	clib/lagan-clang
	clib/wifi-esp32
	clib/async-clang
	clib/tz-time
	clib/tzmalloc
	clib/tzlist
	clib/pt
	clib/udp-esp32
	clib/tztype
	clib/clock-esp32
	clib/tzfifo
	clib/utz-clang
	clib/dcom-clang
	clib/tzbox
	clib/knock-clang
	clib/tzaccess-clang
	clib/tziot-clang