import errno
import subprocess
try:
	from subprocess import DEVNULL
except ImportError:
	import os
	DEVNULL = open(os.devnull, 'wb')
try:
	FileNotFoundError
except NameError:
	FileNotFoundError = OSError
	# default values = null pointer
	git_hash = "0"
	git_version = "0"

try:
	git_hash = subprocess.check_output(["git", "rev-parse", "--short=16", "HEAD"], stderr=DEVNULL).decode().strip()
	git_hash = '"{}"'.format(git_hash)

	git_version = subprocess.check_output(["git", "describe", "HEAD"], stderr=DEVNULL).decode().strip()
	git_version = '"{}"'.format(git_version)
except FileNotFoundError as e:
	if e.errno != errno.ENOENT:
		raise
except subprocess.CalledProcessError:
	pass
print("""
const char *GIT_SHORTREV_HASH = {};
const char *GIT_VERSION = {};
""".format(git_hash, git_version))
