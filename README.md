# csound-datacore : Data audification/sonification for Csound on Linux

## Overview
csound-datacore provides a way to read data associated with a process ID as audio.
Only Linux is supported or an OS that supports userspace reading of /proc/.../mem

## Requirements
* Linux
* Cmake >= 2.8.12
* Csound with development headers >= 6.14.0
* Optional: X11 with development libraries

Tested on Linux as of August 2021.


## Installation
Create a build directory at the top of the source tree, execute *cmake ..*, *make* and optionally *make install* as root. If the latter is not used/possible then the resulting libcsxtract library can be used with the *--opcode-lib* flag in Csound.
eg:

	mkdir build && cd build
	cmake ..
	make && sudo make install

Cmake should find Csound and X11 using the modules in the cmake/Modules directory and installation should be as simple as above.

## Examples
Some examples are provided in the examples directory.


## Opcode reference

### ipids[] memps
Obtain a list of process IDs owned by the executing user.

* ipids[] : array of process IDs


### Sname mempsname ipid
Get the process command line or name for a given process ID if available.

* ipid : the process ID

* Sname : the process command line


### ifn mem2tab ipid [, iskipzero=0]
Read the memory associated with a process ID into a new function table which can be
used by loscil and other such opcodes.

* ifn : the function table number created

* ipid : the process ID
* iskipzero : if non-zero, skip empty memory locations, without this the output may contain a lot of silence depending on the process


### aout memson ipid, koffset, kbuffermultiplier [, ibuffersize=441000, iskipzero=0]
Buffered memory reading and direct audio output. The buffer is only refilled when koffset changes.

* aout : the sonified memory data

* ipid : the process ID
* koffset : position to read memory from, normalised to between 0 and 1
* kbuffermultiplier : buffer read size multiplier between 0 and 1
* ibuffersize : the size in samples of the buffer to read memory data into
* iskipzero : if non-zero, skip empty memory locations, without this the output may contain a lot of silence depending on the process