# Usage

### Downloading the source
```bash
$ git clone https://github.com/jxsl13/zcatch.git zcatch
```

### Building the server
Enter the directory that has been created
```bash
$ cd zcatch
```

Build the release executable
```bash
$ make
or
$ make release_server
```
Build the debugging executable
```bash
$ make debug_server
```

### Starting the server
As there is an example `autoexec.cfg` file in the directory, where the built executable is placed into, we can simply start the server with any of these commands:

##### Easy way
```bash
$ ./zcatch_srv
$ ./zcatch_srv_x86_64
$ ./zcatch_srv_d
$ ./zcatch_srv_x86_64_d
```
The name behind `./` must exactly match the server executable that we built just now.
If you don't know how to see, what kind of file was built, use the unix command `ls`in order to see the current directory's content.

##### Default way
```bash
$ ./zcatch_srv -f autoexec.cfg
or any of the other variants
$ ./zcatch_srv[_x86_64|_srv_d|_x86_64_d] -f autoexec.cfg
```
If your server configuration file has a different name or a different path, you either use the relative path to the executable with `../../srv_1.cfg` or an absolute path like `/home/teeworlds/servers/zcatch/config.cfg`

# License
```
This software called "zCatch" is a modification of "zCatch/TeeVi".

This software called "zCatch/TeeVi" is a modification of "zCatch" which is
a modification of "Teeworlds".
See license.txt.

#############################################################################
The following is the content of the readme.txt distributed with zCatch:
#############################################################################

Copyright (c) 2015 Magnus Auvinen


This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.


Please visit http://www.teeworlds.com for up-to-date information about 
the game, including new versions, custom maps and much more.

##################################################################################
EXTENSION OF THE LICENSE as of the modification done by John Behm, a.k.a. jxsl13:
##################################################################################

0. Any individual that is partaking in multiple teeworlds(vanilla) communities, 
	teeworlds mod communities or other game communities is allowed to use and/or 
	distribute it under the following conditions:

I. Stated individual ...
    1. ...  solely hosts teeworlds vanilla servers and only servers of THIS mod 
    	and/or DERIVED modifications of zCatch or zCatch/Teevi.
    2. ...  does not host any servers of any other games nor teeworlds modification.
    3. ...  is not part of the administration nor partaking in any other supporting 
    	role of any centralized hosting community of multiple modificationss/games
    	(simply playing those mods or partaking in events is not considered as a 
    	supporting role).

II. In contrast to I. those conditions are also alternatively met if...
	1. ...  the individual is either hosting the servers him-/herself or as part of a 
		clan
	2. ...  the hosted servers are neither DDRace, any DDRace based racing modifications
		nor DDNet's zCatch.
	3. ...  the individual or the clan as a whole do not host more than 10 servers in
		total excluding this modification.
	4. ...  the server name is strictly either their nickname, clan name, some made-up
		name, any combination of the previous ones, but in any case a name, that is 
		not associated with any of in I. 3. stated communities.
  

Any modification of this software is to be published under EXACTLY THIS SAME LICENSE and
to be open sourced via any openly accessible version control service like e.g. Github.
Each of the openly accessible modifications of this modification are to be linked in 
this repository's [Issue Tracker](https://github.com/jxsl13/zcatch/issues) by opening an 
issue, stating that something was changed and linking to the modification's openly 
accessible repository via a link.

Any change of this license IS to be propagated to any derived modifications.

Any violation of this license is to be punished by a donation of 500 € multiplied by 
1.05 to the power of X, where X is the count of full years passed since the beginning of
the year 2019, to a charitable organization of my choice.
```

