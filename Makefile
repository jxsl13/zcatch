#!/bin/bash

# This file is used on unix (linux + macOS) 
# systems to easily build the executable 
# for the server

BAM_DIR=bam
CORES=2

default: release_server
clean: clean_build

update_release_server: backup_autoexec update_repository restore_autoexec release_server
	@echo "Done updating the release server executable!"
	
restore_autoexec:
	@echo "Restoring autoexec.cfg"
	rm autoexec.cfg
	mv autoexec.cfg.backup autoexec.cfg

update_repository:
	git checkout autoexec.cfg
	git pull

backup_autoexec:
	@echo "Backing up autoexec.cfg"
	cp autoexec.cfg autoexec.cfg.backup


release_server: build_bam
	@echo "Building normal server executable..."
	../${BAM_DIR}/bam -j ${CORES} -a server_release


debug_server: build_bam
	@echo "Building debug server executable..."
	@../${BAM_DIR}/bam -j ${CORES} -a server_debug


build_bam: download_bam
	@echo "Building bam..." 
	@cd ../${BAM_DIR} && \
	chmod +x make_unix.sh && \
	./make_unix.sh && \
	chmod +x bam 


download_bam:
	# if this fails, we wanna continue anyway, 
	# as it seems that bam has already been downloaded
	# failures are ignored by a - in front of the line
	@echo "Downloading bam..."
	-cd .. && \
	git clone https://github.com/matricks/bam.git ${BAM_DIR}


clean_all: clean_build
	@echo "Removing bam directory..."
	cd .. && \
	rm -rf ${BAM_DIR}


clean_build:
	@echo "Cleaning object files..."
	../${BAM_DIR}/bam -c all






